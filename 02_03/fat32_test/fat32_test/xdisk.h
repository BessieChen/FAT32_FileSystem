#ifndef XDISK_H
#define XDISK_H

#include "xtypes.h"

//2.2  �ļ�ϵͳ����
typedef enum {
	FS_NOT_VALID = 0x00,
	FS_FAT32 = 0x01,
	FS_EXTEND = 0x05, //��չ����
	FS_WIN95_FAT32_0 = 0x0B, //���ֵ���Ѿ�Լ���õ�
	FS_WIN95_FAT32_1 = 0x0C,
}xfs_type_t;

//2.2 �������ÿ�ʼ
#pragma pack(1)

//2.2 ��������, 16�ֽ�
typedef struct _mbr_part_t {
	u8_t boot_active;//�����Ƿ�

	u8_t start_header; //��������ṹ�����ʼ, ������ʼ��header
	u16_t start_sector : 6; //������ʼ����ʼ��ַ, ע����Ȼ��u16_t, ����ֻռ��6��bit //����
	u16_t start_cylinder : 10; //ռ��10��bit

	u8_t system_id; //����������

	u8_t end_header;//����������header
	u16_t end_sector : 6;//������������ʼ��ַ, ע����Ȼ��u16_t, ����ֻռ��6��bit
	u16_t end_cylinder : 10;//����������??, ռ��10��bit

	u32_t relative_sectors; //��Ե�������: ��һ��������, ĳ�����������÷����ĵ�һ�����������λ��
	u32_t total_sectors; //�ܹ���������
}mbr_part_t;
//�ֽڼ���: 8+8+6+10+8+6+10+32+32 = 16*8bit

//2.2 �궨��
#define	MBR_PRIMARV_PART_NR		4	//4����������

//2.2 ���MBR�ṹ
typedef struct _mbr_t {
	//446�ֽڵ���������
	u8_t code[446];
	//64�ֽڵķ�����: 4���������� * 16�ֽ�
	mbr_part_t part_info[MBR_PRIMARV_PART_NR];
	//2�ֽ�:��������Ч��־
	u8_t boot_sig[2];
}mbr_t;
#pragma pack() //2.2 �������ý���
//1.1 ǰ������: ���߱�����, ���struct�Ѿ�������, �����ڵ�line14
struct _xdisk_driver_t;

//1.1 ����ṹ��: ��������disk(���������disk����Ϣ, �Լ�disk�Ĳ���[ͨ���ӿڽṹ���ָ��ʵ��])
typedef struct _xdisk_t
{	
	const char* name; //1.3 ��������ֶ�, ��ʾdisk������
	u32_t sector_size;//1.1 ÿ��disk��Ĵ�С(������С)
	u32_t total_sector;//1.1 һ����������
	struct _xdisk_driver_t* driver;//1.1 ��disk�ӿ�����ṹ���ָ����� //������õ���ǰ������
	void* data; //1.2 ���������͵����ݴ�������, �൱��һ��������
}xdisk_t;

//1.1 ����ṹ��: disk�Ľӿ�
typedef struct _xdisk_driver_t { //����: ���ض�д, ���¶��Ǻ���
	xfat_err_t(*open)(xdisk_t* disk, void* init_data); //����ֵ:������_xfat_err_t, ������:open, �ڶ�������init_data:��Ҫ�����ʼ���Ĳ���, �ṩ����Ӧ����δ򿪵���Ϣ 
	xfat_err_t(*close)(xdisk_t* disk);
	xfat_err_t(*read_sector)(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);//ÿ�ζ�, �����Կ�Ϊ��λ. buffer: ��ȡ����Ϣ�ŵ����buffer, start_sector: ��ʼ��������, count:����������
	xfat_err_t(*write_sector)(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);//ÿ��д, �����Կ�Ϊ��λ. buffer: ����Ϣ��bufferд��, start_sector: ��ʼд������, count:д��������
}xdisk_driver_t;

//1.3 �ĸ��ӿڵ�����(�����Ǵ�driver.c����, �ڴ˻������޸�)
xfat_err_t xdisk_open(xdisk_t* disk, const char* name, xdisk_driver_t* driver, void* init_data); //name: ��ʼ��disk������, driver: ����driver��ָ��
xfat_err_t xdisk_close(xdisk_t* disk);
xfat_err_t xdisk_read_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);
xfat_err_t xdisk_write_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);

//2.2 ֧�ַ��������ļ��, ���disk���ж��ٸ�����
xfat_err_t xdisk_get_part_count(xdisk_t* disk, u32_t* count); //��count����ַȡֵ:ȡ�ж��ٸ�����
#endif