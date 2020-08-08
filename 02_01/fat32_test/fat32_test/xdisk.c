#include "xdisk.h"

//1.3 �ĸ��ӿڵ�����(�����Ǵ�driver.c����, �ڴ˻������޸�)
xfat_err_t xdisk_open(xdisk_t* disk, const char* name, xdisk_driver_t* driver, void* init_data) //name: ��ʼ��disk������, driver: ����driver��ָ��
{
	xfat_err_t err;
	disk->driver = driver; //disk�ĳ�ʼ��

	err = disk->driver->open(disk, init_data);
	if (err < 0) {
		return err;
	}

	disk->name = name;
	return FS_ERR_OK;
}

//1.3
xfat_err_t xdisk_close(xdisk_t* disk)
{
	xfat_err_t err;
	err = disk->driver->close(disk);
	if (err < 0)
		return err;
	return err;
}

//1.3
xfat_err_t xdisk_read_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count)
{
	xfat_err_t err;

	//��ȡ�����յ�����, �Ƿ񳬹��˱߽�
	if (start_sector + count >= disk->total_sector) return FS_ERR_PARAM;

	//���û����, ����������driver�Ľӿ�
	err = disk->driver->read_sector(disk, buffer, start_sector, count);
	return err;
}

//1.3
xfat_err_t xdisk_write_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count)
{
	xfat_err_t err;

	//��д�������, �Ƿ񳬹��˱߽�
	if (start_sector + count >= disk->total_sector) return FS_ERR_PARAM;

	//���û����, ����������driver�Ľӿ�
	err = disk->driver->write_sector(disk, buffer, start_sector, count);
	return err;
}



