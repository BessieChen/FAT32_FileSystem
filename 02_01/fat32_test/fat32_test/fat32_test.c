#include<stdio.h>
#include "xdisk.h"
#include "xfat.h" //������xtypes.h��
#include <string.h> //������memset(), memcmp()

//1.2 ����: ���������vdisk��ʵ����extern���ù���,
extern xdisk_driver_t vdisk_driver;
//1.2 ����һ�����Դ��̵�·��
const char* disk_path_test = "disk_test.img"; //���·��
//1.2 ���弸��������, ���Զ�д
static u32_t write_buffer[160 * 1024]; //160kb
static u32_t read_buffer[160 * 1024];
//1.2 ������Ժ���, �����ĸ��ӿ�:���ض�д
int disk_io_test(void) {
	xfat_err_t err;//������
	
	//������̽ṹ
	xdisk_t disk_test;

	//1.3 ɾdisk_test.driver = &vdisk_driver; //�������������Ϊdisk_test������driver

	//����ȡ����read_buffer����
	memset(read_buffer, 0, sizeof(read_buffer));

	//1.3 ��, ���������Ĵ򿪽ӿ�
	err = xdisk_open(&disk_test, "vdisk_test", &vdisk_driver, (void*)disk_path_test);//����������, ����vdisk_driver��·�� //1.3 ɾerr = disk_test.driver->open(&disk_test, disk_path_test);

	if (err) { //��Ϊ��, ����ʧ��
		printf("open disk failed.\n");
		return -1;
	}

	//1.3 ��, ����д�Ľӿ�
	err = xdisk_write_sector(&disk_test, (u8_t*)write_buffer, 0, 2); //��0������ʼ, д2������, �ǵ���, һ�������Ĵ�С��512(�ֽ�?)
	if (err) {
		printf("write disk failed.\n");
		return -1;
	}

	//1.3 ��, ����:��
	err = xdisk_read_sector(&disk_test, (u8_t*)read_buffer, 0, 2); //��0������ʼ, д2��
	if (err) {
		printf("read disk failed.\n");
		return -1;
	}

	//�Ƚ�����������, �Ƚϵķ�Χ:2������
	err = memcmp((u8_t*)read_buffer, (u8_t*)write_buffer, 2 * disk_test.sector_size);
	if (err)
	{
		printf("data not equal.\n");
		return -1;
	}

	//1.3 ��, ���ر�
	err = xdisk_close(&disk_test); 
	if (err) {
		printf("close disk failed.\n");
		return -1;
	}

	printf("disk io test fine. \n");
	return 0;

	//todo:
	/*1. Ϊʲô��(u8_t) 2. д2����ʲô��λ: ����*/
}


//2.1 ��Ӳ��ԵĽṹ��
#pragma pack(1)
typedef struct _mbr_t {
	u32_t a;
	u16_t b;
	u32_t c;
}mbr_t;
#pragma pack() //ע��, ��������ʲô��û��

int main(void)
{
	//write����ĳ�ʼ��
	for (int i = 0; i < sizeof(write_buffer) / sizeof(u32_t); i++) {
		write_buffer[i] = i;
	}

	/*xfat_err_t err;
	err = disk_io_test();
	if (err)
	{
		return err;
	}*/

	//2.1 ����
	mbr_t* mbr = (mbr_t*)0x100; //����һ����ַ0x100, �����ַ��������mbr_t�ṹ��ĵ�һ����Աu32_t a;
	printf("%p\n", &(mbr->c)); //��%p��ӡ��ַ, ���û��pragma pack(1), ����0000 0108, �������0000 0106

	printf("Test End");
	return 0;
}