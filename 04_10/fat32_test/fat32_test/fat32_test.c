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

//4.8 读取文件
int file_read_and_check(const char* path, xfile_size_t ele_size, xfile_size_t count)
{
	xfile_t file;
	xfile_size_t read_count;

	//打开文件
	xfat_err_t err = xfile_open(&xfat, &file, path);
	if (err != FS_ERR_OK) {
		printf("open file failed! %s \n", path);
		return -1;
	}

	//读取文件
	if ((read_count = xfile_read(read_buffer, ele_size, count, &file) > 0)) {
		u32_t i = 0;
		xfile_size_t bytes_count = read_count * ele_size; //实际读取的元素个数 * 元素的字节数
		u32_t num_start = 0;//要比较的数字

		//开始比较我们读取的值是不是真值(真值是每4个字节, 就是一个数字, 例如前4个字节是00 00 00 00, 下4个字节是01 00 00 00 代表着数字1)
		for(i = 0; i < bytes_count; i += 4) //32位的整型比较, 因为这里是i代表的是字节数, 所以是+=4
		{
			int int_index = i / 4; //这是第几个四字节的意思
			if (read_buffer[int_index] !=  num_start++) //所以每距离4个字节, 就要加一
			{
				printf("number doesn't match.\n");
				return -1;
			}
		}
	}

	//如果上面的if()中read_count == 0说明有错误码
	if (xfile_error(&file) < 0) {
		printf("read file failed.\n");
		return -1;
	}

	//关闭文件
	xfile_close(&file);

	return 0;

}

//4.8 读取文件的测试
int fs_read_test(void) {
	//disk中有一个read目录, 这个目录里有一个0字节的文件:
	const char* file_0b_path = "/read/0b.bin";
	const char* file_1MB_path = "/read/1MB.bin";

	xfat_err_t err;

	printf("\n file read test begins:\n");

	//读取缓冲区的清空
	memset(read_buffer, 0, sizeof(read_buffer));

	err = file_read_and_check(file_0b_path, 32, 1); //给定了path, 读取的元素大小:32字节(不超过一个扇区), 读一个元素
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	//一个扇区内
	err = file_read_and_check(file_1MB_path,disk.sector_size - 32, 1); //给定了path, 读取的元素大小:(差32字节就一个扇区: 也就是512-32=480字节)(也就是不超过一个扇区), 读一个元素
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	//一个整的扇区
	err = file_read_and_check(file_1MB_path, disk.sector_size, 1); //给定了path, 读取的元素大小:一个扇区(但是不超过一个簇). 读一个元素
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	//一个多扇区(但是不跨簇)
	err = file_read_and_check(file_1MB_path, disk.sector_size + 14, 1); //给定了path, 读取的元素大小:一个多扇区(但是不超过一个簇, 读一个元素
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}


	//一个多簇
	err = file_read_and_check(file_1MB_path, xfat.cluster_byte_size + 32, 1);
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	//多个簇
	err = file_read_and_check(file_1MB_path, 2 * xfat.cluster_byte_size + 32, 1);
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	return 0;
}

//4.9 定位
int fs_seek(xfile_t* file, xfile_origin_t origin, xfile_ssize_t offset) {
	
	int err;
	xfile_ssize_t expected_pos;//计算检测的期望值. 
	int count; //用于后面计算, 实际读取了几个字节

	switch (origin) {
	case XFAT_SEEK_SET:
		expected_pos = offset;
		break;

	case XFAT_SEEK_CUR:
		expected_pos = file->pos + offset;
		break;

	case XFAT_SEEK_END:
		expected_pos = file->size + offset;
		break;

	default:
		expected_pos = 0;
		break;
	}

	//调用seek函数去定位
	err = xfile_seek(file, offset, origin);
	if (err) {
		printf("seek error01!\n");
		return err;
	}

	//检测: file->pos是否是真值. 其实我会发现,expected_pos其实就是一个绝对位置, 很好计算. 但是在xfile_seek()中的file->pos是经过很复杂的计算的, 例如是否需要更新簇
	if (xfile_tell(file) != expected_pos) {
		printf("seek error02.\n");
		return -1;
	}

	//在这个file->pos读取数据
	count = xfile_read(read_buffer, 1, 1, file); //从file->pos开始读取, 读取元素大小为1字节, 元素个数为1个, 所以就是一个字节
	if (count < 1) {
		printf("seek error03\n"); //说明没有读到哪个最
		return -1;
	}

	//验证, 读到的这一个字节, 它的值是不是expected % 256, 之所以是256, 因为我们每个字节是00,01,02...FE,FF.然后再从头开始//这是一共2^8=256个字节
	if (*(u8_t*)read_buffer != (expected_pos % 256)) {
		printf("seek error04\n"); 
		return -1;
	}

	//printf("sucess: %d\n",offset);
	return 0;
}


//4.9 定位测试
int fs_seek_test(void) {
	xfat_err_t err;
	xfile_t file;

	printf("\n file seek test!\n");
	err = xfile_open(&xfat, &file, "/seek/1MB.bin"); //打开文件
	if (err != FS_ERR_OK) {
		printf("open file failed.\n");
		return -1;
	}

	//测试从起始开始
	err = fs_seek(&file, XFAT_SEEK_SET, 32); //测试: 一个扇区内
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_SET, 576); //测试: 一个扇区外,一个簇内
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_SET, 4193); //测试: 一个簇外. 一个簇是512*4=2048. 这里超过两个簇:2048*2=4096
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_SET, -1); //测试: 非法
	if (err == FS_ERR_OK) return err;

	//从当前位置开始
	err = fs_seek(&file, XFAT_SEEK_CUR, 32);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, 576);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, 4193);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, -32); //向左
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, -512);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, -1024);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, -0xFFFFFFF); //报错: 因为向左移动了一个最大的数
	if (err == FS_ERR_OK) return err;

	//从结尾开始
	err = fs_seek(&file, XFAT_SEEK_END, -32); //向左
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_END, -576);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_END, -4193);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_END, 32); //报错
	if (err == FS_ERR_OK) return err;


	//关闭
	xfile_close(&file);
	return 0;
}

//4.10 重命名测试
xfat_err_t fs_modify_file_test(void) {
	xfat_err_t err;
	xfile_t file;
	const char* dir_path = "/modify/a0/a1/a2/";
	const char* file_name1[] = "ABC.efg";
	const char* file_name2[] = "efg.ABC";
	char curr_path[64];

	printf("modify file attr test..\n");
	printf("\n Before rename:\n");

	err = xfile_open(&xfat, &file, dir_path); //打开目录
	if (err < 0) {
		printf("open dir failed.\n");
		return err;
	}

	err = list_sub_file(&file, 0);
	if(err < 0)
	{
		return err;
	}

	xfile_close(&file);

	sprintf(curr_path, "%s%s", dir_path, file_name1);
	err = xfile_open(&xfat, &file, curr_path);
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
	//err = dir_trans_test();
	//if (err < 0) return err;

	//4.8 测试: 读取文件
	//err = fs_read_test();
	//if (err < 0) printf("read test failed.\n");

	//4.9 测试: 定位
	err = fs_seek_test();
	if (err) return err;

	//4.10 测试: 文件修改, 重命名
	err = fs_modify_file_test();
	if (err) return err;

	//2.2 关闭
	err = xdisk_close(&disk);
	if (err) {
		printf("close disk failed.\n");
		return -1;
	}

	printf("Test End");
	return 0;
}