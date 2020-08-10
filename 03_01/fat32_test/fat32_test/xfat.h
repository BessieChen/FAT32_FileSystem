#ifndef XFAT_H
#define XFAT_H

#include "xtypes.h"

//3.1 ����FAT��������ĵ�0������, Ҳ���Ǳ�����
#pragma pack(1) //��Ϊ����Ҫ��֤��disk���������һһ��Ӧ, ������϶, ���Լǵ���pragma pack(1)

//bios parameter block(������), һ����3+8+2+1+2+1+2+2+1+2+2+2+4+4=36�ֽ�
typedef struct _bpb_t {
	u8_t BS_jmpBoot[3];//���ֽڵ���ת����, һ����: 0xeb, 0x58, ....,
	u8_t BPB_OEMName[8];//OEM������, 8�ֽ�
	u16_t BPB_BytesPerSec;//ÿ������,�ж����ֽ�
	u8_t BPB_SecPerClus;//ÿ����������Ŀ, section per cluster
	u16_t BPB_RsvdSecCnt;//��������������, reserved section count
	u8_t BPB_NumFATs;//FAT������
	u16_t BPB_BootEntCnt;//��Ŀ¼��Ŀ��, boot entry count
	u16_t BPB_TotSec16;//�ܵ�������, total section
	u8_t BPB_Media;//ý������
	u16_t BPB_FATSz16;//FAT�����С, size
	u16_t BPB_SecPerTrk;//ÿ�ŵ�������, section per track
	u16_t BPB_NumHeads;//��ͷ��
	u32_t BPB_HiddSec;//����������
	u32_t BPB_TotSec32;//�ܵ�������
}bpb_t;

//fat32�ֿ�
typedef struct _fat32_hdr_t {
	u32_t BPB_FATSz32; //fat����ֽڴ�С
	u16_t BPB_ExtFlags; //��չ���
	u16_t BPB_FSVer;//�汾��
	u32_t BPB_RootClus;//��Ŀ¼�Ĵغ�
	u16_t BPB_FsInfo;//fsInfo��������
	u16_t BPB_BkBootSec;//��������
	u8_t BPB_Reserved[12];
	u8_t BS_DrvNum;//�豸��
	u8_t BS_Reserved1;
	u8_t BS_BootSig;//��չ���
	u32_t BS_VolID;//�����к�
	u8_t BS_VolLab[11];//�������
	u8_t BS_FileSysType[8];//�ļ���������, ���Կ�����ʾ����0x00d3 a1b2, "FAT32"
}fat32_hdr_t;

typedef struct _dbr_t {
	bpb_t bpb;
	fat32_hdr_t fat32;
}dbr_t;
#pragma pack()

#endif // !XFAT_H

