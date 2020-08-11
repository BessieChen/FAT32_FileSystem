#include<stdio.h>
#include "xdisk.h"
#include "xfat.h" //包括了xtypes.h等
#include <string.h> //包括了memset(), memcmp()

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

	//2.2 关闭
	err = xdisk_close(&disk);
	if (err) {
		printf("close disk failed.\n");
		return -1;
	}

	printf("Test End");
	return 0;
}