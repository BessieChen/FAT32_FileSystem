#ifndef XFAT_H
#define XFAT_H

#include "xtypes.h"
#include "xdisk.h" //3.2 ��Ϊ��Ҫ�õ�xdisk_part_t

//3.1 ����FAT��������ĵ�0������, Ҳ���Ǳ�����
#pragma pack(1) //��Ϊ����Ҫ��֤��disk���������һһ��Ӧ, ������϶, ���Լǵ���pragma pack(1)

//bios parameter block(������), һ����3+8+2+1+2+1+2+2+1+2+2+2+4+4=36�ֽ�
typedef struct _bpb_t {
	u8_t BS_jmpBoot[3];//���ֽڵ���ת����, һ����: 0xeb, 0x58, ....,
	u8_t BPB_OEMName[8];//OEM������, 8�ֽ�
	u16_t BPB_BytesPerSec;//ÿ������,�ж����ֽ�
	u8_t BPB_SecPerClus;//ÿ����������Ŀ, section per cluster
	u16_t BPB_RsvdSecCnt;//��������������, reserved section count
	u8_t BPB_NumFATs;//FAT���ж��ٸ� //ע��, fat���ж��ٸ���¼��������
	u16_t BPB_BootEntCnt;//��Ŀ¼��Ŀ��, boot entry count
	u16_t BPB_TotSec16;//�ܵ�������, total section. //ע��, �����fat32����, ���ǲ���ʹ����һ��, ����ʹ��line25��u32_t BPB_TotSec32;. ֮��������һ��,����Ϊ��ʷԭ��, Ȼ��Ϊ�˼��ݰ� 
	u8_t BPB_Media;//ý������
	u16_t BPB_FATSz16;//FAT�����С, size //ע��, �����fat32����, ���ǲ���ʹ����һ��, ����ʹ��line30��_fat32_hdr_t�е�u32_t BPB_FATSz32;
	u16_t BPB_SecPerTrk;//ÿ�ŵ�������, section per track
	u16_t BPB_NumHeads;//��ͷ��
	u32_t BPB_HiddSec;//����������
	u32_t BPB_TotSec32;//�ܵ�������
}bpb_t;

//fat32�ֿ�, һ����: 4+2+2+4+2+2+1*4+4+1*2=26�ֽ�
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


//3.2 ��¼xfat��Ĺؼ���Ϣ. ע��:����ṹ��ֻ�Ǽ�¼�ؼ���Ϣ. ����Ҫ��Ӧdisk������, ���Բ���Ҫ#pragma pack(1), ����ṹֻ�������Լ��õ�
typedef struct _xfat_t {
	u32_t fat_start_sector; //fat������ʼ����
	u32_t fat_tbl_nr; //���ٸ�fat��
	u32_t fat_tbl_sectors;//ÿ��fat��ռ�ö��ٸ�����
	u32_t total_sectors;//fat���ܵ���������

	xdisk_part_t* disk_part;//���fat�����ĸ�������fat��
}xfat_t;

//3.2 ����һ������, ʵ��fat���Ĵ򿪹���
xfat_err_t xfat_open(xfat_t* xfat, xdisk_part_t* xdisk_part);//xdisk_part: ��xdisk_part�������

#endif // !XFAT_H

