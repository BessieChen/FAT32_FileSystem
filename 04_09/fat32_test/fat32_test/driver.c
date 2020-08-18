//1.1 前面定义了disk的结构和接口, 所以这里实现disk的接口driver
#include "xdisk.h"
#include "xfat.h" //todo, 这里不太明白为什么要这个头文件
#include <stdio.h>//1.2 会使用到c语言的文件读写接口

//1.1 实现驱动driver的接口:开关读写
//注意: 定义成了static类型, 说明只是属于这个.c文件
static xfat_err_t xdisk_hw_open(xdisk_t* disk, void* init_data)
{
	//1.2 path可以是固定值, 或者是相对值. 这里是init_data复制
	const char* path = (const char*)init_data;
	//1.2 c语言的文件打开, 目录上的文件->当成磁盘
	FILE* file = fopen(path, "rb+"); //　rb+ 读写打开一个二进制文件，只允许读写数据。
	if (file == NULL)
	{
		printf("open disk failed: %s\n", path);
		return FS_ERR_IO;//在xtypes.h
	}

	disk->data = file; //1.2 在文件file打开的时候, 将信息存入disk->data中, 这里相当于缓存在data中, 供以后用
	disk->sector_size = 512; 

	//1.2 老师: c语言没有返回file文件大小的api, 也不想用win自带的api
	fseek(file, 0, SEEK_END); //将读写位置设置到距离文件末尾的0位置处(也就是文件末尾处), 详见笔记
	disk->total_sector = ftell(file) / disk->sector_size; //调用函数 ftell 就能非常容易地确定文件的当前位置, 也就是文件末尾的位置
	return	FS_ERR_OK;
}
static xfat_err_t xdisk_hw_close(xdisk_t* disk)
{
	//1.2 因为file指针已经存在disk->data中了
	FILE* file = (FILE*)disk->data;
	fclose(file);
	return FS_ERR_OK;
}
static xfat_err_t xdisk_hw_read_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count)
{
	u32_t offset = start_sector * disk->sector_size; //这个开始扇区, 是相对于文件的其实位置说的, 所以, 这个offset偏移量, 就是相对于文件其实位置的偏移量
	//1.2 获取文件指针
	FILE* file = (FILE*)disk->data;
	//1.2 计算read的起始位置: 也就是文件起始位置开始, 向前走offset个单位
	int err = fseek(file, offset, SEEK_SET);//其中SEEK_SET就是文件的起始位置

	if (err == -1)
	{
		printf("seek disk failed: 0x%x\n", offset);
		return FS_ERR_IO;
	}

	err = fread(buffer, disk->sector_size, count, file);//buffer: 读完的内容放到buffer, count: 读取的次数, sector_size: 每次读取的大小(字节吧)
	if (err == -1)
	{
		printf("read disk failed: sector: %d, count: %d\n", disk->sector_size, count);
		return FS_ERR_IO;
	}

	//1.2 补充: 为什么先fseek()后fread()
	/*
	1. 其实读取是由一个指针指向进行读取
	2. fread()是从当前指针所指向的地方读取, 可是我们在进入这个函数之前, 我们无法肯定这个指针是不是指向我们期待的位置
	3. 所以用fseek()确保指针指向的是期待的位置
	*/
	return FS_ERR_OK;
}
static xfat_err_t xdisk_hw_write_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count)
{
	u32_t offset = start_sector * disk->sector_size; 
	FILE* file = (FILE*)disk->data;
	int err = fseek(file, offset, SEEK_SET);

	if (err == -1)
	{
		printf("seek disk failed: 0x%x\n", offset);
		return FS_ERR_IO;
	}

	err = fwrite(buffer, disk->sector_size, count, file);
	if (err == -1)
	{
		printf("write disk failed: sector: %d, count: %d\n", disk->sector_size, count);
		return FS_ERR_IO;
	}
	//1.2 前面的内容和read一样, 只不过write需要加一个刷新文件
	fflush(file);
	/*
	为什么需要fflush()
	1. 调试的时候, 会设置断点
	2. 当发现有问题, 会强制终止调试
	3. 终止调试, 导致问题: 一部分数据,从buffer写到了操作系统的缓存里, 没有回写到磁盘disk上
	4. 为了确保写到disk上, 需要用刷新fflush()
	*/
	return FS_ERR_OK;

}

//1.1 定义驱动driver的结构体的对象, 其中vdisk的意思virtual driver虚拟驱动
xdisk_driver_t vdisk_driver =
{
	.close = xdisk_hw_close, //注意是逗号结尾
	.open = xdisk_hw_open, //c99的特性(一个点+函数名: .open), 可以这么定义一个对象, 而且顺序可以改变, 例如close在open前面
	.read_sector = xdisk_hw_read_sector,
	.write_sector = xdisk_hw_write_sector
}; //记得有分号结尾

/*
1. 驱动driver的实现, 为什么不在xdisk.h中实现
	两个同样的sd, 可能只是块大小不同, 但是四个接口都完全一样
	所以, 我们会独立实现driver, 但是其他信息可以不同
	不同的信息 + 相同的diver => 不同的sd
*/
