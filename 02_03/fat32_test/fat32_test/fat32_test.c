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

//2.2 ������ĺ���
int disk_part_test(void) {
	u32_t count;
	xfat_err_t err;

	printf("partition read test...\n");

	//���ü�����������ĺ���
	err = xdisk_get_part_count(&disk, &count);
	if (err < 0) {
		printf("partition count detect failed.\n");
		return err;
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

	//2.2 �ر�
	err = xdisk_close(&disk);
	if (err) {
		printf("close disk failed.\n");
		return -1;
	}

	printf("Test End");
	return 0;
}