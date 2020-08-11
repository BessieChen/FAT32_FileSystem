#include "xfat.h"
#include "xdisk.h" //因为fat建立在disk之上

//3.2 使用全局变量u8_t temp_buffer[512], 因为这里的temp_buffer在xdisk.c中, 所以我们要跨文件使用的话, 要么用#include "xdisk.h", 要么用extern, 否则链接器会报错,因为看到了两个全局变量都叫做temp_buffer
extern u8_t temp_buffer[512];


//3.2 读取fat32分区的第二个部分: fat区, 把关键信息保留在xfat中
static xfat_err_t parse_fat_header(xfat_t* xfat, dbr_t* dbr) //这里传入了保留区dbr的信息, 因为dbr其实是包括了fat区和数据区的信息的. 注意, xfat中只有xfat->disk_part是拥有数据的, 其余都是传地址取值
{
	xdisk_part_t* xdisk_part = xfat->disk_part; //这个fat区是属于哪个fat32分区
	
	//给xfat存储关键信息:
	xfat->fat_tbl_sectors = dbr->fat32.BPB_FATSz32; ////每个fat表占用多少个扇区. 因为dbr中存储了fat的信息

	//fat表是否有映射:
	/*
	1. 我们从 dbr->fat32.BPB_ExtFlag看出, 它一共2个字节, 8bits
	2. 第7个bit
		1. 如果是0, 说明是镜像(也就是全部fat表都一样, 这是为了备份, 因为有时候修改一个fat表遇到断电, 可以通过其他镜像恢复)
		2. 如果是1, 说明只有一个fat表在使用. 到底是哪一个在使用(活动状态)
			1. 查看第0-3bit
			2. 这4个bit可以代表16个值, 说明我们可以有16个fat表
			3. 这4个bit代表的0-15的值, 就是说明哪个fat表正在使用
				举例0011, 说明index==3的表正在使用
			注意:
				1. 我之前以为是那种bit的表示方法, 即0011表示第0个和第1个fat表在使用
				2. 因为这里已经说了, 只有一个fat表在使用, 所以这里不是看哪个bit为1, 而是4个bit代表的数字是多少
	*/
	if (dbr->fat32.BPB_ExtFlags & (1<<7) )//说明第7个bit的值是1, 说明不是镜像, 需要寻找是哪个fat表是活动的
	{
		u32_t table = dbr->fat32.BPB_ExtFlags & 0xF; //是0到15哪个表正在活动. 这里取最后4个bit, 其实我觉得这里可以设置成u8_t table;
		xfat->fat_start_sector = xdisk_part->start_sector + dbr->bpb.BPB_RsvdSecCnt + table * xfat->fat_tbl_sectors;// 这个活动的fat表相对于disk起始位置, 是第几个扇区: 这个fat32分区的起始扇区号 + 保留区占了几个扇区(dbr->bpb.BPB_RsvdSecCnt) + 这歌活动的fat表是第几个fat表 * 一个fat表占多少扇区 //注意,如果这个活动的fat表是第0个, 那么最后一项全为0, 这样也是从0开始index的好处
		xfat->fat_tbl_nr = 1; //只有一个fat表活动
	}
	else//说明是镜像, 全部fat表都活动
	{
		xfat->fat_start_sector = xdisk_part->start_sector + dbr->bpb.BPB_RsvdSecCnt; // 这个活动的fat表相对于disk起始位置, 是第几个扇区: 这个fat32分区的起始扇区号 + 保留区占了几个扇区(dbr->bpb.BPB_RsvdSecCnt)
		xfat->fat_tbl_nr = dbr->bpb.BPB_NumFATs; //其实bpb里面就记录了有多少个fat表
	}

	xfat->total_sectors = dbr->bpb.BPB_TotSec32; //全部fat表一共占了多少扇区, 这里我们直接调用bpb的诗句

	return FS_ERR_OK;
}

//3.2 定义一个函数, 实现fat区的打开功能, todo: 之前我在xdisk_part_t结构体的注释是fat32的分区, 也就是那4个分区配置的指向的四个分区的其中一个
xfat_err_t xfat_open(xfat_t* xfat, xdisk_part_t* xdisk_part)//xdisk_part: 打开xdisk_part这个fat32分区, xfat是传地址取值
{
	//3.2 回忆:
	/*
	1. disk
	2. _mbr_t是disk前512字节的mbr
	2. _mbr_part_t是mbr中的4个分区配置
	3. _mbr_part_t->relative_sector指向了第一个fat32分区, 这里的相对位置指的是fat32区举例mbr的位置(如果这个fat32分区是普通的分区,那么这个相对位置就是绝对位置因为mbr就是从第0块开始的. 如果这个fat32分区是扩展分区,那么扩展分区的第一个块将还是一个mbr, 那么这个xx->relative_sector应该是这个mbr距离我们全局mbr的位置, 然后之后这个mbr中也有两个分区配置, 其中第一个分区配置的relative_sector也就是小碎片分区距离这个mbr的相对距离, 如果计算小碎片在disk中的绝对距离, 还需要加上这个mbr距离全局mbr的距离)
	4. 这个fat32分区的前xx字节是保留区, 用dbr_t结构体表示
		1. 注意这个dbr中已经包含了: 1. bios参数块 2. fat区的一些信息(注意, 这个要区别于xfat_t结构, 见第6点)
	5. fat32分区接下来是我们的fat区, 最后是数据区
	6. 注意我们参数的xfat_t仅仅是为了我们提取fat区的关键数据, 并不映射到disk上
	*/

	xdisk_t* disk = xdisk_part->disk; //我们的四个fat32分区,保留了属于哪个disk
	dbr_t* dbr = (dbr_t*)temp_buffer;//将全局变量, 当做是保留区dbr使用
	xfat->disk_part = xdisk_part; //记录这个存放关键信息的xfat到底是属于哪个fat32分区, 这个回用于parse_fat_header()中
	xfat_err_t err;


	//首先,我们要读fat32分区的第一个部分: 保留区, 并保存到dbr_t*中, 这里是传地址存值
	err = xdisk_read_sector(disk, (u8_t*)dbr, xdisk_part->start_sector, 1); //也就是在disk中读取, 起始地址是xdisk_part->start_sector, 读一个扇区, 这个扇区就是我们的dbr保留区
	if (err < 0) return err;

	//上一步是读取保留区, 把信息存到dbr中. 这里是将dbr中关于fat的信息, 存到xfat中, 因为xfat存的是fat区的关键信息
	err = parse_fat_header(xfat, dbr);
	if (err < 0) return err;

	return FS_ERR_OK;
}


//static xfat_err_t parse_fat_header(xfat_t * xfat, dbr_t * dbr) {
//	xdisk_part_t * xdisk_part = xfat->disk_part;
//
//	// 解析DBR参数，解析出有用的参数
//	xfat->fat_tbl_sectors = dbr->fat32.BPB_FATSz32;
//
//	// 如果禁止FAT镜像，只刷新一个FAT表
//	// disk_part->start_block为该分区的绝对物理扇区号，所以不需要再加上Hidden_sector
//	if (dbr->fat32.BPB_ExtFlags & (1 << 7)) {
//		u32_t table = dbr->fat32.BPB_ExtFlags & 0xF;
//		xfat->fat_start_sector = dbr->bpb.BPB_RsvdSecCnt + xdisk_part->start_sector + table * xfat->fat_tbl_sectors;
//		xfat->fat_tbl_nr = 1;
//	}
//	else {
//		xfat->fat_start_sector = dbr->bpb.BPB_RsvdSecCnt + xdisk_part->start_sector;
//		xfat->fat_tbl_nr = dbr->bpb.BPB_NumFATs;
//	}
//
//	xfat->total_sectors = dbr->bpb.BPB_TotSec32;
//
//	return FS_ERR_OK;
//}
//
///**
//* 初始化FAT项
//* @param xfat xfat结构
//* @param disk_part 分区结构
//* @return
//*/
//xfat_err_t xfat_open(xfat_t * xfat, xdisk_part_t * xdisk_part) {
//	dbr_t * dbr = (dbr_t *)temp_buffer;
//	xdisk_t * xdisk = xdisk_part->disk;
//	xfat_err_t err;
//
//	xfat->disk_part = xdisk_part;
//
//	// 读取dbr参数区
//	err = xdisk_read_sector(xdisk, (u8_t *)dbr, xdisk_part->start_sector, 1);
//	if (err < 0) {
//		return err;
//	}
//
//	// 解析dbr参数中的fat相关信息
//	err = parse_fat_header(xfat, dbr);
//	if (err < 0) {
//		return err;
//	}
//
//	return FS_ERR_OK;
//}