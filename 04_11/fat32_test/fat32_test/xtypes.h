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
	FS_ERR_EOF = 1, //end of file, �����Ҳ�����һ��512�ֽڵ�mbr
	FS_ERR_OK = 0,	//û�д���. (ע��!�Ƕ��Ž�β, ���Ƿֺ�)
	FS_ERR_IO = -1, //���ļ�����
	FS_ERR_PARAM = -2, //��������, ���紫��Ķ�ȡ��Χ����
	FS_ERR_NONE = -3, //����û���ҵ�
	FS_ERR_FSTYPE = -4, //�ļ����ʹ���
}xfat_err_t; //�ǵ�Ҫд��xfat_err_t, ���򱨴�˵: ��Ҫ��������

//4.8 �ļ���С
typedef u32_t xfile_size_t; //32λ, 4GB

//4.9 ƫ������: ����<0, ����>0, �������з���, ������64λ
typedef int64_t xfile_ssize_t;
//4.9 �ļ��Ķ�λ
typedef enum _xfile_origin_t {
	XFAT_SEEK_SET,	//�ļ���ʼλ��
	XFAT_SEEK_CUR,	//�ļ���ǰλ��
	XFAT_SEEK_END,	//�ļ�����λ��
}xfile_origin_t;

#endif
