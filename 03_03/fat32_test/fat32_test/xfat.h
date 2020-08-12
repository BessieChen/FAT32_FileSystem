#ifndef XFAT_H
#define XFAT_H

#include "xtypes.h"
#include "xdisk.h" //3.2 ��Ϊ��Ҫ�õ�xdisk_part_t

//3.3 ��, ����Dir_name
#define DIRITEM_NAME_FREE               0xE5                // Ŀ¼����������
#define DIRITEM_NAME_END                0x00                // Ŀ¼����������

//3.3 ����
#define DIRITEM_ATTR_READ_ONLY          0x01                // Ŀ¼�����ԣ�ֻ��
#define DIRITEM_ATTR_HIDDEN             0x02                // Ŀ¼�����ԣ�����
#define DIRITEM_ATTR_SYSTEM             0x04                // Ŀ¼�����ԣ�ϵͳ����
#define DIRITEM_ATTR_VOLUME_ID          0x08                // Ŀ¼�����ԣ���id
#define DIRITEM_ATTR_DIRECTORY          0x10                // Ŀ¼�����ԣ�Ŀ¼
#define DIRITEM_ATTR_ARCHIVE            0x20                // Ŀ¼�����ԣ��鵵
#define DIRITEM_ATTR_LONG_NAME          0x0F                // Ŀ¼�����ԣ����ļ���


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
	u32_t BPB_FATSz32; //fat�����ֽڴ�С
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
	u8_t BS_VolLab[11];//��������
	u8_t BS_FileSysType[8];//�ļ���������, ���Կ�����ʾ����0x00d3 a1b2, "FAT32"
}fat32_hdr_t;

typedef struct _dbr_t {
	bpb_t bpb;
	fat32_hdr_t fat32;
}dbr_t;


// 3.3 FATĿ¼�����������, diritem_t��ʹ��
typedef struct _diritem_date_t {
	u16_t day : 5;                  // ��
	u16_t month : 4;                // ��
	u16_t year_from_1980 : 7;       // ��
} diritem_date_t;


//3.3 FATĿ¼���ʱ������, diritem_t��ʹ��
typedef struct _diritem_time_t {
	u16_t second_2 : 5;             // 2��, ���Ӧ�þ�������˵��fat32ϵͳ��bug, ֻ����ż����
	u16_t minute : 6;               // ��
	u16_t hour : 5;                 // ʱ
} diritem_time_t;

/**
* //3.3 ������������, �����������, Ŀ¼��
*/
typedef struct _diritem_t {
	u8_t DIR_Name[8];                   // �ļ���
	u8_t DIR_ExtName[3];                // ��չ��
	u8_t DIR_Attr;                      // ����
	u8_t DIR_NTRes;
	u8_t DIR_CrtTimeTeenth;             // ����ʱ��ĺ���
	diritem_time_t DIR_CrtTime;         // ����ʱ��
	diritem_date_t DIR_CrtDate;         // ��������
	diritem_date_t DIR_LastAccDate;     // ����������
	u16_t DIR_FstClusHI;                // �غŸ�16λ
	diritem_time_t DIR_WrtTime;         // �޸�ʱ��
	diritem_date_t DIR_WrtDate;         // �޸�ʱ��
	u16_t DIR_FstClusL0;                // �غŵ�16λ
	u32_t DIR_FileSize;                 // �ļ��ֽڴ�С
} diritem_t;

#pragma pack() //���϶��ǲ����п�϶��


//3.2 ��¼xfat�����������Ĺؼ���Ϣ. ע��:����ṹ��ֻ�Ǽ�¼�ؼ���Ϣ. ����Ҫ��Ӧdisk������, ���Բ���Ҫ#pragma pack(1), ����ṹֻ�������Լ��õ�
typedef struct _xfat_t {
	u32_t fat_start_sector; //fat������ʼ����
	u32_t fat_tbl_nr; //���ٸ�fat��
	u32_t fat_tbl_sectors;//ÿ��fat��ռ�ö��ٸ�����

	//3.3 ���Ӻʹ���ص��ֶ�
	u32_t sec_per_cluster; //3.3 ÿ����, �ж��ٸ�����
	u32_t root_cluster; //3.3 �������ĸ�Ŀ¼(����Ŀ¼)����ʼ�غ�(Ҳ���ǵ�һ���غ�)�Ƕ���
	u32_t cluster_byte_size;//3.3 ÿ����, ռ���˶����ֽ�(ͨ��������Ի��)

	u32_t total_sectors;//fat���ܵ���������
	xdisk_part_t* disk_part;//���fat�����ĸ�������fat��
}xfat_t;

//3.2 ����һ������, ʵ��fat���Ĵ򿪹���
xfat_err_t xfat_open(xfat_t* xfat, xdisk_part_t* xdisk_part);//xdisk_part: ��xdisk_part�������

//3.3 ��ȡ�ص�����
xfat_err_t read_cluster(xfat_t* xfat, u8_t* buffer, u32_t cluster, u32_t count); //xfat: �����ṩһЩ��Ϣ(�����ĸ�disk,�ĸ�fat32����..), buffer: ��ȡ�Ľ���浽buffer, ����ַȡֵ, cluster: �����fat32���������������ĸ��غſ�ʼ��ȡ, count:������ȡ������


#endif // !XFAT_H
