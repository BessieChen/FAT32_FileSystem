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

//2.3 ʵ�ּ�����չ�����м���С��Ƭ����
static xfat_err_t xdisk_get_extend_part_count(xdisk_t* disk, u32_t start_sector, u32_t* count) //start_sector��ָ����������������disk,�ǵڼ�������
{
	int r_count = 0;

	//����һ����ǰ����������
	u32_t ext_start_sector = start_sector;

	//����һ��������ָ��:
	mbr_part_t* part;

	//���建����
	u8_t* disk_buffer = temp_buffer; //ʹ����ȫ�ֻ�����

	//��ȡ������
	do {
		
		//��ȡһ������, ע��, ÿһ��while֮��, start_sector�������, start_sector���·����������disk����λ��
		int err = xdisk_read_sector(disk, disk_buffer, start_sector, 1); //��start_sector��ʼ��, ��һ��
		if (err < 0) return err;

		//��Ϊ����һ����չ����, ����ĵ�һ��, ͬ��Ҳ��512�ֽ�(446+64������+2), ��������Ҫ�������м�ķ�����part_info;
		part = ((mbr_t*)disk_buffer)->part_info;
		//��Ϊmbr�ṹ��, ��mbr_part_t part_info[MBR_PRIMARV_PART_NR]; ����part��ʼָ����Ƿ�����ĵ�һ����������
		//ע��, 64�ֽڵķ�������, Ӧ��ֻ��ǰ�������ÿ���ʹ��
		//��һ�����õ�xxx��Ա, ��¼��С��Ƭ����ʼλ��
		//�ڶ������õ�ysstem_id��Ա, ��¼���Ƿ�����һ��������
		
		if (part->system_id == FS_NOT_VALID) //��һ���Ҹо��Ǳ��ص�д��, ��Ϊ���ߵ���һ��, һ�㶼˵����һ��������ָ��һ��С��Ƭ
		{
			break;
		}

		r_count++;

		//�������жϵڶ�������, ����++part
		if ((++part)->system_id != FS_EXTEND) //˵���ڶ�������Ϊ��, ������ָ���µķ�����
		{
			break;
		}

		//ȥ���ڶ�������˵�����·������λ��: ��������������disk����λ�� + �·��������������������λ�� = �·����������disk����λ��
		start_sector = ext_start_sector + part->relative_sectors;
	
	} while (1);

	*count = r_count;
	return FS_ERR_OK;
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
	u8_t extend_part_flag = 0;//2.3 �����Ǳ���
	u8_t start_sector[4];	//2.3 ����������, ��Ϊ4���������ñ�, �������ֻ��4����չ����,  һ����չ���������ж��ٸ�С��Ƭ����, ��ȷ��

	for (i = 0; i < MBR_PRIMARV_PART_NR; i++, part++)
	{
		if (part->system_id == FS_NOT_VALID)
		{
			continue;//����������һ��
		}
		else if (part->system_id == FS_EXTEND) //2.3 �ж��ǲ�����չ����
		{
			//�������չ��������ʼ�����ű���
			start_sector[i] = part->relative_sectors;//��Ե�������: ��һ��������, ĳ�����������÷����ĵ�һ�����������λ��
			//����Ӧ��flag���ó�1
			extend_part_flag |= 1 << i;
		}
		else
		{
			r_count++;
		}
	}

	//2.3 ����չ����������, ����:4���������ñ�, �������ֻ��4����չ����, һ����չ���������ж��ٸ�С��Ƭ����, ��ȷ��
	if (extend_part_flag) {
		for (i = 0; i < MBR_PRIMARV_PART_NR; i++) {
			if (extend_part_flag & (1 << i)) {
				//˵����i����������չ����

				u32_t ext_count = 0;//����i�����������ж���С��Ƭ������������¼
				err = xdisk_get_extend_part_count(disk, start_sector[i], &ext_count);
				if (err < 0) {
					return err;
				}

				r_count += ext_count;
			}
		}
	}
	*count = r_count; //����Ǻܾò�д��, ��Ϊcount�ǵ�ַ, ����*count��ָ���Ǹ���ַ
	return FS_ERR_OK;
}

//2.4 ��ȡ��չ�����е���Ϣ
static xfat_err_t xdisk_get_extend_part(xdisk_t* disk, xdisk_part_t* disk_part, u32_t start_sector, int part_no, u32_t* count)//disk�����Ǹ�����Ϣ, disk_part�Ǵ���ַȡֵ�������Ҫ��С��Ƭ����Ϣ, start_sector:�����չ���������disk��ʼ��λ��λ��,  part_no����Ҫ���ǵڼ���С��Ƭ, count����ַȡֵ���://���������count��ָ, �ҵ�����Ҫ��С��Ƭ��, �Ѿ������˼���С��Ƭ(����Ҫ����)
{
	
	u8_t* disk_buffer = temp_buffer;//��ȫ�ֻ���
	xfat_err_t err = FS_ERR_OK;
	u32_t ext_start_sector = start_sector;//��Ϊ֮��start_sector��ֵ�ᾭ����, �������ﱣ��һ����ʼֵ. start_sector֮���������: 512�ֽڵ�mbr(��ָ��ĳ��С��Ƭ)���������disk��λ��
	int curr_no = -1; //���ָ���ǵڼ���С��Ƭ

	//����
	do {
		//��Ȼ֪��С��Ƭ��λ��, �Ȱѽ������buffer��
		err = xdisk_read_sector(disk, disk_buffer, start_sector, 1);//ֻ��һ������, Ҳ����ǰ512�ֽ�

		//��512�ֽ���, ����������Ҫ��64�ֽڷ�����, ����ֻ��ע:��һ����������, �ڶ�����������
		mbr_part_t* part;
		part = ((mbr_t*)disk_buffer)->part_info;//��ʱpart��ָ���˵�һ���������õ��׵�ַ

		//����: ��һ���������õ�relative_sectorָ����С��Ƭ(ע��: С��Ƭ�ͱ�����һ������). �ڶ����������õ�relative_sectorָ������һ��512�ֽڵ�mbr
		//����������: �������ж�, ��һ������������û��ָ��С��Ƭ
		if (part->system_id == FS_NOT_VALID)//˵��û��ָ��С��Ƭ, ˵���Ѿ�������, ������֮ǰ˵��, ����һ�����ص�д��, һ���ߵ���һ��, �����ܹ���֤��С��Ƭ��
		{
			break;
		}

		//�����С��Ƭ�ǲ����û�Ҫ��. ����, ����part_no���û���Ҫ�ĵ�0123��С��Ƭ, Ȼ��curr_no�ǵ�ǰ�ߵ��ڼ���С��Ƭ, ���Կ������Ƿ����
		if (++curr_no == part_no) {
			disk_part->type = part->system_id;
			disk_part->start_sector = start_sector + part->relative_sectors; //todo, start_sectorӦ���ǵ�ǰָ��С��Ƭ�ǿ�512�ֽ����������disk��λ��(��Ϊstart_sector��һֱ�ۼӵ�), Ȼ��relative_sector��С��Ƭ������ǿ�512�ֽڵ�λ��, ����������С��Ƭ�����disk��λ��
			disk_part->total_sector = part->total_sectors;
			disk_part->disk = disk;
			break;//��Ȼ�ҵ���, �˳�while
		}

		//�ߵ�����, ˵����Ȼ�ҵ�һ��С��Ƭ, ���ǲ�������Ҫ��С��Ƭ, ���Ǽ�������
		//������, ����Ҫ�ҵ���һ����512�ֽڵ�mbr, ������ǵڶ�����������(++part)ָ��ĵط�
		if ((++part)->system_id != FS_EXTEND)//���û����һ��mbr��,�˳�
		{
			err = FS_ERR_EOF;//end of file
			break;
		}

		//����һ��mbr,����Ҫ��ȡ���mbr�ĵ�ַ
		start_sector = ext_start_sector + part->relative_sectors; //��һ��mbr���׵�ַ�����disk��λ�� + ��һ��mbr��������mbr��λ��(ע��, �����part�Ѿ�ָ���˵ڶ�����������)
	}while(1);

	*count = curr_no + 1;//���������count��ָ, �ҵ�����Ҫ��С��Ƭ��, �Ѿ������˼���С��Ƭ(����Ҫ����)
	return err;
}

//2.4 ��ȡ������Ϣ
xfat_err_t xdisk_get_part(xdisk_t* disk, xdisk_part_t* xdisk_part, int part_no)
{
	u8_t* disk_buffer = temp_buffer;
	mbr_part_t* mbr_part;
	int i;
	int curr_no = -1; //��⵽�˷����ĸ���

	//��ȡmbr
	int err = xdisk_read_sector(disk, disk_buffer, 0, 1);
	if (err < 0) return err;

	//�����м�64�ֽڵķ�����
	mbr_part = ((mbr_t*)disk_buffer)->part_info; //����ָ���һ����������

	//����: ��234����������
	for (i = 0; i < MBR_PRIMARV_PART_NR; i++, mbr_part++) {
		//��Ч�Ϳ���һ��
		if (mbr_part->system_id == FS_NOT_VALID) {
			continue;
		}

		//�ж��ǲ�����չ����
		if (mbr_part->system_id == FS_EXTEND) {
			//��չ�������м�����Ƭ����
			u32_t count = 0;

			err = xdisk_get_extend_part(disk, xdisk_part, mbr_part->relative_sectors, part_no - i, &count);//�����disk����Ϣ, xdisk_part�Ǵ���ַȡֵ, mbr..��˵����ʼλ��, part_no - i(�������������part_no��3, ʵ�ʵ���0, 1, (2,3), 4, ����i = 2����(2,3), ����Ҳ�����������չ�����ĵ�3-2=1����Ƭ����, count:����ַȡֵ)
			if (err < 0) return err;

			if (err == FS_ERR_OK)//˵������Ҫ��part_no������ȷ����չ����i��
			{
				return err;
			}
			else 
			{
				curr_no += count;//todo

				//��ʦ˵, xdisk_get_extend_part()���޸�disk_buffer, ����������temp_buffer. ����Ҫ���¶�ȡ
				err = xdisk_read_sector(disk, disk_buffer, 0, 1);
				if (err < 0) return err;

			}
		}
		else {
			//����: ��ǰ������
			if (++curr_no == part_no) //��Ϊ:�ҵ�������
			{
				xdisk_part->type = mbr_part->system_id; //֮���Կ�������, ��Ϊsystem_id��01����, Ȼ��������xfs_type_t���Ѿ�������01���ֶ�Ӧ��Ӣ����ʲô
				xdisk_part->start_sector = mbr_part->relative_sectors; //��ʼ����
				xdisk_part->total_sector = mbr_part->total_sectors;
				xdisk_part->disk = disk;
				return FS_ERR_OK;
			}
		}
	}

	//����ߵ�������, ˵������Ҫ�ķ���û���ҵ�
	return FS_ERR_NONE;
}






