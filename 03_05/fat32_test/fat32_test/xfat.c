#include "xfat.h"
#include "xdisk.h" //因为fat建立在disk之上
#include "stdlib.h" //因为有malloc()

//3.2 使用全局变量u8_t temp_buffer[512], 因为这里的temp_buffer在xdisk.c中, 所以我们要跨文件使用的话, 要么用#include "xdisk.h", 要么用extern, 否则链接器会报错,因为看到了两个全局变量都叫做temp_buffer
extern u8_t temp_buffer[512];

//3.3 宏定义, 之所以将这个函数设置成宏定义, 因为这个函数的功能简单, 用宏快一点
#define xfat_get_disk(xfat) ((xfat)->disk_part->disk) //也就是给我一个xfat结构, 找到它的fat32分区, 在从fat32分区找到它所在的disk的起始地址

//3.2 读取fat32分区的第二个部分: fat区, 把关键信息保留在xfat中
static xfat_err_t parse_fat_header(xfat_t* xfat, dbr_t* dbr) //这里传入了保留区dbr的信息, 因为dbr其实是包括了fat区和数据区的信息的. 注意, xfat中只有xfat->disk_part是拥有数据的, 其余都是传地址取值
{
	xdisk_part_t* xdisk_part = xfat->disk_part; //这个fat区是属于哪个fat32分区
	
	//给xfat存储关键信息:
	//3.3 初始化根目录的第一个簇号
	xfat->root_cluster = dbr->fat32.BPB_RootClus; //直接从fat32.xxx获得

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
	xfat->sec_per_cluster = dbr->bpb.BPB_SecPerClus;//todo
	xfat->cluster_byte_size = xfat->sec_per_cluster * dbr->bpb.BPB_BytesPerSec; //计算一个簇占用的字节数= 一个扇区占用的字节数 * 一个簇有多少个扇区


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

	//3.4 定义一个缓存, 放一个fat表
	xdisk_t* xdisk = xdisk_part->disk;
	xfat->fat_buffer = (u8_t*)malloc(xfat->fat_tbl_sectors * xdisk->sector_size); //一个fat表的扇区数 * 一个扇区多少字节
	//补充:
	/*
	数据区的簇:
		1. 一个簇是2kb, 一个扇区是512b, 所以一个簇是4个扇区
		2. 一个目录项是32b, 一个扇区最多可以有16个目录项, 一个簇最多可以有16*4 = 64个目录项
			1. 一个目录项对应着一个目录或者文件的信息
			2. 看来一个簇, 里面最多只能有64个文件/目录的信息, 太少了
			3. 如果需要新增, 需要我们使用其他簇
				1. 但是簇不是连续使用的, 一个簇号==10的簇, 它的下一个簇的簇号可能是99
				2. 那么在哪里记录这种连接的关系呢? 那就是在fat表的fat表项中记录
	fat表
		0. fat表的结构中有fat_buffer(里面是全部fat表项的信息)里面的4个字节, 这4字节叫做一个(fat表项), 它对应于数据区中的一个簇. 所以fat_buffer的有4n个字节, 说明数据区有n个簇(但是不代表这个簇被使用)
		1. 每4个字节, 代表着一个fat32表项, 所以整个fat_buffer像是一个数组, 每个元素是4字节
			1. 4字节的内容:
				1. 0000 0000, 说明这个4字节对应的那个数据区的簇, 是空的
				2. 0000 0002 到 ffff ffef, 说明这个4字节对应的簇, 下一个簇对应的fat_buffer的4字节的index是这个0000 0002 到 ffff ffef
					1. 之所以从2开始, 因为fat_buffer中的前8个字节, 也就是2 * 4字节, 是用作其他用途的
					2. 注意, 我们看4字节的时候, 前4bit(也就是前半个字节)是忽略不看的, 所以index的范围应该是从 000 0002 到 fff ffef
						说明fat_buffer中的代表簇的4字节有: (fff ffef - 2) + 1个
				3. ffff fff0 到 ffff fff6, 系统保留
				4. ffff fff7, 代表这个4字节对应的簇是坏的
				5. ffff fff8 -> ffff ffff 结束簇, 说明此时没有下一个簇了
			2. 注意: 我们说的4字节对应数据区的一个簇, 对应方式应该是: 按地址对应
				fat_buffer中的前2个4字节(共8字节), 是保留的, 所以(第一个代表簇的)4字节, index == 2(base 0)
				簇的簇号也是从2开始的
					所以: 
						4字节, 也就是fat表项在fat表中的index, 也就是簇在数据区中的index(也称作簇号)
						4字节里面的内容, 就是那个簇的信息: 是空, 坏了, 还是由下一个簇
	 */

	//3.4 读取参数xfat所代表的fat表
	err = xdisk_read_sector(xdisk, (u8_t*)xfat->fat_buffer, xfat->fat_start_sector, xfat->fat_tbl_sectors); //第3个参数: xfat所代表的的fat表的起始扇区(绝对位置)
	if (err < 0) return err;

	//


	return FS_ERR_OK;
}

//3.3 给定xfat结构的信息, 计算数据区中第cluster_no个簇的距离disk起始位置的扇区号
u32_t cluster_first_sector(xfat_t* xfat,u32_t cluster_no)
{
	//计算数据区的起始扇区号 = fat区的第一个扇区号(fat_start_sector) + fat区有几个fat表(fat_tbl_nr) * fat表有几个扇区(fat_tbl_sectors)
	u32_t data_start_sector = xfat->fat_start_sector + xfat->fat_tbl_nr * xfat->fat_tbl_sectors; //注意, 这里可能会有错. 因为老师的disk是使用镜像, 所以fat_start_sector刚好就是fat区的起始扇区(见line40: xfat->fat_start_sector = xdisk_part->start_sector + dbr->bpb.BPB_RsvdSecCnt; // 这个活动的fat表相对于disk起始位置, 是第几个扇区: 这个fat32分区的起始扇区号 + 保留区占了几个扇区(dbr->bpb.BPB_RsvdSecCnt)
	
	//参数cluster_no是指数据区中的第几个簇, 但是数据区的簇(在指导文档中)是从2开始索引的(因为0和1用于别的用处), 所以我们要向左偏移2个index
	u32_t sectors = (cluster_no - 2) * xfat->sec_per_cluster; //簇个数 * 簇中扇区数

	return data_start_sector + sectors;
}

//3.3 读取簇的内容
xfat_err_t read_cluster(xfat_t* xfat, u8_t* buffer, u32_t cluster, u32_t count) //xfat: 可以提供一些信息(例如哪个disk,哪个fat32分区..), buffer: 读取的结果存到buffer, 传地址取值, cluster: 从这个fat32分区的数据区的哪个簇号开始读取, count:连续读取几个簇
{
	xfat_err_t err;
	u32_t i;

	u8_t* curr_buffer = buffer; //curr_buffer: 语意是当前的buffer
	u32_t curr_sector = cluster_first_sector(xfat, cluster);//当前的扇区号, 初始化的时候, 值是第cluster个簇的第一个扇区号(距离disk起始位置的扇区号). 但是我们只有簇, 所以需要计算
	

	for (i = 0; i < count; i++) {
		
		//获取磁盘结构, 用宏定义
		xdisk_t* disk = xfat_get_disk(xfat);

		//读取
		err = xdisk_read_sector(disk, curr_buffer, curr_sector, xfat->sec_per_cluster); //哪个disk, 结果存到curr_buffer, 从扇区号curr_sector开始读, 读xfat->xx个扇区号(因为一个簇有多少个扇区, 就读多少个)
		if (err < 0) return err;


		//更新buffer的地址值, 然后簇号的值
		curr_buffer += xfat->cluster_byte_size; //注意, 这里是一个簇占了xx个字节, 所以地址就要+=xx个, 因为这个u8_t*, 是地址
		curr_sector += xfat->sec_per_cluster; //这里是需要加上一个簇的扇区数, 所以更新了新的扇区
	}
	return FS_ERR_OK;
}

//3.4 判断簇号是不是有效的. 如果cluster_no是ffff fff8 到 ffff ffff 这个值也是无效的, 因为这表示结束
int is_cluster_valid(u32_t cluster_no) {
	//只取低28位:
	cluster_no &= 0x0fffffff;

	//index的范围应该是从 000 0002 到 fff ffef, 在这个范围之外的就是非法的index(相当于簇号). 注意, <= 0fff ffef 就是 < 0fff ffff
	return (cluster_no < 0x0ffffff0) && (cluster_no >= 2);
}

//3.4 读取下一个簇号
xfat_err_t get_next_cluster(xfat_t* xfat, u32_t curr_cluster_no, u32_t* next_cluster) {
	//先判断当前的簇号是不是有效的
	if (is_cluster_valid(curr_cluster_no)) {
		cluster32_t* cluster32_buf = (cluster32_t*)xfat->fat_buffer; //所以fat_buffer的有4n\个字节, 说明数据区有n个簇. 因为cluster32_t联合体的大小是32bit, 也就是4个字节, 所以将fat_buffer里面的内容拆成了4字节4字节的
		*next_cluster = cluster32_buf[curr_cluster_no].s.next; //因为我们之前用的是指针cluster32_t* , 所以现在可以当成数组来使用, 我们的簇号就相当于这个数组的索引, 然后我们要的内容是32bit中的低28bit, 所以用.s.next
	}
	else { //当前簇号无效, 那么下一个簇号也无效
		*next_cluster = CLUSTER_INVALID; //见xfat.h
	}

	return FS_ERR_OK;
}
