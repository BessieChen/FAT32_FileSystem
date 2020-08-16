#include<stdio.h>
#include "xdisk.h"
#include "xfat.h" //包括了xtypes.h等
#include <string.h> //包括了memset(), memcmp()
#include <stdlib.h> //3.3 包括了一些malloc()

//1.2 测试: 将虚拟磁盘vdisk的实例用extern引用过来,
extern xdisk_driver_t vdisk_driver;

//2.2 定义一个测试磁盘的路径
const char* disk_path = "disk.img";

//2.2 定义一个disk
xdisk_t disk;
//3.1 定义一个disk part存分区的信息
xdisk_part_t  disk_part;
//3.1 定义buffer
static u32_t read_buffer[160 * 1024]; //可以存160*1024个字节的东西, 相当于160MB
//3.2 定义xfat, 这里存储了fat表的关键信息
xfat_t xfat;

//2.4 修改: 获取每个分区的信息 //2.2 定义检测的函数
int disk_part_test(void) {
	u32_t count, i;
	xfat_err_t err;

	printf("partition read test...\n");

	//调用计算分区数量的函数
	err = xdisk_get_part_count(&disk, &count);
	if (err < 0) {
		printf("partition count detect failed.\n");
		return err;
	}

	//2.4 把所有的分区信息都拿出来
	for (i = 0; i < count; i++) {
		xdisk_part_t part;

		int err;

		//开始将每一个分区(包括小碎片)的信息存到part中
		err = xdisk_get_part(&disk, &part, i);//说明我们要第i个分区
		if (err < 0) {
			printf("read partition failed.\n");
			return -1;
		}

		printf("no: %d, start sector: %d, total sector: %d, capcity:%.0f M\n",
			i, part.start_sector, part.total_sector, part.total_sector * disk.sector_size / 1024 / 1024.0);

	}

	printf("partition count:%d\n", count);
	return 0;
}

//3.3 打印目录项的详细信息
void show_dir_info(diritem_t* diritem) {
	
	char file_name[12];//拷贝文件名的缓存
	memset(file_name, 0, sizeof(file_name)); //将file_name都设置成0
	memcpy(file_name, diritem->DIR_Name, 11); //从目录项diritem中拷贝文件名, 文件名是11字节
	
	//判断,文件名的的第0个字节
	if (file_name[0] == 0x05) {
		file_name[0] = 0xE5; //文档上说的, 如果是05, 需要改成E5
	}

	//打印文件名
	printf("\n name: %s", file_name);
	printf("\n\t");

	//打印属性
	u8_t attr = diritem->DIR_Attr;
	if (attr & DIRITEM_ATTR_READ_ONLY)//如果是只读的
	{
		printf("read only, ");
	}

	if (attr & DIRITEM_ATTR_HIDDEN) //如果是隐藏的
	{
		printf("hidden, ");
	}

	if (attr & DIRITEM_ATTR_SYSTEM) //如果是系统文件
	{
		printf("system, ");
	}

	if (attr & DIRITEM_ATTR_DIRECTORY) //如果是目录
	{
		printf("direcotry, ");
	}

	if (attr & DIRITEM_ATTR_ARCHIVE) //如果是归档
	{
		printf("archive.");
	}
	printf("\n\t");

	//打印: 创建日期, Crt:create
	printf("create date: %d-%d-%d\n\t", diritem->DIR_CrtDate.year_from_1980 + 1980,
		diritem->DIR_CrtDate.month, diritem->DIR_CrtDate.day);
	//打印: 创建时间
	printf("create time: %d-%d-%d\n\t", diritem->DIR_CrtTime.hour, diritem->DIR_CrtTime.minute,
		diritem->DIR_CrtTime.second_2 * 2 + diritem->DIR_CrtTimeTeenth / 100); //最后这个是毫秒的意思:DIR_CrtTimeTeenth, 然后我们的秒是偶数秒

	//打印: 最后修改日期, Wrt:write
	printf("last write date: %d-%d-%d\n\t", diritem->DIR_WrtDate.year_from_1980 + 1980,
		diritem->DIR_WrtDate.month, diritem->DIR_WrtDate.day);
	//打印: 最后修改时间
	printf("last write time: %d-%d-%d\n\t", diritem->DIR_WrtTime.hour, diritem->DIR_WrtTime.minute,
		diritem->DIR_WrtTime.second_2 * 2);


	//打印: 最后访问日期, Wrt:write
	printf("last access date: %d-%d-%d\n\t", diritem->DIR_LastAccDate.year_from_1980 + 1980,
		diritem->DIR_LastAccDate.month, diritem->DIR_LastAccDate.day);

	//打印: 文件大小
	printf("size %d KB\n\t", diritem->DIR_FileSize / 1024);

	//这个目录项, 相对于数据区起始位置的簇号
	printf("cluster %d\n\t", diritem->DIR_FstClusHI << 16 | diritem->DIR_FstClusL0); //将低16位和高16位组合

	printf("\n");
}

//3.3 添加fat目录的测试
int fat_dir_test(void) {
	u32_t j;
	xfat_err_t err;
	diritem_t* dir_item; //目录项. 用于后面将cluster_buffer强制转化成这个结构体

	//分配一个缓存, 大小是一个簇的大小(一个簇拥有多少字节, 是512的偶数倍, 例如512, 1024, 2048..) //注意这里没有用全局变量read_buffer
	u8_t* cluster_buffer;
	cluster_buffer = (u8_t*)malloc(xfat.cluster_byte_size); //这里的xfat是本.c文件的全局变量

	//定义一个当前的簇
	u32_t curr_cluster;
	curr_cluster = xfat.root_cluster; //初始值: 数据区的根目录的第一个簇 //这里的xfat是本.c文件的全局变量

	//第几个目录项
	int index = 0;
	
	//如果当前的簇号是有效的, 那么久读取这个簇号, 解析里面的信息
	while (is_cluster_valid(curr_cluster)) {
		//读取这个簇
		err = read_cluster(&xfat, cluster_buffer, curr_cluster, 1); //这里的xfat是本.c文件的全局变量
		if (err) {
			printf("read cluster %d failed.\n", curr_cluster);
			return -1;
		}

		//查看簇里面的内容: 目录项(结构体是diritem_t)
		//目录项的个数: xfat.cluster_byte_size / sizeof(diritem_t), 也就是一个簇的字节数 / 目录项的字节数 == 目录项个数
		dir_item = (diritem_t*)cluster_buffer;
		for (j = 0; j < xfat.cluster_byte_size / sizeof(diritem_t); j++) {
			//获取文件名
			u8_t* name = (u8_t*)(dir_item[j].DIR_Name);//我喜欢这个写法, 也就是将指针当做数组来用dir_item[j]

													   //文件名的第一个字符(第一个字符DIR_Name,表示: 有效, 空闲, 或者结束), 如果是0xE5 就是空. 如果是0x00,就是结束
			if (name[0] == DIRITEM_NAME_FREE) //这里是0xE5
			{
				continue;//空闲的话, 看下一个目录项
			}
			else if (name[0] == DIRITEM_NAME_END) //这里是0x00
			{
				break; //结束的话, 跳出
			}

			//走到这里: 说明是有效
			printf("No: %d, ", ++index);//第几个目录项

										//打印目录项的详细信息
			show_dir_info(&dir_item[j]);
		}

		//3.4 获取下一个簇号
		err = get_next_cluster(&xfat, curr_cluster, &curr_cluster); //第二个参数, 当前簇号, 第三个参数: 将下一个簇号赋给这个地址
		if (err)
		{
			printf("get next cluster failed! current cluster No: %d\n", curr_cluster);
			return -1;
		}
	}
	return 0;
}

//3.5 打印文件信息
int fat_file_test(void) {
	xfat_err_t err;

	//分配一个缓存, 大小是一个簇的大小(一个簇拥有多少字节, 是512的偶数倍, 例如512, 1024, 2048..) //注意这里没有用全局变量read_buffer
	u8_t* cluster_buffer;
	cluster_buffer = (u8_t*)malloc(xfat.cluster_byte_size + 1); //3.5 修改, 因为后面我们给buffer加上了一个自己的结束符 //这里的xfat是本.c文件的全局变量

	//定义一个当前的簇
	u32_t curr_cluster;
	curr_cluster = 4565; //3.5 老师这里选取的是64.txt这个文件, 这个文件在目录项中显示的起始簇号就时4565 //curr_cluster = xfat.root_cluster; //初始值: 数据区的根目录的第一个簇 //这里的xfat是本.c文件的全局变量

	//3.5 计算占用的空间(这个将会是簇占有的字节数的倍数)
	int size = 0;

	//如果当前的簇号是有效的, 那么久读取这个簇号, 解析里面的信息
	while (is_cluster_valid(curr_cluster)) {
		//读取这个簇
		err = read_cluster(&xfat, cluster_buffer, curr_cluster, 1); //这里的xfat是本.c文件的全局变量
		if (err) {
			printf("read cluster %d failed.\n", curr_cluster);
			return -1;
		}

		//3.5 打印buffer里面的信息
		cluster_buffer[xfat.cluster_byte_size + 1] = '\0'; //在最后加上结束符
		printf("%s", (char*)cluster_buffer);

		//3.5 计算这个文件占有的字节. 因为整个文件会分散到不同的簇上, 所以size是簇的大小的倍数
		size += xfat.cluster_byte_size;

		//3.4 获取下一个簇号
		err = get_next_cluster(&xfat, curr_cluster, &curr_cluster); //第二个参数, 当前簇号, 第三个参数: 将下一个簇号赋给这个地址
		if (err)
		{
			printf("get next cluster failed! current cluster No: %d\n", curr_cluster);
			return -1;
		}
	}

	//3.5 打印size
	printf("\nfile size: %d", size);

	return 0;
}

//4.1 
int fs_open_test(void) {
	//4.2 定义不存在的文件
	const char* not_exist_path = "/file_not_exist.txt";
	//4.2 定义存在的文件
	const char* exist_path = "/12345678ABC"; //对应的实际的文件名: 12345678.abc. 其中/是根目录
	//4.3 定义路径名
	const char* file1 = "/open/file.txt"; //存在的
	const char* file2 = "/open/a0/a1/a2/a3/a4/a5/a6/a7/a8/a9/a10/a11/a12/a13/a14/a15/a16/a17/a18/a19/file.txt"; //存在的

	xfat_err_t err;
	xfile_t file;

	printf("fs open test...\n");

	//打开根目录:
	err = xfile_open(&xfat, &file, "/"); //"/是根目录的意思, 这很像linux"
	if (err) {
		printf("open file failed./ \n");
		return -1;
	}
	xfile_close(&file);

	//4.2 打开不存在的文件
	err = xfile_open(&xfat, &file, not_exist_path);
	if (err == 0) { //为了调试起见, 所以我们会让错误的通过, 错误的是 err != 0
		printf("open file succeed./ \n");
		return -1;
	}
	xfile_close(&file);
	
	//4.2 打开存在的文件
	err = xfile_open(&xfat, &file, exist_path);
	if (err) {
		printf("open file failed./ \n");
		return -1;
	}
	xfile_close(&file);

	//4.3 打开文件file1
	err = xfile_open(&xfat, &file, file1);
	if (err) {
		printf("open file failed./ \n");
		return -1;
	}
	xfile_close(&file);

	//4.3 打开文件file2
	err = xfile_open(&xfat, &file, file2);
	if (err) {
		printf("open file failed./ \n");
		return -1;
	}
	xfile_close(&file);


	printf("file open test ok.\n");
	return 0;
}

//4.4 打印文件信息
void show_file_info(xfileinfo_t* info) {
	//名字
	printf("\n\n name: %s, ", info->file_name); 
	
	//类型
	switch (info->type) {
	case FAT_FILE:
		printf("file, ");
		break;
	case FAT_DIR:
		printf("dir, ");
		break;
	case FAT_VOL:
		printf("vol, ");
		break;
	default:
		printf("Unknown,");
		break;
	}

	//时间
	printf("\n\tcreate date: %d-%d-%d, ", info->create_time.year, info->create_time.month, info->create_time.day);
	printf("\n\tcreate time: %d-%d-%d, ", info->create_time.hour, info->create_time.minute, info->create_time.second);

	printf("\n\twrite date: %d-%d-%d, ", info->modify_time.year, info->modify_time.month, info->modify_time.day);
	printf("\n\twrite time: %d-%d-%d, ", info->modify_time.hour, info->modify_time.minute, info->modify_time.second);

	printf("\n\tlast access date: %d-%d-%d, ", info->last_acctime.year, info->last_acctime.month, info->last_acctime.day);

	//文件大小
	printf("\n\tsize %d kB, ", info->size / 1024);

	printf("\n");

}

//4.6 打印file下面的所有文件和目录
int list_sub_file(xfile_t* file, int curr_depth) {
	int i;
	int err = 0;
	xfileinfo_t fileinfo;

	//将当前目录所有文件遍历
	err = xdir_first_file(file, &fileinfo);
	if (err) return err;



	//将所有目录项遍历
	do {
		xfile_t sub_file;

		if (fileinfo.type == FAT_DIR) { //如果这也是个目录, 那就继续列举子目录(递归)
			//打印树结构:
			for (i = 0; i < curr_depth; i++) {
				printf("-");
			}

			printf("%s\n", fileinfo.file_name);
			
			//打开当前目录下面的所有子目录
			err = xfile_open_sub(file, fileinfo.file_name, &sub_file);
			if (err < 0) return err;

			//把里面的信息列出来
			err = list_sub_file(&sub_file, curr_depth + 1); //递归
			if (err < 0) return err;

			//关闭
			xfile_close(&sub_file);
		}
		else { //普通文件
			//打印树结构: (注意, 如果打印的文件前面没有-, 说明这个文件在根目录下面.)
			for (i = 0; i < curr_depth; i++) {
				printf("-");
			}

			printf("%s\n", fileinfo.file_name);
		}
	} while ((err = xdir_next_file(file, &fileinfo)) == 0);

	return err;
}


//4.4 目录的遍历函数
int dir_trans_test(void) {
	xfat_err_t err;
	xfile_t top_dir;
	xfileinfo_t fileinfo;

	printf("\ntrans dir test begin.\n");

	//err = xfile_open(&xfat, &top_dir, "/");//遍历根目录下的文件名和目录
	//err = xfile_open(&xfat, &top_dir, "/read");//遍历read目录下的文件名和目录
	err = xfile_open(&xfat, &top_dir, "/read/..");//遍历read的上层: 根目录的文件名和目录
	if (err < 0)
	{
		printf("Open Directory Failed.\n");
		return -1;
	}

	//获取第一个文件
	err = xdir_first_file(&top_dir, &fileinfo);
	if (err < 0) {
		printf("get first file info failed.\n");
		return -1;
	}

	//显示文件信息
	show_file_info(&fileinfo);

	//遍历获取这个跟目录下的所有文件
	while ((err = xdir_next_file(&top_dir, &fileinfo)) == 0) {
		show_file_info(&fileinfo);
	}
	if (err < 0) {
		printf("get next file info failed.\n");
		return -1;
	}

	//4.6 打印top_dir下面的所有文件和目录. 
	printf("\n try to list all sub files.\n");
	err = list_sub_file(&top_dir, 0); //这是个递归函数, 深度是0
	if (err < 0) {
		printf("list sub file failed.\n");
		return -1;
	}


	err = xfile_close(&top_dir);
	if (err < 0)
	{
		printf("close file failed. \n");
		return -1;
	}

	printf("file trans test ok.\n");
	return 0;
}

int main(void)
{
	xfat_err_t err;
	
	//2.2 打开disk文件
	err = xdisk_open(&disk, "vdisk", &vdisk_driver, (void*)disk_path);
	if (err) {
		printf("open disk failed.\n");
		return -1;
	}

	//2.2 打开后, 
	err = disk_part_test();//设置断点
	if (err) return err;

	//3.1 获取第0块扇区(第一个分区的那512字节的mbr)的信息. 注意, 其实这里disk_part里面将会保存指向的FAT32部分的起始地址, 见disk_part.start_sector
	err = xdisk_get_part(&disk, &disk_part, 1); //读取第0块
	if (err < 0) {
		printf("read partition failed!\n");
		return -1;
	}

	//3.2 测试
	err = xfat_open(&xfat, &disk_part);
	if (err < 0) return err;

	//3.3 测试: fat目录的测试 //注意, 这个是fat_dir_test(), 不是disk_part_test(), 既然是fat_dir_test(), 就要运行在fat_open()后面
	//err = fat_dir_test(); //3.3 这里只是检测了根目录的第一个簇里面的N个目录项, 不是根目录的所有的簇
	//if (err) return err;

	//3.5 测试: 打印64.txt的信息
	//err = fat_file_test();
	//if (err) return err;

	//4.1 测试
	//err = fs_open_test();
	//if (err < 0) return err;

	//4.4 测试: 目录的遍历函数
	err = dir_trans_test();
	if (err < 0) return err;

	//2.2 关闭
	err = xdisk_close(&disk);
	if (err) {
		printf("close disk failed.\n");
		return -1;
	}

	printf("Test End");
	return 0;
}