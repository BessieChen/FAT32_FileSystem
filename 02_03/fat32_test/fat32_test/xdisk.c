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

//2.3 实现计算扩展分区有几个小碎片分区
static xfat_err_t xdisk_get_extend_part_count(xdisk_t* disk, u32_t start_sector, u32_t* count) //start_sector是指这个分区相对于整个disk,是第几个扇区
{
	int r_count = 0;

	//定义一个当前检测的扇区号
	u32_t ext_start_sector = start_sector;

	//定义一个分区表指针:
	mbr_part_t* part;

	//定义缓冲区
	u8_t* disk_buffer = temp_buffer; //使用了全局缓冲区

	//读取分区表
	do {
		
		//读取一个扇区, 注意, 每一轮while之后, start_sector都会更新, start_sector是新分区表相对于disk起点的位置
		int err = xdisk_read_sector(disk, disk_buffer, start_sector, 1); //从start_sector开始读, 读一个
		if (err < 0) return err;

		//因为这是一个扩展分区, 它存的第一个, 同样也是512字节(446+64分区表+2), 所以我们要看的是中间的分区表part_info;
		part = ((mbr_t*)disk_buffer)->part_info;
		//因为mbr结构中, 是mbr_part_t part_info[MBR_PRIMARV_PART_NR]; 所以part起始指向的是分区表的第一个分区配置
		//注意, 64字节的分区表中, 应该只有前两个配置可以使用
		//第一个配置的xxx成员, 记录了小碎片的起始位置
		//第二个配置的ysstem_id成员, 记录了是否还有下一个分区表
		
		if (part->system_id == FS_NOT_VALID) //这一句我感觉是保守的写法, 因为能走到这一步, 一般都说明第一个配置能指向一个小碎片
		{
			break;
		}

		r_count++;

		//接下来判断第二个配置, 所以++part
		if ((++part)->system_id != FS_EXTEND) //说明第二个配置为空, 不会再指向新的分区表
		{
			break;
		}

		//去往第二个配置说明的新分区表的位置: 这个分区表相对于disk起点的位置 + 新分区表相对于这个分区表的位置 = 新分区表相对于disk起点的位置
		start_sector = ext_start_sector + part->relative_sectors;
	
	} while (1);

	*count = r_count;
	return FS_ERR_OK;
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
	u8_t extend_part_flag = 0;//2.3 定义标记变量
	u8_t start_sector[4];	//2.3 定义扇区号, 因为4个分区配置表, 所以最多只有4个扩展分区,  一个扩展分区里面有多少个小碎片分区, 不确定

	for (i = 0; i < MBR_PRIMARV_PART_NR; i++, part++)
	{
		if (part->system_id == FS_NOT_VALID)
		{
			continue;//继续查找下一个
		}
		else if (part->system_id == FS_EXTEND) //2.3 判断是不是拓展分区
		{
			//将这个扩展分区的起始扇区号保存
			start_sector[i] = part->relative_sectors;//相对的扇区数: 在一个分区中, 某个扇区举例该分区的第一个扇区的相对位置
			//将对应的flag设置成1
			extend_part_flag |= 1 << i;
		}
		else
		{
			r_count++;
		}
	}

	//2.3 对扩展分区做处理, 回忆:4个分区配置表, 所以最多只有4个扩展分区, 一个扩展分区里面有多少个小碎片分区, 不确定
	if (extend_part_flag) {
		for (i = 0; i < MBR_PRIMARV_PART_NR; i++) {
			if (extend_part_flag & (1 << i)) {
				//说明第i个分区是扩展分区

				u32_t ext_count = 0;//将第i个分区里面有多少小碎片分区的数量记录
				err = xdisk_get_extend_part_count(disk, start_sector[i], &ext_count);
				if (err < 0) {
					return err;
				}

				r_count += ext_count;
			}
		}
	}
	*count = r_count; //真的是很久不写了, 因为count是地址, 所以*count是指向那个地址
	return FS_ERR_OK;
}



