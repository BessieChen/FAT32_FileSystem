#ifndef	XTYPES_H
#define XTYPES_H

//定义基本的数据类型,对于移植很友好
#include <stdint.h> //这个头文件里面有关于数据类型的定义: uint8_t等
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

//定义基本的错误码类型, 以后会用到  //定义成了枚举enum
typedef enum _xfat_err_t {
	FS_ERR_OK = 0,	//没有错误. (注意!是逗号结尾, 不是分号)
	FS_ERR_IO = -1, //打开文件出错
	FS_ERR_PARAM = -2, //参数错误, 例如传入的读取范围过大
}xfat_err_t; //记得要写上xfat_err_t, 否则报错说: 需要设置声明

#endif
