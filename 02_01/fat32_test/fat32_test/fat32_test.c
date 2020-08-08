#include<stdio.h>
#include "xdisk.h"
#include "xfat.h" //包括了xtypes.h等
#include <string.h> //包括了memset(), memcmp()

//1.2 测试: 将虚拟磁盘vdisk的实例用extern引用过来,
extern xdisk_driver_t vdisk_driver;
//1.2 定义一个测试磁盘的路径
const char* disk_path_test = "disk_test.img"; //相对路径
//1.2 定义几个缓冲区, 测试读写
static u32_t write_buffer[160 * 1024]; //160kb
static u32_t read_buffer[160 * 1024];
//1.2 定义测试函数, 测试四个接口:开关读写
int disk_io_test(void) {
	xfat_err_t err;//错误码
	
	//定义磁盘结构
	xdisk_t disk_test;

	//1.3 删disk_test.driver = &vdisk_driver; //将虚拟磁盘设置为disk_test的驱动driver

	//将读取缓存read_buffer清零
	memset(read_buffer, 0, sizeof(read_buffer));

	//1.3 改, 调用驱动的打开接口
	err = xdisk_open(&disk_test, "vdisk_test", &vdisk_driver, (void*)disk_path_test);//加入了名字, 还有vdisk_driver的路径 //1.3 删err = disk_test.driver->open(&disk_test, disk_path_test);

	if (err) { //不为零, 就是失败
		printf("open disk failed.\n");
		return -1;
	}

	//1.3 改, 测试写的接口
	err = xdisk_write_sector(&disk_test, (u8_t*)write_buffer, 0, 2); //从0扇区开始, 写2个扇区, 记得吗, 一个扇区的大小是512(字节?)
	if (err) {
		printf("write disk failed.\n");
		return -1;
	}

	//1.3 改, 测试:读
	err = xdisk_read_sector(&disk_test, (u8_t*)read_buffer, 0, 2); //从0扇区开始, 写2个
	if (err) {
		printf("read disk failed.\n");
		return -1;
	}

	//比较两个缓存区, 比较的范围:2个扇区
	err = memcmp((u8_t*)read_buffer, (u8_t*)write_buffer, 2 * disk_test.sector_size);
	if (err)
	{
		printf("data not equal.\n");
		return -1;
	}

	//1.3 改, 最后关闭
	err = xdisk_close(&disk_test); 
	if (err) {
		printf("close disk failed.\n");
		return -1;
	}

	printf("disk io test fine. \n");
	return 0;

	//todo:
	/*1. 为什么是(u8_t) 2. 写2个是什么单位: 扇区*/
}


//2.1 添加测试的结构体
#pragma pack(1)
typedef struct _mbr_t {
	u32_t a;
	u16_t b;
	u32_t c;
}mbr_t;
#pragma pack() //注意, 这里括号什么都没有

int main(void)
{
	//write缓冲的初始化
	for (int i = 0; i < sizeof(write_buffer) / sizeof(u32_t); i++) {
		write_buffer[i] = i;
	}

	/*xfat_err_t err;
	err = disk_io_test();
	if (err)
	{
		return err;
	}*/

	//2.1 测试
	mbr_t* mbr = (mbr_t*)0x100; //声明一个地址0x100, 这个地址上面存的是mbr_t结构体的第一个成员u32_t a;
	printf("%p\n", &(mbr->c)); //用%p打印地址, 如果没有pragma pack(1), 就是0000 0108, 否则就是0000 0106

	printf("Test End");
	return 0;
}