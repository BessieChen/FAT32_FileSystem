#include "xfat.h"
#include "xdisk.h" //因为fat建立在disk之上
#include "stdlib.h" //因为有malloc()
#include <string.h> //memcmp()
#include <ctype.h> //toupper()

//3.2 使用全局变量u8_t temp_buffer[512], 因为这里的temp_buffer在xdisk.c中, 所以我们要跨文件使用的话, 要么用#include "xdisk.h", 要么用extern, 否则链接器会报错,因为看到了两个全局变量都叫做temp_buffer
extern u8_t temp_buffer[512];

//3.3 宏定义, 之所以将这个函数设置成宏定义, 因为这个函数的功能简单, 用宏快一点
#define xfat_get_disk(xfat) ((xfat)->disk_part->disk) //也就是给我一个xfat结构, 找到它的fat32分区, 在从fat32分区找到它所在的disk的起始地址
//4.2 判断ch是否是分隔符: '/'或者 '\'
#define is_path_sep(ch)		( ((ch) == '\\') ||  ((ch) == '/') ) //其中\\是反斜杠/, 
//4.2 
#define to_sector(disk, offset) ((offset) / (disk)->sector_size) //其中sector_size指的是disk中每个扇区的大小(字节数), 我们的例子中是512字节 ////开始找的地方, 是簇里面的第几个扇区, 使用宏函数
#define to_sector_offset(disk, offset)  ((offset) % (disk)->sector_size) //开始找的地方, 是这个扇区的具体哪个字节
//4.4 
#define to_cluster_offset(xfat, pos) ((pos) % (xfat->cluster_byte_size)) //pos位置, 相对于簇内部的偏移位置

//3.2 读取fat32分区的第二个部分: fat区, 把关键信息保留在xfat中
static xfat_err_t parse_fat_header(xfat_t* xfat, dbr_t* dbr) //这里传入了保留区dbr的信息, 因为dbr其实是包括了fat区和数据区的信息的. 注意, xfat中只有xfat->disk_part是拥有数据的, 其余都是传地址取值
{
	xdisk_part_t* xdisk_part = xfat->disk_part; //这个fat区是属于哪个fat32分区
	
	//给xfat存储关键信息:
	//3.3 初始化根目录的第一个簇号
	xfat->root_cluster = dbr->fat32.BPB_RootClus; //直接从fat32.xxx获得

	xfat->fat_tbl_sectors = dbr->fat32.BPB_FATSz32; //每个fat表占用多少个扇区. 因为dbr中存储了fat的信息

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

//4.2 跳过路径的分隔符: '/'
static const char* skip_first_path_sep(const char* path) {
	const char* c = path;
	
	while(is_path_sep(*c))//如果c是一个分隔符. 其中is_path_sep是一个宏函数
	{
		c++; //如果当前c指向的是分隔符, 就跳到分隔符的下一个
	}
	return c;//当前c指向的就不是分隔符
}

//4.2 这个函数的目标是获得子目录, 例如传入的是/aa/bb/cc.txt, 那么返回值是bb/cc.txt, 注意,最前面都是不/, 而是aa/.., bb/...
static const char* get_child_path(const char* dir_path) {
	
	const char* c = skip_first_path_sep(dir_path);	//假设我们传入的dir_path的值是/a/b/c.txt, 那么在运行完这一句(剥离分隔符)之后,就会变成a/b/c.txt

	//接下来是从a/b/c.txt变成/b/c.txt
	while ((*c != '\0') && !is_path_sep(*c)) //因为现在c指向了a/b/c.txt中的a(非分隔符), 所以要进入.//首先看, c是不是到达了文件结束(保守起见), 之后我们是不是一般的符号aa,bb
	{
		c++;//往下继续走
	}

	//退出while的时候, c要么是指向了\0, 要么是指向了一个分隔符
	return (*c != '\0') ? c + 1 : (const char*)0; //如果是指向分隔符, 那就指向下一位(这个下一位一定是一个非分隔符, 因为分隔符只占1位), 如果是结束符, 那就返回空
}

//4.2 获取文件属性
static xfile_type_t get_file_type(const diritem_t* diritem) {
	xfile_type_t type;

	u8_t attr = diritem->DIR_Attr;
	if (attr & DIRITEM_ATTR_VOLUME_ID) //是不是卷标
	{
		type = FAT_VOL;
	}
	else if (attr & DIRITEM_ATTR_DIRECTORY) //目录
	{
		type = FAT_DIR;
	}
	else {
		type = FAT_FILE;
	}

	return type;
}

//4.2 获取文件的簇号, 高16位和低16位组合
static u32_t get_diritem_cluster(diritem_t* item) {
	return (item->DIR_FstClusHI << 16) | item->DIR_FstClusL0;
}

//4.3 文件名转换: 将我们的文件名123TXT转换成8+3文件名. 其中最后一个参数01是大小写的配置(0代表配置: 文件名, 扩展名都为大写)
static xfat_err_t to_sfn(char* dest_name, const char* source_name, u8_t case_config) { //不过老师说case_config 没有用到, 我们假设默认的大写
	int name_len; //文件名长度
	const char* ext_dot; //分隔符
	const char* p;	//遍历指针
	char* dest = dest_name;
	
	int ext_existed;//扩展名是否存在

	//将dest_name全部清空, 不要填成0, 而是填空格
	memset(dest_name, ' ', SFN_LEN);

	//跳过反斜杠, 因为上一层来的时候, 可能会有 aa/bb/cc/123.txt
	while (is_path_sep(*source_name)) {
		source_name++;
	}
	//出来while之后, source_name指向的那个字符, 会是非反斜杠

	//判断点分隔符在哪里, 把文件名和扩展名分割开来
	ext_dot = source_name; //初始化位my_name的开头, 注意source_name是指针
	p = source_name;  //p用于遍历整个名字
	name_len = 0;

	//开始遍历, 老师说, 低一级的路径, 不在这个while里面比较, 所以遇到分隔符就跳出
	//总之: 这个while就是将/xxx/中间的目录名xxx, 或者/xxx.txt的文件名取出来, 都是不包括/的
	while ((*p != '\0') && !is_path_sep(*p)) 
	{
		if (*p == '.') {
			ext_dot = p;// 让扩展名分隔符指向p
		}

		p++;
		name_len++; //长度的计数++
	}

	ext_existed = (ext_dot > source_name) && (ext_dot < (source_name + name_len - 1)); //如果这个点, 大于文件名的开头, 小于文件名的结束

	p = source_name; //指向source_name的开头
	int i;
	for (i = 0; (i < SFN_LEN) && (*p != '\0') && !is_path_sep(*p); i++) {
		if (ext_existed) {
			//有文件名 + 扩展名
			if (p == ext_dot) {
				dest = dest_name + 8;
				p++;
				i--;
				continue;
			}
			else if (p < ext_dot) {
				*dest++ = toupper(*p++);
			}
			else {
				*dest++ = toupper(*p++);
			}
		}
		else {
			//只有目录名
			*dest++ = toupper(*p++); //就将p指向的内容直接加入就好了
		}
	}

	return FS_ERR_OK;

}
//4.3 修改 //4.2 文件名的匹配
static u8_t is_filename_match(const char* name_in_dir, const char* target_name) {
	//4.3 转换后的文件名
	char temp_name[SFN_LEN]; //长度为8的字符数组

	//4.3 tiny test case
	char* path = "open.txt";
	to_sfn(temp_name, path, 0);

	to_sfn(temp_name, target_name, 0); //将我们的文件名123TXT转换成8+3文件名. 其中0是大小写的配置(0代表配置: 文件名, 扩展名都为大写) //这里比较的是, 转换后的名字: temp_Name, 和我们想要的名字 target_name比较


	return memcmp(name_in_dir, temp_name, SFN_LEN) == 0;//短文件名一共11字节的比较
}

//4.5 判断当前目录项的类型是不是我们想要的. 老师的思路: 先认为你是match的, 一旦发现你的attr不是我要的, 我就认为你不match
//老师为什么不: 先认为它不match, 后认为match. 其实我这个先不Match后matach认为应该也是可以的, 
static u8_t is_locate_match(diritem_t* dir_item, u8_t locate_type) {
	u8_t match = 1;
	//if (locate_type & XFILE_LOCATE_ALL) return match; //bug! 不能这么写!, 因为这样子无论locate_type是什么, 都可以通过.

//宏定义
#define	DOT_FILE		".          " //char[12], 也就是8+3+一个结束符
#define	DOT_DOT_FILE	"..         " //char[12], 也就是8+3+一个结束符

	if ((dir_item->DIR_Attr & DIRITEM_ATTR_SYSTEM) && !(locate_type & XFILE_LOCATE_SYSTEM)) { //实际是系统文件 && 我不要系统文件
		match = 0;
	}
	else if ((dir_item->DIR_Attr & DIRITEM_ATTR_HIDDEN) && !(locate_type & XFILE_LOCATE_HIDDEN)) { //实际是XX && 我不要XX
		match = 0;
	}
	else if ((dir_item->DIR_Attr & DIRITEM_ATTR_VOLUME_ID) && !(locate_type & XFILE_LOCATE_VOL)) {
		match = 0;
	}
	else if ((memcmp(DOT_FILE, dir_item->DIR_Name, SFN_LEN) == 0) //发现dir_name是一个点
		|| (memcmp(DOT_DOT_FILE, dir_item->DIR_Name, SFN_LEN) == 0) )   //发现dir_name是一个点点
	{
		if(!(locate_type & XFILE_LOCATE_DOT)) //如果我们不要点/点点, 不match
			match = 0;
	}
	else if (!(locate_type & XFILE_LOCATE_NORMAL)) //剩下的说明dir_name是普通文件(可读, 可写, 长文件名...), 如果我们不要普通文件, 同样不match
	{
		match = 0;
	}
	return match;
}

//4.5 修改: 只返回指定的文件(通过标志位) //4.2 这个函数的实现有点复杂. //从第dir_cluster个簇开始找, 开始扫描目录项. 其中cluster_offset是字节数的偏移, 也就是说开始找的地方, 是从簇的起始位置开始的cluster_offset字节个偏移. (根据这个字节偏移: 我们可以求出是簇的第几个扇区, 和那个扇区中的第几个字节数)扫描的目标: Path, 找到后, 发现距离起始位置是Moved_bytes, 然后结果存到r_diritem
static xfat_err_t locate_file_dir_item(xfat_t* xfat, u32_t locate_type, u32_t* dir_cluster, u32_t* cluster_offset, const char* path, u32_t* moved_bytes, diritem_t** r_diritem) {
	
	//当前扫描的是哪个簇
	u32_t curr_cluster = *dir_cluster;
	
	//定义一个disk,以后会用到
	xdisk_t* xdisk = xfat_get_disk(xfat);

	//补充:
	/*
	1. 因为我们的全局缓存buffer是512字节, 所以最多只能保存一个扇区的内容
	2. 所以: 我们是一个扇区一个扇区的扫描.
	2. 回忆: 一个簇有多个扇区, 一个扇区里面有多个目录项
	3. 但是参数给的offset(相对于簇起始位置),所以我们要求出: 开始找的地方, 应该是簇里面的第几个扇区, 以及是这个扇区的第几个字节
	*/
	u32_t initial_sector = to_sector(xdisk, *cluster_offset); //开始找的地方, 是簇里面的第几个扇区, 使用宏函数
	u32_t initial_offset = to_sector_offset(xdisk, *cluster_offset); //开始找的地方, 是这个扇区的具体哪个字节. 这个字节数对应的是第几个目录项, 我们后面会用:  initial_offset / sizeof(diritem_t), 也就是除以目录项的字节数

	//统计走了多少bytes, 初始值是0
	u32_t r_moved_bytes = 0;
	
	//扫描簇链, 因为起始的簇我们认为不是无效簇(例如未被使用, 或者坏簇), 所以用do while, 而不是while
	do {
		xfat_err_t err;
		u32_t i;

		//我们后面要将内容保存到缓存中, 但是读取函数需要的是全局的扇区号, 但是我们知道的只是相对于簇的扇区号, 所以绝对扇区号 = 簇号*一个簇的扇区个数 + 相对扇区号
		//接下来求: 该簇的第一个扇区的绝对扇区号
		u32_t start_sector = cluster_first_sector(xfat, curr_cluster); //curr_cluster是当前簇号, 这个簇号是在while里面会不断更新的

		//这个for loop是遍历一个簇的所有扇区
		for (i = initial_sector; i < xfat->sec_per_cluster; i++) //从intial_sector定义的扇区开始扫描, 一直到扫描到一个簇的结尾扇区
		{
			u32_t j;

			//将扇区读取到全局缓存buffer(512字节)中, 读取一个扇区
			err = xdisk_read_sector(xdisk, temp_buffer, start_sector + i, 1);//start_sector + i就是我们说的: 绝对扇区号 = 簇的第一个扇区的绝对扇区号 + 相对扇区号
			if (err < 0) return err;

			//这个for loop是遍历一个扇区的所有目录项
			//读取完, 对扇区里面的一个个目录项进行比对, 看是否是我们的目标path
			//注意, offset不一定是我们读取的buffer的第一个目录项, 可能是中间的, 所以我们是从buffer的第offset个目录项开始读取, 而不是从第一个开始读取
			//下面的for loop是: 偏移量offset(这个是字节数, 所以要除以目录项directory_item: dir_item的字节数大小), 一个扇区中目录项的总数: 一个扇区的字节数/一个目录项的字节数
			for (j = initial_offset / sizeof(diritem_t); j < xdisk->sector_size / sizeof(diritem_t); j++) {

				//获取当前的目录项, 因为buffer中是全部的目录项, 我们只需要找第j个
				diritem_t* dir_item = ((diritem_t*)temp_buffer) + j; //我喜欢这个表达,因为我们把buffer中的东西分成了目录项的大小, 这里是指针的加减, 所以+j相当于位移了j*目录项的大小

				if (dir_item->DIR_Name[0] == DIRITEM_NAME_END)//后面没有有效的目录项了
				{
					return FS_ERR_EOF;
				}
				else if (dir_item->DIR_Name[0] == DIRITEM_NAME_FREE)//如果是空闲的,说明可以继续找, 但是不是找这个目录项了, 是找J++下一个目录项
				{
					r_moved_bytes += sizeof(diritem_t); //这里说明已经走了一个, 不是我们需要的, 目录项, 但是走的字节数需要记录
					continue;
				}
				else if (!is_locate_match(dir_item, locate_type))//4.5 判断文件类型是不是我们想要的
				{
					r_moved_bytes += sizeof(diritem_t); //这里说明已经走了一个, 不是我们需要的, 目录项, 但是走的字节数需要记录
					continue;
				}

				//如果当前不是空闲或者无效, 那就去看看是不是和我们path一致. 下面说的是: 的确是找到了, bingo
				if ((path == (const char*)0) || (*path == 0) || is_filename_match((const char*)dir_item->DIR_Name, path))
				{
					u32_t total_offset = i * xdisk->sector_size + j * sizeof(diritem_t);//总的偏移字节数 = 从簇起始位置开始,前面有几个扇区 * 一个扇区几个字节 + 这是第几个目录项 * 目录项的字节数 
					*dir_cluster = curr_cluster;	//簇号, 是我们当前的簇号, 这个是用于返回的
					*cluster_offset = total_offset;	//簇中的字节偏移, 这个是用于返回的
					*moved_bytes = r_moved_bytes + sizeof(diritem_t); 

					if (r_diritem)//如果参数传来的是一个有效的指针, 我们就往里面传入整个目录项
					{
						*r_diritem = dir_item;
					}
					return FS_ERR_OK;
				}
			}
		}

		//接下来找下一个簇
		err = get_next_cluster(xfat, curr_cluster, &curr_cluster);
		if (err < 0) return err;

		//将扇区,偏移给清零, 供下一个簇开始使用
		initial_sector = 0;
		initial_sector = 0;

	} while (is_cluster_valid(curr_cluster));

	//遍历完了所有的簇, 但是还是没有找到
	return FS_ERR_EOF;
}
//4.2 修改 //4.1 打开函数
static xfat_err_t open_sub_file(xfat_t* xfat, u32_t dir_cluster, xfile_t* file, const char* path)//找一个文件, 这个文件的父目录的簇号是dir_cluster(也就是说, 去这个dir_cluster簇中, 这个簇里面有很多目录项, 其中一个目录项代表了这个file, 我们通过判断path和哪个目录项的路径一致, 来判断这个file属于哪个目录项, 从而找到这个file的簇号)
{
	u32_t parent_cluster = dir_cluster;
	u32_t parent_cluster_offset = 0;

	path = skip_first_path_sep(path); //跳过第一个分隔符, 例如/abc.txt, 跳过第一个分隔符之后就变成了abc.txt
	if ((path != '\0') && (*path != '\0')) //除了根目录以外的目录
	{
		//定义路径
		const char* curr_path = path;

		//找到的diritem_t
		diritem_t* dir_item = (diritem_t*)0;

		//定义簇号
		u32_t file_start_cluster = 0;

		while (curr_path != (const char*)0)  //如果路径没有到头
		{
			//移动的字节数
			u32_t moved_bytes = 0;

			//4.5 设置成: 只要dot文件和普通文件.为什么? 因为我们需要找的目录可能是/read/./a/../b/c, 如果不要dot的话, 你会发现执行到/read就执行不下去了.  //找到文件所在. 如果是根目录下的文件, 就要从根目录的簇链(parent_cluster)开始找
			xfat_err_t err = locate_file_dir_item(xfat, XFILE_LOCATE_DOT | XFILE_LOCATE_NORMAL, &parent_cluster, &parent_cluster_offset, curr_path, &moved_bytes, &dir_item);
			//解释:
			/*
			1. parent_cluster: 所在簇号
			2. parent_cluster_offset: 该簇所在的具体位置(我觉得可能是字节为单位), 这里是指开始找的具体位置, moved_bytes指的是找到的位置举例开始找的位置的偏移量
			3. curr_path: 用于验证, 找到的东西是不是和我们要找的路径是匹配的
			4. moved_bytes: moved_bytes指的是找到的位置举例开始找的位置的偏移量
			5. dir_item: 找到的结果(文件的信息:大小, 属性, 位置..)
			*/
			if (err < 0) return err;

			//检车diritem是否有效
			if (dir_item == (diritem_t*)0)//说明无效
			{
				return FS_ERR_NONE;
			}

			//4.5  /xxx/..中, 读到..之后, 簇号会变成父目录的第一个簇, 也就是相对簇号是0
			//4.5 但是注意, 如果父目录是根目录, 那么第一个簇将是2. 例如read的上一层是根目录, 所以/read/.., 读到..之后, 簇号会变成父目录(根目录)的第一个簇, 也就是相对簇号是2
			//4.5 修改, 见下的else
			


			//获取子路径
			curr_path = get_child_path(curr_path); //例如之前curr_path是a/b/c.txt, 现在curr_path变成b/c.txt
			if (curr_path != (const char*)0) { //说明还有子目录
				parent_cluster = get_diritem_cluster(dir_item); //从目录项, 获取相应的簇号
				parent_cluster_offset = 0;
			}
			else { //已经是最后一个c.txt了?
				//记录文件的簇号:
				file_start_cluster = get_diritem_cluster(dir_item); //记录文件的簇号

				//4.5 判断是否是..这个特殊的文件名. 如果下面这个if(){}不加, 打印/read/..的时候, 会输出烫烫烫烫. 因为簇==0是乱码, 根目录的簇是从2开始.
				if ((memcmp(DOT_DOT_FILE, dir_item->DIR_Name, SFN_LEN) == 0) && (file_start_cluster == 0)) //如果是..文件名 && 它的上一级是根目录, 例如/read/.., 因为read是根目录下, 所以..返回到上一级也就是根目录
				{
					file_start_cluster = xfat->root_cluster;
				}
			}
		}

		file->size = dir_item->DIR_FileSize;
		file->type = get_file_type(dir_item); //虽然dir_item里面有Attr属性, 但是是通过8个bit来表示的, 所以这里用函数获取
		file->start_cluster = file_start_cluster;
		file->curr_cluster = file_start_cluster;
	}
	else { //说明要找的是根目录: '/'
		file->size = 0;
		file->type = FAT_DIR;
		file->start_cluster = dir_cluster;
		file->curr_cluster = dir_cluster;
	}

	//这一块是共同的
	file->xfat = xfat;
	file->pos = 0;
	file->err = FS_ERR_OK;
	file->attr = 0;

	return FS_ERR_OK;
}
//4.5 修改, 如果传入了错误路径 //4.1 文件打开
xfat_err_t xfile_open(xfat_t* xfat, xfile_t* file, const char* path) {

	//4.5 错误路径: /.., 根目录没有上一层目录
	path = skip_first_path_sep(path);//跳过开头的/反斜杠
	if (memcmp(path, "..", 2) == 0) {
		return FS_ERR_PARAM; //如果发现是.., 说明原先是/.., 也就是错误参数
	}
	else if (memcmp(path, ".", 1) == 0) {
		path++; //如果发现是., 说明之前是/., 直接跳过就好了
	}

	return open_sub_file(xfat, xfat->root_cluster, file, path); //因为这里我们是将根目录看成要找的file ,但是根目录没有父目录, 所以第二个参数(父目录的簇号), 我们就直接用根目录的簇号代替

}

//4.6 打开子目录. 现在不支持: 同一个路径的文件, 打开多次
xfat_err_t xfile_open_sub(xfile_t* dir, const char* sub_path, xfile_t* sub_file) //当前目录:dir, 要打开的子路径: sub_path, 子路径的文件: sub_file
{
	//4.5 错误路径: /.., 根目录没有上一层目录
	sub_path = skip_first_path_sep(sub_path);//跳过开头的/反斜杠

	//4.5 删除下面的, 因为现在是在子目录, 所以肯定不在根目录, 所以不用担心/..这样的情况
	/*if (memcmp(path, "..", 2) == 0) {
	//	return FS_ERR_PARAM; //如果发现是.., 说明原先是/.., 也就是错误参数
	}*/

	//4.5 老师说, 因为dir是已经打开的了, 我们不希望再打开一次, 所以认为 /dir/.是参数错误
	if (memcmp(sub_path, ".", 1) == 0) {
		return FS_ERR_PARAM;
	}

	//我认为: 簇链的信息保存在了xfat中, 子目录的父目录的簇号保存在了dir->start_cluster中
	return open_sub_file(dir->xfat, dir->start_cluster, sub_file, sub_path); //因为这里我们是将根目录看成要找的file ,但是根目录没有父目录, 所以第二个参数(父目录的簇号), 我们就直接用根目录的簇号代替

}


//4.1 文件关闭
xfat_err_t xfile_close(xfile_t* file)
{
	return	FS_ERR_OK;
}

//4.4 拷贝时间
static void copy_date_time(xfile_time_t* dest, const diritem_date_t* date, const diritem_time_t* time, const u8_t mili_sec) {
	if (date) {
		dest->year = date->year_from_1980 + 1980;
		dest->month = date->month;
		dest->day = date->day;
	}
	else
	{
		dest->year = 0;
		dest->month = 0;
		dest->day = 0;
	}

	if (time) {
		dest->hour = time->hour;
		dest->minute = time->minute;
		dest->second = time->second_2 * 2 + mili_sec / 100;
	}
	else {
		dest->hour = 0;
		dest->minute = 0;
		dest->second = 0;
	}
}

//4.4 将用户视角的文件名, 转化成内部的文件名
static void sfn_to_myname(char* dest_name, const diritem_t* diritem) { //已有的信息: dititem中的内部名字123     ABC, 转换成用户读的123.abc放入dest_name中
	//定义指针, 指向两个参数, 调试的时候方便观察
	char* dest = dest_name, *raw_name = (char*)diritem->DIR_Name;

	//判断扩展名是否存在, 后面拷贝的时候会用到
	u8_t ext_exist = (raw_name[8] != 0x20);//看raw_name的第九个字节(index==8), 如果不是空格(!= 0x20), 说明扩展名存在. //你可以去DIR_Name看到, 第八个字节, 就是扩展名u8_t Dir_ExtName的第一个字节. 

	//如果有扩展名, 最多拷贝12个字节
	// --> 12345678ABC(内部看到) --> 12345678.ABC(用户看到)
	//如何判断有没有扩展名, 就是看第9个字节, 是不是有字符, 有的话说明存在扩展名
	u8_t scan_len = ext_exist ? SFN_LEN + 1 : SFN_LEN; //如果存在扩展名就是12, 不存在就是11=8+3

	//将dest_name清空
	memset(dest_name, 0, X_FILEINFO_NAME_SIZE); //长度是32

	int i;
	for (i = 0; i < scan_len; i++) {
		if (*raw_name == ' ') {
			raw_name++;
		}
		else if( (i == 8) && ext_exist){
			*dest++ = '.';
		}
		else
		{
			u8_t lower = 0;
			if( ((i < 8) && (diritem->DIR_NTRes & DIRITEM_NTRES_BODY_LOWER)) 
				|| ((i > 8) && (diritem->DIR_NTRes & DIRITEM_NTRES_EXT_LOWER)) ) //如果扩展名需要小写, 或者文件名需要小写. 注意, 这里如果没有扩展名的话, 我认为扩展名就都是空格, 但是空格的大小写都一样
			{
				lower = 1;
			}

			*dest++ = lower ? tolower(*raw_name++) : toupper(*raw_name++);
		}
	}
	*dest++ = '\0'; //最后一个字符串为空. 
	
}

//4.4 拷贝文件信息
static void copy_file_info(xfileinfo_t* info, const diritem_t* dir_item) {
	//存储名字的时候, 是给应用看的ABC_____TXT(不是给用户看的abc.txt)
	sfn_to_myname(info->file_name, dir_item);

	info->size = dir_item->DIR_FileSize;
	info->attr = dir_item->DIR_Attr;
	info->type = get_file_type(dir_item);

	copy_date_time(&info->create_time, &dir_item->DIR_CrtDate, &dir_item->DIR_CrtTime, dir_item->DIR_CrtTimeTeenth);
	copy_date_time(&info->last_acctime, &dir_item->DIR_LastAccDate,0,0);
	copy_date_time(&info->modify_time, &dir_item->DIR_WrtDate, &dir_item->DIR_WrtTime, 0);
}

//4.4 定位到当前文件所在的簇 //todo, 目的到底是什么? 
xfat_err_t xdir_first_file(xfile_t* file, xfileinfo_t* info)//传入file结构, 还有传地址取值的info
{
	u32_t cluster_offset;
	u32_t moved_bytes = 0; //在簇链中移动到了多少字节
	xfat_err_t err;
	diritem_t* diritem = (diritem_t*)0;

	//判断file是否指向了目录
	if (file->type != FAT_DIR) {
		return FS_ERR_PARAM;
	}

	//指向了目录就继续往下做


	//返回目录的第一个
	file->curr_cluster = file->start_cluster;
	file->pos = 0;

	cluster_offset = 0; //从当前簇的最开始开始找
	
	//4.5 修改 //获取dir_item
	err = locate_file_dir_item(file->xfat, XFILE_LOCATE_NORMAL, &file->curr_cluster, &cluster_offset, "", &moved_bytes, &diritem);	//文件路径: 设置"", 为空, 也就是说, 我们可以通过xfat.c中的if(path == (const char*)0), 也就是找到任意个有效的目录项, 不管是文件还是目录都返回 //找到diritem后, 在簇链中移动了moved_bytes
	if(err < 0) return err;

	if (diritem == (diritem_t*)0)//说明没有读取成功: 没有找到任何有效的文件
	{
		return FS_ERR_EOF;	
	}

	file->pos += moved_bytes;

	copy_file_info(info, diritem); //将diritem的信息拷贝
	
	return err;
}

//4.4 当前文件的下一个簇
xfat_err_t xdir_next_file(xfile_t* file, xfileinfo_t* info) //传入file结构, 还有传地址取值的info
{
	u32_t cluster_offset;
	u32_t moved_bytes = 0; //在簇链中移动到了多少字节
	xfat_err_t err;
	diritem_t* diritem = (diritem_t*)0;

	//判断file是否指向了目录
	if (file->type != FAT_DIR) {
		return FS_ERR_PARAM;
	}

	//指向了目录就继续往下做


	//从当前的位置继续找下去, 所以不需要定位到文件的开头, 所以comment下面两句
	//file->curr_cluster = file->start_cluster;
	//file->pos = 0;

	cluster_offset = to_cluster_offset(file->xfat, file->pos); //将pos转换成当前簇的偏移

	//4.5 修改 //获取dir_item
	err = locate_file_dir_item(file->xfat, XFILE_LOCATE_NORMAL, &file->curr_cluster, &cluster_offset, "", &moved_bytes, &diritem);	//文件路径: 设置"", 为空, 也就是说, 我们可以通过xfat.c中的if(path == (const char*)0), 也就是找到任意个有效的目录项, 不管是文件还是目录都返回 //找到diritem后, 在簇链中移动了moved_bytes
	if (err < 0) return err;

	if (diritem == (diritem_t*)0)//说明没有读取成功: 没有找到任何有效的文件
	{
		return FS_ERR_EOF;
	}

	file->pos += moved_bytes;

	//在原有的cluster_offset这个偏移项中, 移动一个目录项的字节数之后, 判断是否超出簇的边界, 如果超出, 就跳到簇链的下一个簇
	if (cluster_offset + sizeof(diritem_t) >= file->xfat->cluster_byte_size) {
		err = get_next_cluster(file->xfat, file->curr_cluster, &file->curr_cluster);
		if (err < 0) return err;
	}

	copy_file_info(info, diritem); //将diritem的信息拷贝

	return err;
}

//4.7 获取错误码
xfat_err_t xfile_error(xfile_t* file) 
{
	return file->err;
}

//4.7 清除错误码
void xfile_clear_err(xfile_t* file)
{
	file->err = FS_ERR_OK;
}
