#ifndef XFAT_H
#define XFAT_H

#include "xtypes.h"
#include "xdisk.h" //3.2 因为需要用到xdisk_part_t

//3.1 定义FAT分区里面的第0块内容, 也就是保留区
#pragma pack(1) //因为我们要保证和disk里面的内容一一对应, 不留空隙, 所以记得用pragma pack(1)

//bios parameter block(参数块), 一共有3+8+2+1+2+1+2+2+1+2+2+2+4+4=36字节
typedef struct _bpb_t {
	u8_t BS_jmpBoot[3];//三字节的跳转代码, 一般是: 0xeb, 0x58, ....,
	u8_t BPB_OEMName[8];//OEM的名称, 8字节
	u16_t BPB_BytesPerSec;//每个扇区,有多少字节
	u8_t BPB_SecPerClus;//每簇扇区的数目, section per cluster
	u16_t BPB_RsvdSecCnt;//保留区的扇区数, reserved section count
	u8_t BPB_NumFATs;//FAT表有多少个 //注意, fat表有多少个记录在了这里
	u16_t BPB_BootEntCnt;//根目录项目数, boot entry count
	u16_t BPB_TotSec16;//总的扇区数, total section. //注意, 如果是fat32分区, 我们不会使用这一条, 而是使用line25的u32_t BPB_TotSec32;. 之所以有这一个,是因为历史原因, 然后为了兼容把 
	u8_t BPB_Media;//媒体类型
	u16_t BPB_FATSz16;//FAT表项大小, size //注意, 如果是fat32分区, 我们不会使用这一条, 而是使用line30的_fat32_hdr_t中的u32_t BPB_FATSz32;
	u16_t BPB_SecPerTrk;//每磁道扇区数, section per track
	u16_t BPB_NumHeads;//磁头数
	u32_t BPB_HiddSec;//隐藏扇区数
	u32_t BPB_TotSec32;//总的扇区数
}bpb_t;

//fat32字块, 一共有: 4+2+2+4+2+2+1*4+4+1*2=26字节
typedef struct _fat32_hdr_t {
	u32_t BPB_FATSz32; //fat表的字节大小
	u16_t BPB_ExtFlags; //扩展标记
	u16_t BPB_FSVer;//版本号
	u32_t BPB_RootClus;//根目录的簇号
	u16_t BPB_FsInfo;//fsInfo的扇区号
	u16_t BPB_BkBootSec;//备份扇区
	u8_t BPB_Reserved[12];
	u8_t BS_DrvNum;//设备号
	u8_t BS_Reserved1;
	u8_t BS_BootSig;//扩展标记
	u32_t BS_VolID;//卷序列号
	u8_t BS_VolLab[11];//卷标名称
	u8_t BS_FileSysType[8];//文件类型名称, 可以看到显示的是0x00d3 a1b2, "FAT32"
}fat32_hdr_t;

typedef struct _dbr_t {
	bpb_t bpb;
	fat32_hdr_t fat32;
}dbr_t;
#pragma pack()


//3.2 记录xfat表的关键信息. 注意:这个结构体只是记录关键信息. 不需要对应disk的数据, 所以不需要#pragma pack(1), 这个结构只是我们自己用的
typedef struct _xfat_t {
	u32_t fat_start_sector; //fat区的起始扇区
	u32_t fat_tbl_nr; //多少个fat表
	u32_t fat_tbl_sectors;//每个fat表占用多少个扇区
	u32_t total_sectors;//fat区总的扇区数量

	xdisk_part_t* disk_part;//这个fat区是哪个分区的fat区
}xfat_t;

//3.2 定义一个函数, 实现fat区的打开功能
xfat_err_t xfat_open(xfat_t* xfat, xdisk_part_t* xdisk_part);//xdisk_part: 打开xdisk_part这个分区

#endif // !XFAT_H

