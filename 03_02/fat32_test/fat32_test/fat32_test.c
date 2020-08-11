#include<stdio.h>
#include "xdisk.h"
#include "xfat.h" //������xtypes.h��
#include <string.h> //������memset(), memcmp()

//1.2 ����: ���������vdisk��ʵ����extern���ù���,
extern xdisk_driver_t vdisk_driver;

//2.2 ����һ�����Դ��̵�·��
const char* disk_path = "disk.img";

//2.2 ����һ��disk
xdisk_t disk;
//3.1 ����һ��disk part���������Ϣ
xdisk_part_t  disk_part;
//3.1 ����buffer
static u32_t read_buffer[160 * 1024]; //���Դ�160*1024���ֽڵĶ���, �൱��160MB
//3.2 ����xfat, ����洢��fat��Ĺؼ���Ϣ
xfat_t xfat;

//2.4 �޸�: ��ȡÿ����������Ϣ //2.2 ������ĺ���
int disk_part_test(void) {
	u32_t count, i;
	xfat_err_t err;

	printf("partition read test...\n");

	//���ü�����������ĺ���
	err = xdisk_get_part_count(&disk, &count);
	if (err < 0) {
		printf("partition count detect failed.\n");
		return err;
	}

	//2.4 �����еķ�����Ϣ���ó���
	for (i = 0; i < count; i++) {
		xdisk_part_t part;

		int err;

		//��ʼ��ÿһ������(����С��Ƭ)����Ϣ�浽part��
		err = xdisk_get_part(&disk, &part, i);//˵������Ҫ��i������
		if (err < 0) {
			printf("read partition failed.\n");
			return -1;
		}

		printf("no: %d, start sector: %d, total sector: %d, capcity:%.0f M\n",
			i, part.start_sector, part.total_sector, part.total_sector * disk.sector_size / 1024 / 1024.0);

	}

	printf("partition count:%d\n", count);
	return 0;
}

int main(void)
{
	xfat_err_t err;
	
	//2.2 ��disk�ļ�
	err = xdisk_open(&disk, "vdisk", &vdisk_driver, (void*)disk_path);
	if (err) {
		printf("open disk failed.\n");
		return -1;
	}

	//2.2 �򿪺�, 
	err = disk_part_test();//���öϵ�
	if (err) return err;

	//3.1 ��ȡ��0������(��һ����������512�ֽڵ�mbr)����Ϣ. ע��, ��ʵ����disk_part���潫�ᱣ��ָ���FAT32���ֵ���ʼ��ַ, ��disk_part.start_sector
	err = xdisk_get_part(&disk, &disk_part, 1); //��ȡ��0��
	if (err < 0) {
		printf("read partition failed!\n");
		return -1;
	}

	//3.2 ����
	err = xfat_open(&xfat, &disk_part);
	if (err < 0) return err;

	//2.2 �ر�
	err = xdisk_close(&disk);
	if (err) {
		printf("close disk failed.\n");
		return -1;
	}

	printf("Test End");
	return 0;
}