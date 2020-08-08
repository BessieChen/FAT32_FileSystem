#ifndef XDISK_H
#define XDISK_H

#include "xtypes.h"

//1.1 前向声明: 告诉编译器, 这个struct已经定义了, 服务于第line14
struct _xdisk_driver_t;

//1.1 定义结构体: 用来描述disk(里面包含了disk的信息, 以及disk的操作[通过接口结构体的指针实现])
typedef struct _xdisk_t
{
	u32_t sector_size;//每个disk块的大小(扇区大小)
	u32_t total_sector;//一共多少扇区
	struct _xdisk_driver_t* driver;//将disk接口这个结构体的指针放入 //这里就用到了前向声明
	void* data; //1.2 将任意类型的数据传入这里, 相当于一个缓存区
}xdisk_t;

//1.1 定义结构体: disk的接口
typedef struct _xdisk_driver_t { //四种: 开关读写, 以下都是函数
	xfat_err_t(*open)(xdisk_t* disk, void* init_data); //返回值:错误码_xfat_err_t, 函数名:open, 第二个参数init_data:需要传入初始化的参数, 提供具体应该如何打开的信息 
	xfat_err_t(*close)(xdisk_t* disk);
	xfat_err_t(*read_sector)(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);//每次读, 都是以块为单位. buffer: 读取的信息放到这个buffer, start_sector: 开始读的扇区, count:读多少内容
	xfat_err_t(*write_sector)(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);//每次写, 都是以块为单位. buffer: 将信息从buffer写出, start_sector: 开始写的扇区, count:写多少内容
}xdisk_driver_t;

//1.1 两个需要注意的点
/*
1. 因为struct _xdisk_t和 struct _xdisk_driver_t内部都有对方的定义
	1. 因为我们是先定义了struct _xdisk_t, 所以在后者struct _xdisk_driver_t中有前者的typedef, 所以我们看到可以直接写(xdisk_t* disk);
	2. 因为struct _xdisk_t在前面, 它不知道struct _xdisk_driver_t的定义, 所以需要前向声明: 加上line7的struct _xdisk_driver_t;; 在最前面
2. 对于xfat_err_t, 因为这个结构体xfat_err_t已经在#include "xtypes.h"里面了, 所以line19-22出现xfat_err_t完全没问题
*/
#endif