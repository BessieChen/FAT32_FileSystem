//1.1 ǰ�涨����disk�Ľṹ�ͽӿ�, ��������ʵ��disk�Ľӿ�driver
#include "xdisk.h"
#include "xfat.h" //todo, ���ﲻ̫����ΪʲôҪ���ͷ�ļ�
#include <stdio.h>//1.2 ��ʹ�õ�c���Ե��ļ���д�ӿ�

//1.1 ʵ������driver�Ľӿ�:���ض�д
//ע��: �������static����, ˵��ֻ���������.c�ļ�
static xfat_err_t xdisk_hw_open(xdisk_t* disk, void* init_data)
{
	//1.2 path�����ǹ̶�ֵ, ���������ֵ. ������init_data����
	const char* path = (const char*)init_data;
	//1.2 c���Ե��ļ���, Ŀ¼�ϵ��ļ�->���ɴ���
	FILE* file = fopen(path, "rb+"); //��rb+ ��д��һ���������ļ���ֻ�����д���ݡ�
	if (file == NULL)
	{
		printf("open disk failed: %s\n", path);
		return FS_ERR_IO;//��xtypes.h
	}

	disk->data = file; //1.2 ���ļ�file�򿪵�ʱ��, ����Ϣ����disk->data��, �����൱�ڻ�����data��, ���Ժ���
	disk->sector_size = 512; 

	//1.2 ��ʦ: c����û�з���file�ļ���С��api, Ҳ������win�Դ���api
	fseek(file, 0, SEEK_END); //����дλ�����õ������ļ�ĩβ��0λ�ô�(Ҳ�����ļ�ĩβ��), ����ʼ�
	disk->total_sector = ftell(file) / disk->sector_size; //���ú��� ftell ���ܷǳ����׵�ȷ���ļ��ĵ�ǰλ��, Ҳ�����ļ�ĩβ��λ��
	return	FS_ERR_OK;
}
static xfat_err_t xdisk_hw_close(xdisk_t* disk)
{
	//1.2 ��Ϊfileָ���Ѿ�����disk->data����
	FILE* file = (FILE*)disk->data;
	fclose(file);
	return FS_ERR_OK;
}
static xfat_err_t xdisk_hw_read_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count)
{
	u32_t offset = start_sector * disk->sector_size; //�����ʼ����, ��������ļ�����ʵλ��˵��, ����, ���offsetƫ����, ����������ļ���ʵλ�õ�ƫ����
	//1.2 ��ȡ�ļ�ָ��
	FILE* file = (FILE*)disk->data;
	//1.2 ����read����ʼλ��: Ҳ�����ļ���ʼλ�ÿ�ʼ, ��ǰ��offset����λ
	int err = fseek(file, offset, SEEK_SET);//����SEEK_SET�����ļ�����ʼλ��

	if (err == -1)
	{
		printf("seek disk failed: 0x%x\n", offset);
		return FS_ERR_IO;
	}

	err = fread(buffer, disk->sector_size, count, file);//buffer: ��������ݷŵ�buffer, count: ��ȡ�Ĵ���, sector_size: ÿ�ζ�ȡ�Ĵ�С(�ֽڰ�)
	if (err == -1)
	{
		printf("read disk failed: sector: %d, count: %d\n", disk->sector_size, count);
		return FS_ERR_IO;
	}

	//1.2 ����: Ϊʲô��fseek()��fread()
	/*
	1. ��ʵ��ȡ����һ��ָ��ָ����ж�ȡ
	2. fread()�Ǵӵ�ǰָ����ָ��ĵط���ȡ, ���������ڽ����������֮ǰ, �����޷��϶����ָ���ǲ���ָ�������ڴ���λ��
	3. ������fseek()ȷ��ָ��ָ������ڴ���λ��
	*/
	return FS_ERR_OK;
}
static xfat_err_t xdisk_hw_write_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count)
{
	u32_t offset = start_sector * disk->sector_size; 
	FILE* file = (FILE*)disk->data;
	int err = fseek(file, offset, SEEK_SET);

	if (err == -1)
	{
		printf("seek disk failed: 0x%x\n", offset);
		return FS_ERR_IO;
	}

	err = fwrite(buffer, disk->sector_size, count, file);
	if (err == -1)
	{
		printf("write disk failed: sector: %d, count: %d\n", disk->sector_size, count);
		return FS_ERR_IO;
	}
	//1.2 ǰ������ݺ�readһ��, ֻ����write��Ҫ��һ��ˢ���ļ�
	fflush(file);
	/*
	Ϊʲô��Ҫfflush()
	1. ���Ե�ʱ��, �����öϵ�
	2. ������������, ��ǿ����ֹ����
	3. ��ֹ����, ��������: һ��������,��bufferд���˲���ϵͳ�Ļ�����, û�л�д������disk��
	4. Ϊ��ȷ��д��disk��, ��Ҫ��ˢ��fflush()
	*/
	return FS_ERR_OK;

}

//1.1 ��������driver�Ľṹ��Ķ���, ����vdisk����˼virtual driver��������
xdisk_driver_t vdisk_driver =
{
	.close = xdisk_hw_close, //ע���Ƕ��Ž�β
	.open = xdisk_hw_open, //c99������(һ����+������: .open), ������ô����һ������, ����˳����Ըı�, ����close��openǰ��
	.read_sector = xdisk_hw_read_sector,
	.write_sector = xdisk_hw_write_sector
}; //�ǵ��зֺŽ�β

/*
1. ����driver��ʵ��, Ϊʲô����xdisk.h��ʵ��
	����ͬ����sd, ����ֻ�ǿ��С��ͬ, �����ĸ��ӿڶ���ȫһ��
	����, ���ǻ����ʵ��driver, ����������Ϣ���Բ�ͬ
	��ͬ����Ϣ + ��ͬ��diver => ��ͬ��sd
*/
