#ifndef	XTYPES_H
#define XTYPES_H

//�����������������,������ֲ���Ѻ�
#include <stdint.h> //���ͷ�ļ������й����������͵Ķ���: uint8_t��
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

//��������Ĵ���������, �Ժ���õ�  //�������ö��enum
typedef enum _xfat_err_t {
	FS_ERR_OK = 0,	//û�д���. (ע��!�Ƕ��Ž�β, ���Ƿֺ�)
	FS_ERR_IO = -1, //���ļ�����
}xfat_err_t; //�ǵ�Ҫд��xfat_err_t, ���򱨴�˵: ��Ҫ��������

#endif
