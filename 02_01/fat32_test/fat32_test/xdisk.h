#ifndef XDISK_H
#define XDISK_H

#include "xtypes.h"

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

#endif