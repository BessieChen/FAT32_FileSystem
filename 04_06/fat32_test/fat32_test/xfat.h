#ifndef XFAT_H
#define XFAT_H

#include "xtypes.h"
#include "xdisk.h" //3.2 因为需要用到xdisk_part_t

//4.5 标志位判断
#define XFILE_LOCATE_NORMAL		(1<<0)//只返回普通文件
#define XFILE_LOCATE_DOT		(1<<1)//只返回./..文件
#define XFILE_LOCATE_VOL		(1<<2)//只返回卷标
#define XFILE_LOCATE_SYSTEM		(1<<3)//只返回系统文件: 垃圾箱bin
#define XFILE_LOCATE_HIDDEN		(1<<4)//只返回隐藏文件
#define XFILE_LOCATE_ALL		0xFF//返回全部类型

//3.4 簇号无效:
#define CLUSTER_INVALID					0x0FFFFFFF			//参见文档, 表示簇号无效

//3.3 宏, 用于Dir_name
#define DIRITEM_NAME_FREE               0xE5                // 目录项空闲名标记
#define DIRITEM_NAME_END                0x00                // 目录项结束名标记

//4.4
#define DIRITEM_NTRES_BODY_LOWER        0x08                // 文件名小写: 也就是0100
#define DIRITEM_NTRES_EXT_LOWER         0x10                // 扩展名小写: 也就是1000

//3.3 属性
#define DIRITEM_ATTR_READ_ONLY          0x01                // 目录项属性：只读
#define DIRITEM_ATTR_HIDDEN             0x02                // 目录项属性：隐藏
#define DIRITEM_ATTR_SYSTEM             0x04                // 目录项属性：系统类型
#define DIRITEM_ATTR_VOLUME_ID          0x08                // 目录项属性：卷id
#define DIRITEM_ATTR_DIRECTORY          0x10                // 目录项属性：目录
#define DIRITEM_ATTR_ARCHIVE            0x20                // 目录项属性：归档
#define DIRITEM_ATTR_LONG_NAME          0x0F                // 目录项属性：长文件名

//4.2 8+3格式的文件名, 例如12345678txt
#define SFN_LEN							11					//短文件名, 11字节

//3.1 定义FAT分区里面的第0块内容, 也就是保留区
#pragma pack(1) //因为我们要保证和disk里面的内容一一对应, 不留空隙, 所以记得用pragma pack(1)

//bios parameter block(参数块), 一共有3+8+2+1+2+1+2+2+1+2+2+2+4+4=36字节
typedef struct _bpb_t {
	u8_t BS_jmpBoot[3];//三字节的跳转代码, 一般是: 0xeb, 0x58, ....,
	u8_t BPB_OEMName[8];//OEM的名称, 8字节
	u16_t BPB_BytesPerSec;//每个扇区,有多少字节
	u8_t BPB_SecPerClus;//每簇扇区的数目, section per cluster
	u16_t BPB_RsvdSecCnt;//保留区的扇区数, reserved section count
	u8_t BPB_NumFATs;//FAT表有多少个 //注意, fat表有多少个记录在了这里
	u16_t BPB_BootEntCnt;//根目录项目数, boot entry count
	u16_t BPB_TotSec16;//总的扇区数, total section. //注意, 如果是fat32分区, 我们不会使用这一条, 而是使用line25的u32_t BPB_TotSec32;. 之所以有这一个,是因为历史原因, 然后为了兼容把 
	u8_t BPB_Media;//媒体类型
	u16_t BPB_FATSz16;//FAT表项大小, size //注意, 如果是fat32分区, 我们不会使用这一条, 而是使用line30的_fat32_hdr_t中的u32_t BPB_FATSz32;
	u16_t BPB_SecPerTrk;//每磁道扇区数, section per track
	u16_t BPB_NumHeads;//磁头数
	u32_t BPB_HiddSec;//隐藏扇区数
	u32_t BPB_TotSec32;//总的扇区数
}bpb_t;

//fat32字块, 一共有: 4+2+2+4+2+2+1*4+4+1*2=26字节
typedef struct _fat32_hdr_t {
	u32_t BPB_FATSz32; //fat表的字节大小
	u16_t BPB_ExtFlags; //扩展标记
	u16_t BPB_FSVer;//版本号
	u32_t BPB_RootClus;//根目录的簇号
	u16_t BPB_FsInfo;//fsInfo的扇区号
	u16_t BPB_BkBootSec;//备份扇区
	u8_t BPB_Reserved[12];
	u8_t BS_DrvNum;//设备号
	u8_t BS_Reserved1;
	u8_t BS_BootSig;//扩展标记
	u32_t BS_VolID;//卷序列号
	u8_t BS_VolLab[11];//卷标名称
	u8_t BS_FileSysType[8];//文件类型名称, 可以看到显示的是0x00d3 a1b2, "FAT32"
}fat32_hdr_t;

typedef struct _dbr_t {
	bpb_t bpb;
	fat32_hdr_t fat32;
}dbr_t;

// 3.3 FAT目录项的日期类型, diritem_t会使用
typedef struct _diritem_date_t {
	u16_t day : 5;                  // 日
	u16_t month : 4;                // 月
	u16_t year_from_1980 : 7;       // 年
} diritem_date_t;

//3.3 FAT目录项的时间类型, diritem_t会使用
typedef struct _diritem_time_t {
	u16_t second_2 : 5;             // 2秒, 这个应该就是他们说的fat32系统的bug, 只能走偶数秒
	u16_t minute : 6;               // 分
	u16_t hour : 5;                 // 时
} diritem_time_t;

/**
* //3.3 定义数据区中, 簇里面的内容, 目录项
*/
typedef struct _diritem_t {
	u8_t DIR_Name[8];                   // 文件名
	u8_t DIR_ExtName[3];                // 扩展名
	u8_t DIR_Attr;                      // 属性
	u8_t DIR_NTRes;
	u8_t DIR_CrtTimeTeenth;             // 创建时间的毫秒
	diritem_time_t DIR_CrtTime;         // 创建时间
	diritem_date_t DIR_CrtDate;         // 创建日期
	diritem_date_t DIR_LastAccDate;     // 最后访问日期
	u16_t DIR_FstClusHI;                // 簇号高16位
	diritem_time_t DIR_WrtTime;         // 修改时间
	diritem_date_t DIR_WrtDate;         // 修改时期
	u16_t DIR_FstClusL0;                // 簇号低16位
	u32_t DIR_FileSize;                 // 文件字节大小
} diritem_t;

//3.4 方便我们访问fat_buffer (参见文档: fat表的目录项一共是32位, 其中高4位是无效的(保留的), 低28位是有效的. 所以1000 0000和F000 0000的低28位都是000 0000, 所以都表示簇为空)
typedef union _cluster32_t { //这里是联合体, 有种选择题的感觉
	struct {
		u32_t next : 28;
		u32_t reserved : 4; //这个是高4位
	}s;//这个是使用低28位
	u32_t v; //这个是使用32位全部
}cluster32_t;

#pragma pack() //以上都是不能有空隙的


//3.2 记录xfat表和数据区的关键信息. 注意:这个结构体只是记录关键信息. 不需要对应disk的数据, 所以不需要#pragma pack(1), 这个结构只是我们自己用的
typedef struct _xfat_t {
	u32_t fat_start_sector; //fat区的起始扇区
	u32_t fat_tbl_nr; //多少个fat表
	u32_t fat_tbl_sectors;//每个fat表占用多少个扇区

	//3.3 添加和簇相关的字段
	u32_t sec_per_cluster; //3.3 每个簇, 有多少个扇区
	u32_t root_cluster; //3.3 数据区的根目录(顶层目录)的起始簇号(也就是第一个簇号)是多少
	u32_t cluster_byte_size;//3.3 每个簇, 占用了多少字节(通过计算可以获得)

	u8_t* fat_buffer; 	//3.4 指向fat表的缓冲, 将所有fat表的内容都存在缓冲中


	u32_t total_sectors;//fat区总的扇区数量
	xdisk_part_t* disk_part;//这个fat区是哪个分区的fat区
}xfat_t;

//4.1 文件类型
typedef enum _xfile_type_t {
	FAT_DIR, //目录
	FAT_FILE, //普通文件
	FAT_VOL, //卷标. 是cde盘
}xfile_type_t;

//4.1 文件描述结构, 代表某个文件
typedef struct _xfile_t {
	xfat_t* xfat; //这个file位于哪个xfat上
	u32_t size;//文件大小
	u32_t attr;//文件属性
	xfile_type_t type;//文件类型: 普通文件 , 目录
	u32_t pos;//当前位置
	xfat_err_t err;//上一次读写的结果
	u32_t start_cluster;//文件的第一个簇号
	u32_t curr_cluster;//当前的簇号
}xfile_t;

//4.4 文件的时间
typedef struct _xfile_time_t {
	u16_t year;
	u16_t month;
	u16_t day;
	u16_t hour;
	u16_t minute;
	u16_t second;
}xfile_time_t;

//4.4 文件信息
typedef struct _xfileinfo_t {
#define X_FILEINFO_NAME_SIZE	32	//可配置的形式
	char file_name[X_FILEINFO_NAME_SIZE]; //文件名是给应用看的, 所以是32字节

	u32_t size;	//文件的大小
	u16_t attr;	//文件的属性
	xfile_type_t type;	//类型: 目录/普通文件
	xfile_time_t create_time; //创建事件
	xfile_time_t last_acctime;//上一次访问事件
	xfile_time_t modify_time;//修改事件
}xfileinfo_t;

//3.2 定义一个函数, 实现fat区的打开功能
xfat_err_t xfat_open(xfat_t* xfat, xdisk_part_t* xdisk_part);//xdisk_part: 打开xdisk_part这个分区

//3.3 读取簇的内容
xfat_err_t read_cluster(xfat_t* xfat, u8_t* buffer, u32_t cluster, u32_t count); //xfat: 可以提供一些信息(例如哪个disk,哪个fat32分区..), buffer: 读取的结果存到buffer, 传地址取值, cluster: 从这个fat32分区的数据区的哪个簇号开始读取, count:连续读取几个簇

//3.4 判断簇号是否合法
int is_cluster_valid(u32_t cluster_no);

//3.4 查找某个簇的后续簇的簇号
xfat_err_t get_next_cluster(xfat_t* xfat, u32_t curr_cluster_no, u32_t* next_cluster); //next_cluster:传地址取值存下一个簇的簇号

//4.1 文件接口
xfat_err_t xfile_open(xfat_t* xfat, xfile_t* file, const char* path);//path: 打开的路径
xfat_err_t xfile_close(xfile_t* file);

//4.4 定位到当前文件所在的簇
xfat_err_t xdir_first_file(xfile_t* file, xfileinfo_t* info); //传入file结构, 还有传地址取值的info
//4.4 当前文件的下一个簇
xfat_err_t xdir_next_file(xfile_t* file, xfileinfo_t* info); //传入file结构, 还有传地址取值的info

//4.6 打开子目录
xfat_err_t xfile_open_sub(xfile_t* dir, const char* sub_path, xfile_t* sub_file); //当前目录:dir, 要打开的子路径: sub_path, 子路径的文件: sub_file


#endif // !XFAT_H

