#ifndef XDISK_H
#define XDISK_H

#include "xtypes.h"

//1.1 ǰ������: ���߱�����, ���struct�Ѿ�������, �����ڵ�line14
struct _xdisk_driver_t;

//1.1 ����ṹ��: ��������disk(���������disk����Ϣ, �Լ�disk�Ĳ���[ͨ���ӿڽṹ���ָ��ʵ��])
typedef struct _xdisk_t
{
	u32_t sector_size;//ÿ��disk��Ĵ�С(������С)
	u32_t total_sector;//һ����������
	struct _xdisk_driver_t* driver;//��disk�ӿ�����ṹ���ָ����� //������õ���ǰ������
	void* data; //1.2 ���������͵����ݴ�������, �൱��һ��������
}xdisk_t;

//1.1 ����ṹ��: disk�Ľӿ�
typedef struct _xdisk_driver_t { //����: ���ض�д, ���¶��Ǻ���
	xfat_err_t(*open)(xdisk_t* disk, void* init_data); //����ֵ:������_xfat_err_t, ������:open, �ڶ�������init_data:��Ҫ�����ʼ���Ĳ���, �ṩ����Ӧ����δ򿪵���Ϣ 
	xfat_err_t(*close)(xdisk_t* disk);
	xfat_err_t(*read_sector)(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);//ÿ�ζ�, �����Կ�Ϊ��λ. buffer: ��ȡ����Ϣ�ŵ����buffer, start_sector: ��ʼ��������, count:����������
	xfat_err_t(*write_sector)(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count);//ÿ��д, �����Կ�Ϊ��λ. buffer: ����Ϣ��bufferд��, start_sector: ��ʼд������, count:д��������
}xdisk_driver_t;

//1.1 ������Ҫע��ĵ�
/*
1. ��Ϊstruct _xdisk_t�� struct _xdisk_driver_t�ڲ����жԷ��Ķ���
	1. ��Ϊ�������ȶ�����struct _xdisk_t, �����ں���struct _xdisk_driver_t����ǰ�ߵ�typedef, �������ǿ�������ֱ��д(xdisk_t* disk);
	2. ��Ϊstruct _xdisk_t��ǰ��, ����֪��struct _xdisk_driver_t�Ķ���, ������Ҫǰ������: ����line7��struct _xdisk_driver_t;; ����ǰ��
2. ����xfat_err_t, ��Ϊ����ṹ��xfat_err_t�Ѿ���#include "xtypes.h"������, ����line19-22����xfat_err_t��ȫû����
*/
#endif