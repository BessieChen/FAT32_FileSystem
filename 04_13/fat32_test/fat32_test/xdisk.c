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

//2.4 获取扩展分区中的信息
static xfat_err_t xdisk_get_extend_part(xdisk_t* disk, xdisk_part_t* disk_part, u32_t start_sector, int part_no, u32_t* count)//disk是我们给的信息, disk_part是传地址取值获得我们要的小碎片的信息, start_sector:这个扩展分区相对于disk起始部位的位置,  part_no我们要的是第几个小碎片, count传地址取值获得://所以这里的count是指, 找到我们要的小碎片后, 已经看过了几个小碎片(包括要看的)
{
	
	u8_t* disk_buffer = temp_buffer;//用全局缓存
	xfat_err_t err = FS_ERR_OK;
	u32_t ext_start_sector = start_sector;//因为之后start_sector的值会经常变, 所以这里保留一个初始值. start_sector之后的语意是: 512字节的mbr(会指向某个小碎片)相对于整个disk的位置
	int curr_no = -1; //这个指的是第几个小锁片

	//遍历
	do {
		//既然知道小碎片的位置, 先把结果读到buffer中
		err = xdisk_read_sector(disk, disk_buffer, start_sector, 1);//只读一个扇区, 也就是前512字节

		//这512字节中, 包括了我们要的64字节分区表, 我们只关注:第一个分区配置, 第二个分区配置
		mbr_part_t* part;
		part = ((mbr_t*)disk_buffer)->part_info;//此时part就指向了第一个分区配置的首地址

		//回忆: 第一个分区配置的relative_sector指向了小碎片(注意: 小碎片就被当做一个分区). 第二个分区配置的relative_sector指向了下一个512字节的mbr
		//分区配置中: 我们先判断, 第一个分区配置有没有指向小碎片
		if (part->system_id == FS_NOT_VALID)//说明没有指向小碎片, 说明已经结束了, 或者像之前说的, 这是一个保守的写法, 一般走到这一步, 都是能够保证有小碎片的
		{
			break;
		}

		//看这个小碎片是不是用户要的. 例如, 参数part_no是用户想要的第0123个小碎片, 然后curr_no是当前走到第几个小碎片, 所以看两者是否相等
		if (++curr_no == part_no) {
			disk_part->type = part->system_id;
			disk_part->start_sector = start_sector + part->relative_sectors; //todo, start_sector应该是当前指向小碎片那块512字节相对于我们disk的位置(因为start_sector是一直累加的), 然后relative_sector是小碎片相对于那块512字节的位置, 所以整个是小碎片相对于disk的位置
			disk_part->total_sector = part->total_sectors;
			disk_part->disk = disk;
			break;//既然找到了, 退出while
		}

		//走到这里, 说明虽然找到一个小碎片, 但是不是我们要的小碎片, 我们继续往下
		//往下走, 首先要找到下一个块512字节的mbr, 这个就是第二个分区配置(++part)指向的地方
		if ((++part)->system_id != FS_EXTEND)//如果没有下一块mbr了,退出
		{
			err = FS_ERR_EOF;//end of file
			break;
		}

		//有下一块mbr,我们要获取这个mbr的地址
		start_sector = ext_start_sector + part->relative_sectors; //第一个mbr的首地址相对于disk的位置 + 下一个mbr相对于这个mbr的位置(注意, 这里的part已经指向了第二个分区配置)
	}while(1);

	*count = curr_no + 1;//所以这里的count是指, 找到我们要的小碎片后, 已经看过了几个小碎片(包括要看的)
	return err;
}

//2.4 获取分区信息
xfat_err_t xdisk_get_part(xdisk_t* disk, xdisk_part_t* xdisk_part, int part_no)
{
	u8_t* disk_buffer = temp_buffer;
	mbr_part_t* mbr_part;
	int i;
	int curr_no = -1; //检测到了分区的个数

	//读取mbr
	int err = xdisk_read_sector(disk, disk_buffer, 0, 1);
	if (err < 0) return err;

	//解析中间64字节的分区表
	mbr_part = ((mbr_t*)disk_buffer)->part_info; //将会指向第一个分区配置

	//遍历: 第234个分区配置
	for (i = 0; i < MBR_PRIMARV_PART_NR; i++, mbr_part++) {
		//无效就看下一个
		if (mbr_part->system_id == FS_NOT_VALID) {
			continue;
		}

		//判断是不是扩展分区
		if (mbr_part->system_id == FS_EXTEND) {
			//扩展分区中有几个碎片分区
			u32_t count = 0;

			err = xdisk_get_extend_part(disk, xdisk_part, mbr_part->relative_sectors, part_no - i, &count);//传入的disk是信息, xdisk_part是传地址取值, mbr..是说明起始位置, part_no - i(假设求的扇区数part_no是3, 实际的是0, 1, (2,3), 4, 所以i = 2代表(2,3), 所以也就是求这个扩展分区的第3-2=1个碎片分区, count:传地址取值)
			if (err < 0) return err;

			if (err == FS_ERR_OK)//说明我们要的part_no分区的确在扩展分区i中
			{
				return err;
			}
			else 
			{
				curr_no += count;//todo

				//老师说, xdisk_get_extend_part()会修改disk_buffer, 让他差异于temp_buffer. 所以要重新读取
				err = xdisk_read_sector(disk, disk_buffer, 0, 1);
				if (err < 0) return err;

			}
		}
		else {
			//计数: 当前分区号
			if (++curr_no == part_no) //认为:找到分区了
			{
				xdisk_part->type = mbr_part->system_id; //之所以可以这样, 因为system_id是01数字, 然后我们在xfs_type_t中已经给定了01数字对应的英文是什么
				xdisk_part->start_sector = mbr_part->relative_sectors; //起始扇区
				xdisk_part->total_sector = mbr_part->total_sectors;
				xdisk_part->disk = disk;
				return FS_ERR_OK;
			}
		}
	}

	//如果走到了这里, 说明我们要的分区没有找到
	return FS_ERR_NONE;
}






