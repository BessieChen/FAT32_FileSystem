#ifndef XFAT_H
#define XFAT_H

#include "xtypes.h"

//3.1 定义FAT分区里面的第0块内容, 也就是保留区
#pragma pack(1) //因为我们要保证和disk里面的内容一一对应, 不留空隙, 所以记得用pragma pack(1)

//bios parameter block(参数块), 一共有3+8+2+1+2+1+2+2+1+2+2+2+4+4=36字节
typedef struct _bpb_t {
	u8_t BS_jmpBoot[3];//三字节的跳转代码, 一般是: 0xeb, 0x58, ....,
	u8_t BPB_OEMName[8];//OEM的名称, 8字节
	u16_t BPB_BytesPerSec;//每个扇区,有多少字节
	u8_t BPB_SecPerClus;//每簇扇区的数目, section per cluster
	u16_t BPB_RsvdSecCnt;//保留区的扇区数, reserved section count
	u8_t BPB_NumFATs;//FAT表项数
	u16_t BPB_BootEntCnt;//根目录项目数, boot entry count
	u16_t BPB_TotSec16;//总的扇区数, total section
	u8_t BPB_Media;//媒体类型
	u16_t BPB_FATSz16;//FAT表项大小, size
	u16_t BPB_SecPerTrk;//每磁道扇区数, section per track
	u16_t BPB_NumHeads;//磁头数
	u32_t BPB_HiddSec;//隐藏扇区数
	u32_t BPB_TotSec32;//总的扇区数
}bpb_t;

//fat32字块
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

#endif // !XFAT_H

