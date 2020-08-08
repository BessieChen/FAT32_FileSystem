#include "xdisk.h"

u8_t temp_buffer[512]; //2.2 暂时用于存放启动硬盘的前512字节

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

//2.2 获取有多少个分区, 其实就是看分区表里4个分区配置的信息哪些是有效地
xfat_err_t xdisk_get_part_count(xdisk_t * disk, u32_t * count)
{
	int r_count = 0, i = 0;
	
	//存4个分区配置:
	mbr_part_t* part;
	u8_t* disk_buffer = temp_buffer;//这个temp_buffer是全局变量, 其他文件也可以用, 当做专门存开头512字节的

	//读取disk的前512字节, 存到disk_buffer指向的temp_buffer
	int err = xdisk_read_sector(disk, disk_buffer, 0, 1); //这里的0是从第0个扇区开始读, 1是只读一个扇区, 因为我们设置了一个扇区就是512字节, 所以刚刚好
	if (err < 0) return err;

	//因为disk_buffer是第0个扇区的首地址, 所以可以知道其中的分区表Part_info
	part = ((mbr_t*)disk_buffer)->part_info;
	//解释: 1. disk_buffer是一个指针, 但是我们现在认为他不是u8_t指针, 而是mbr_t指针, 所以可以用->去获取到成员part_info, 之前定义的mbr_part_t* part, 对应的是成员mbr_part_t part_info[MBR_PRIMARV_PART_NR];(见xdisk.h)

	//查找每一个分区设置, 看system_id是否合法, 合法就说明多一个分区. 我喜欢的写法: part++, 因为之前是mbr_part_t part_info[], 是这个类型的数组, 所以可以用part++
	
	u8_t hi[2];
	u8_t* hi2 = hi;
	hi2 = ((mbr_t*)disk_buffer)->boot_sig;

	for (i = 0; i < MBR_PRIMARV_PART_NR; i++, part++)
	{
		if (part->system_id == FS_NOT_VALID)
		{
			continue;//继续查找下一个
		}
		else
		{
			r_count++;
		}
	}
	*count = r_count; //真的是很久不写了, 因为count是地址, 所以*count是指向那个地址
	return FS_ERR_OK;
}



