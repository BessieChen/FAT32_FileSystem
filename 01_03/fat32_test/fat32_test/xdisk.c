#include "xdisk.h"

//1.3 四个接口的声明(这里是从driver.c来的, 在此基础上修改)
xfat_err_t xdisk_open(xdisk_t* disk, const char* name, xdisk_driver_t* driver, void* init_data) //name: 初始化disk的名字, driver: 传入driver的指针
{
	xfat_err_t err;
	disk->driver = driver; //disk的初始化

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

	//读取的最终的扇区, 是否超过了边界
	if (start_sector + count >= disk->total_sector) return FS_ERR_PARAM;

	//如果没出错, 调用驱动层driver的接口
	err = disk->driver->read_sector(disk, buffer, start_sector, count);
	return err;
}

//1.3
xfat_err_t xdisk_write_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count)
{
	xfat_err_t err;

	//被写入的扇区, 是否超过了边界
	if (start_sector + count >= disk->total_sector) return FS_ERR_PARAM;

	//如果没出错, 调用驱动层driver的接口
	err = disk->driver->write_sector(disk, buffer, start_sector, count);
	return err;
}



