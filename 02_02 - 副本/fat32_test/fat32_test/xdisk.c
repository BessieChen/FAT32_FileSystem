#include "xdisk.h"

u8_t temp_buffer[512]; //2.2 ��ʱ���ڴ������Ӳ�̵�ǰ512�ֽ�

//1.3 �ĸ��ӿڵ�����(�����Ǵ�driver.c����, �ڴ˻������޸�)
xfat_err_t xdisk_open(xdisk_t* disk, const char* name, xdisk_driver_t* driver, void* init_data) //name: ��ʼ��disk������, driver: ����driver��ָ��
{
	xfat_err_t err;
	disk->driver = driver; //disk�ĳ�ʼ��

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

	//��ȡ�����յ�����, �Ƿ񳬹��˱߽�
	if (start_sector + count >= disk->total_sector) return FS_ERR_PARAM;

	//���û����, ����������driver�Ľӿ�
	err = disk->driver->read_sector(disk, buffer, start_sector, count);
	return err;
}

//1.3
xfat_err_t xdisk_write_sector(xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count)
{
	xfat_err_t err;

	//��д�������, �Ƿ񳬹��˱߽�
	if (start_sector + count >= disk->total_sector) return FS_ERR_PARAM;

	//���û����, ����������driver�Ľӿ�
	err = disk->driver->write_sector(disk, buffer, start_sector, count);
	return err;
}

//2.2 ��ȡ�ж��ٸ�����, ��ʵ���ǿ���������4���������õ���Ϣ��Щ����Ч��
xfat_err_t xdisk_get_part_count(xdisk_t * disk, u32_t * count)
{
	int r_count = 0, i = 0;
	
	//��4����������:
	mbr_part_t* part;
	u8_t* disk_buffer = temp_buffer;//���temp_buffer��ȫ�ֱ���, �����ļ�Ҳ������, ����ר�Ŵ濪ͷ512�ֽڵ�

	//��ȡdisk��ǰ512�ֽ�, �浽disk_bufferָ���temp_buffer
	int err = xdisk_read_sector(disk, disk_buffer, 0, 1); //�����0�Ǵӵ�0��������ʼ��, 1��ֻ��һ������, ��Ϊ����������һ����������512�ֽ�, ���Ըոպ�
	if (err < 0) return err;

	//��Ϊdisk_buffer�ǵ�0���������׵�ַ, ���Կ���֪�����еķ�����Part_info
	part = ((mbr_t*)disk_buffer)->part_info;
	//����: 1. disk_buffer��һ��ָ��, ��������������Ϊ������u8_tָ��, ����mbr_tָ��, ���Կ�����->ȥ��ȡ����Աpart_info, ֮ǰ�����mbr_part_t* part, ��Ӧ���ǳ�Աmbr_part_t part_info[MBR_PRIMARV_PART_NR];(��xdisk.h)

	//����ÿһ����������, ��system_id�Ƿ�Ϸ�, �Ϸ���˵����һ������. ��ϲ����д��: part++, ��Ϊ֮ǰ��mbr_part_t part_info[], ��������͵�����, ���Կ�����part++
	
	u8_t hi[2];
	u8_t* hi2 = hi;
	hi2 = ((mbr_t*)disk_buffer)->boot_sig;

	for (i = 0; i < MBR_PRIMARV_PART_NR; i++, part++)
	{
		if (part->system_id == FS_NOT_VALID)
		{
			continue;//����������һ��
		}
		else
		{
			r_count++;
		}
	}
	*count = r_count; //����Ǻܾò�д��, ��Ϊcount�ǵ�ַ, ����*count��ָ���Ǹ���ַ
	return FS_ERR_OK;
}



