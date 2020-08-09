#ifndef XDISK_H
#define XDISK_H

#include "xtypes.h"

//2.2  文件系统类型
typedef enum {
	FS_NOT_VALID = 0x00,
	FS_FAT32 = 0x01,
	FS_EXTEND = 0x05, //扩展分区
	FS_WIN95_FAT32_0 = 0x0B, //这个值是已经约定好的
	FS_WIN95_FAT32_1 = 0x0C,
}xfs_type_t;

//2.2 紧凑设置开始
#pragma pack(1)

//2.2 分配配置, 16字节
typedef struct _mbr_part_t {
	u8_t boot_active;//分区是否活动

	u8_t start_header; //磁盘物理结构体的起始, 扇区开始的header
	u16_t start_sector : 6; //扇区开始的起始地址, 注意虽然是u16_t, 但是只占了6个bit //少用
	u16_t start_cylinder : 10; //占了10个bit

	u8_t system_id; //分区的类型

	u8_t end_header;//扇区结束的header
	u16_t end_sector : 6;//扇区结束的起始地址, 注意虽然是u16_t, 但是只占了6个bit
	u16_t end_cylinder : 10;//扇区结束的??, 占了10个bit

	u32_t relative_sectors; //相对的扇区数: 在一个分区中, 某个扇区举例该分区的第一个扇区的相对位置
	u32_t total_sectors; //总共的扇区数
}mbr_part_t;
//字节计算: 8+8+6+10+8+6+10+32+32 = 16*8bit

//2.2 宏定义
#define	MBR_PRIMARV_PART_NR		4	//4个分区配置

//2.2 添加MBR结构
typedef struct _mbr_t {
	//446字节的启动代码
	u8_t code[446];
	//64字节的分区表: 4个分区配置 * 16字节
	mbr_part_t part_info[MBR_PRIMARV_PART_NR];
	//2字节:启动的有效标志
	u8_t boot_sig[2];
}mbr_t;
#pragma pack() //2.2 紧凑设置结束
//1.1 前向声明: 告诉编译器, 这个struct已经定义了, 服务于第line14
struct _xdisk_driver_t;

//1.1 定义结构体: 用来描述disk(里面包含了disk的信息, 以及disk的操作[通过接口结构体的指针实现])
typedef struct _xdisk_t
{	
	const char* name; //1.3 添加名称字段, 表示disk的名称
	u32_t sector_size;//1.1 每个disk块的大小(扇区大小)
	u32_t total_sector;//1.1 一共多少扇区
	struct _xdisk_driver_t* driver;//1.1 将disk接口这个结构体的指针放入 //这里就用到了前向声明
	void* data; //1.2 将任意类型的数据传入这里, 相当于一个缓存区
}xdisk_t;

//1.1 定义结构体: disk的接口
typedef struct _xdisk_driver_t { //四种: 开关读写, 以下都是函数
	xfat_err_t(*open)(xdisk_t* disk, void* init_data); //返回值:错误码_xfat_err_t, 函数名:open, 第二个参数init_data:需要传入初始化的参数, 提供具体应该如何打开的信息 
	xfat_err_t(*close)(xdisk_t* disk);
	xfat_err_t(*read_sector)(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);//每次读, 都是以块为单位. buffer: 读取的信息放到这个buffer, start_sector: 开始读的扇区, count:读多少内容
	xfat_err_t(*write_sector)(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);//每次写, 都是以块为单位. buffer: 将信息从buffer写出, start_sector: 开始写的扇区, count:写多少内容
}xdisk_driver_t;

//1.3 四个接口的声明(这里是从driver.c来的, 在此基础上修改)
xfat_err_t xdisk_open(xdisk_t* disk, const char* name, xdisk_driver_t* driver, void* init_data); //name: 初始化disk的名字, driver: 传入driver的指针
xfat_err_t xdisk_close(xdisk_t* disk);
xfat_err_t xdisk_read_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);
xfat_err_t xdisk_write_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);

//2.2 支持分区数量的检测, 检测disk上有多少个分区
xfat_err_t xdisk_get_part_count(xdisk_t* disk, u32_t* count); //用count传地址取值:取有多少个分区
#endif