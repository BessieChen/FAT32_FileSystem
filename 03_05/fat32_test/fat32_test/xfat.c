#include "xfat.h"
#include "xdisk.h" //��Ϊfat������disk֮��
#include "stdlib.h" //��Ϊ��malloc()

//3.2 ʹ��ȫ�ֱ���u8_t temp_buffer[512], ��Ϊ�����temp_buffer��xdisk.c��, ��������Ҫ���ļ�ʹ�õĻ�, Ҫô��#include "xdisk.h", Ҫô��extern, �����������ᱨ��,��Ϊ����������ȫ�ֱ���������temp_buffer
extern u8_t temp_buffer[512];

//3.3 �궨��, ֮���Խ�����������óɺ궨��, ��Ϊ��������Ĺ��ܼ�, �ú��һ��
#define xfat_get_disk(xfat) ((xfat)->disk_part->disk) //Ҳ���Ǹ���һ��xfat�ṹ, �ҵ�����fat32����, �ڴ�fat32�����ҵ������ڵ�disk����ʼ��ַ

//3.2 ��ȡfat32�����ĵڶ�������: fat��, �ѹؼ���Ϣ������xfat��
static xfat_err_t parse_fat_header(xfat_t* xfat, dbr_t* dbr) //���ﴫ���˱�����dbr����Ϣ, ��Ϊdbr��ʵ�ǰ�����fat��������������Ϣ��. ע��, xfat��ֻ��xfat->disk_part��ӵ�����ݵ�, ���඼�Ǵ���ַȡֵ
{
	xdisk_part_t* xdisk_part = xfat->disk_part; //���fat���������ĸ�fat32����
	
	//��xfat�洢�ؼ���Ϣ:
	//3.3 ��ʼ����Ŀ¼�ĵ�һ���غ�
	xfat->root_cluster = dbr->fat32.BPB_RootClus; //ֱ�Ӵ�fat32.xxx���

	xfat->fat_tbl_sectors = dbr->fat32.BPB_FATSz32; ////ÿ��fat��ռ�ö��ٸ�����. ��Ϊdbr�д洢��fat����Ϣ

	//fat���Ƿ���ӳ��:
	/*
	1. ���Ǵ� dbr->fat32.BPB_ExtFlag����, ��һ��2���ֽ�, 8bits
	2. ��7��bit
		1. �����0, ˵���Ǿ���(Ҳ����ȫ��fat��һ��, ����Ϊ�˱���, ��Ϊ��ʱ���޸�һ��fat�������ϵ�, ����ͨ����������ָ�)
		2. �����1, ˵��ֻ��һ��fat����ʹ��. ��������һ����ʹ��(�״̬)
			1. �鿴��0-3bit
			2. ��4��bit���Դ���16��ֵ, ˵�����ǿ�����16��fat��
			3. ��4��bit�����0-15��ֵ, ����˵���ĸ�fat������ʹ��
				����0011, ˵��index==3�ı�����ʹ��
			ע��:
				1. ��֮ǰ��Ϊ������bit�ı�ʾ����, ��0011��ʾ��0���͵�1��fat����ʹ��
				2. ��Ϊ�����Ѿ�˵��, ֻ��һ��fat����ʹ��, �������ﲻ�ǿ��ĸ�bitΪ1, ����4��bit����������Ƕ���
	*/
	if (dbr->fat32.BPB_ExtFlags & (1<<7) )//˵����7��bit��ֵ��1, ˵�����Ǿ���, ��ҪѰ�����ĸ�fat���ǻ��
	{
		u32_t table = dbr->fat32.BPB_ExtFlags & 0xF; //��0��15�ĸ������ڻ. ����ȡ���4��bit, ��ʵ�Ҿ�������������ó�u8_t table;
		xfat->fat_start_sector = xdisk_part->start_sector + dbr->bpb.BPB_RsvdSecCnt + table * xfat->fat_tbl_sectors;// ������fat�������disk��ʼλ��, �ǵڼ�������: ���fat32��������ʼ������ + ������ռ�˼�������(dbr->bpb.BPB_RsvdSecCnt) + �����fat���ǵڼ���fat�� * һ��fat��ռ�������� //ע��,���������fat���ǵ�0��, ��ô���һ��ȫΪ0, ����Ҳ�Ǵ�0��ʼindex�ĺô�
		xfat->fat_tbl_nr = 1; //ֻ��һ��fat��
	}
	else//˵���Ǿ���, ȫ��fat���
	{
		xfat->fat_start_sector = xdisk_part->start_sector + dbr->bpb.BPB_RsvdSecCnt; // ������fat�������disk��ʼλ��, �ǵڼ�������: ���fat32��������ʼ������ + ������ռ�˼�������(dbr->bpb.BPB_RsvdSecCnt)
		xfat->fat_tbl_nr = dbr->bpb.BPB_NumFATs; //��ʵbpb����ͼ�¼���ж��ٸ�fat��
	}

	xfat->total_sectors = dbr->bpb.BPB_TotSec32; //ȫ��fat��һ��ռ�˶�������, ��������ֱ�ӵ���bpb��ʫ��
	xfat->sec_per_cluster = dbr->bpb.BPB_SecPerClus;//todo
	xfat->cluster_byte_size = xfat->sec_per_cluster * dbr->bpb.BPB_BytesPerSec; //����һ����ռ�õ��ֽ���= һ������ռ�õ��ֽ��� * һ�����ж��ٸ�����


	return FS_ERR_OK;
}

//3.2 ����һ������, ʵ��fat���Ĵ򿪹���, todo: ֮ǰ����xdisk_part_t�ṹ���ע����fat32�ķ���, Ҳ������4���������õ�ָ����ĸ�����������һ��
xfat_err_t xfat_open(xfat_t* xfat, xdisk_part_t* xdisk_part)//xdisk_part: ��xdisk_part���fat32����, xfat�Ǵ���ַȡֵ
{
	//3.2 ����:
	/*
	1. disk
	2. _mbr_t��diskǰ512�ֽڵ�mbr
	2. _mbr_part_t��mbr�е�4����������
	3. _mbr_part_t->relative_sectorָ���˵�һ��fat32����, ��������λ��ָ����fat32������mbr��λ��(������fat32��������ͨ�ķ���,��ô������λ�þ��Ǿ���λ����Ϊmbr���Ǵӵ�0�鿪ʼ��. ������fat32��������չ����,��ô��չ�����ĵ�һ���齫����һ��mbr, ��ô���xx->relative_sectorӦ�������mbr��������ȫ��mbr��λ��, Ȼ��֮�����mbr��Ҳ��������������, ���е�һ���������õ�relative_sectorҲ����С��Ƭ�����������mbr����Ծ���, �������С��Ƭ��disk�еľ��Ծ���, ����Ҫ�������mbr����ȫ��mbr�ľ���)
	4. ���fat32������ǰxx�ֽ��Ǳ�����, ��dbr_t�ṹ���ʾ
		1. ע�����dbr���Ѿ�������: 1. bios������ 2. fat����һЩ��Ϣ(ע��, ���Ҫ������xfat_t�ṹ, ����6��)
	5. fat32���������������ǵ�fat��, �����������
	6. ע�����ǲ�����xfat_t������Ϊ��������ȡfat���Ĺؼ�����, ����ӳ�䵽disk��
	*/

	xdisk_t* disk = xdisk_part->disk; //���ǵ��ĸ�fat32����,�����������ĸ�disk
	dbr_t* dbr = (dbr_t*)temp_buffer;//��ȫ�ֱ���, �����Ǳ�����dbrʹ��
	xfat->disk_part = xdisk_part; //��¼�����Źؼ���Ϣ��xfat�����������ĸ�fat32����, ���������parse_fat_header()��
	xfat_err_t err;


	//����,����Ҫ��fat32�����ĵ�һ������: ������, �����浽dbr_t*��, �����Ǵ���ַ��ֵ
	err = xdisk_read_sector(disk, (u8_t*)dbr, xdisk_part->start_sector, 1); //Ҳ������disk�ж�ȡ, ��ʼ��ַ��xdisk_part->start_sector, ��һ������, ��������������ǵ�dbr������
	if (err < 0) return err;

	//��һ���Ƕ�ȡ������, ����Ϣ�浽dbr��. �����ǽ�dbr�й���fat����Ϣ, �浽xfat��, ��Ϊxfat�����fat���Ĺؼ���Ϣ
	err = parse_fat_header(xfat, dbr);
	if (err < 0) return err;

	//3.4 ����һ������, ��һ��fat��
	xdisk_t* xdisk = xdisk_part->disk;
	xfat->fat_buffer = (u8_t*)malloc(xfat->fat_tbl_sectors * xdisk->sector_size); //һ��fat��������� * һ�����������ֽ�
	//����:
	/*
	�������Ĵ�:
		1. һ������2kb, һ��������512b, ����һ������4������
		2. һ��Ŀ¼����32b, һ��������������16��Ŀ¼��, һ������������16*4 = 64��Ŀ¼��
			1. һ��Ŀ¼���Ӧ��һ��Ŀ¼�����ļ�����Ϣ
			2. ����һ����, �������ֻ����64���ļ�/Ŀ¼����Ϣ, ̫����
			3. �����Ҫ����, ��Ҫ����ʹ��������
				1. ���Ǵز�������ʹ�õ�, һ���غ�==10�Ĵ�, ������һ���صĴغſ�����99
				2. ��ô�������¼�������ӵĹ�ϵ��? �Ǿ�����fat���fat�����м�¼
	fat��
		0. fat��Ľṹ����fat_buffer(������ȫ��fat�������Ϣ)�����4���ֽ�, ��4�ֽڽ���һ��(fat����), ����Ӧ���������е�һ����. ����fat_buffer����4n���ֽ�, ˵����������n����(���ǲ���������ر�ʹ��)
		1. ÿ4���ֽ�, ������һ��fat32����, ��������fat_buffer����һ������, ÿ��Ԫ����4�ֽ�
			1. 4�ֽڵ�����:
				1. 0000 0000, ˵�����4�ֽڶ�Ӧ���Ǹ��������Ĵ�, �ǿյ�
				2. 0000 0002 �� ffff ffef, ˵�����4�ֽڶ�Ӧ�Ĵ�, ��һ���ض�Ӧ��fat_buffer��4�ֽڵ�index�����0000 0002 �� ffff ffef
					1. ֮���Դ�2��ʼ, ��Ϊfat_buffer�е�ǰ8���ֽ�, Ҳ����2 * 4�ֽ�, ������������;��
					2. ע��, ���ǿ�4�ֽڵ�ʱ��, ǰ4bit(Ҳ����ǰ����ֽ�)�Ǻ��Բ�����, ����index�ķ�ΧӦ���Ǵ� 000 0002 �� fff ffef
						˵��fat_buffer�еĴ���ص�4�ֽ���: (fff ffef - 2) + 1��
				3. ffff fff0 �� ffff fff6, ϵͳ����
				4. ffff fff7, �������4�ֽڶ�Ӧ�Ĵ��ǻ���
				5. ffff fff8 -> ffff ffff ������, ˵����ʱû����һ������
			2. ע��: ����˵��4�ֽڶ�Ӧ��������һ����, ��Ӧ��ʽӦ����: ����ַ��Ӧ
				fat_buffer�е�ǰ2��4�ֽ�(��8�ֽ�), �Ǳ�����, ����(��һ������ص�)4�ֽ�, index == 2(base 0)
				�صĴغ�Ҳ�Ǵ�2��ʼ��
					����: 
						4�ֽ�, Ҳ����fat������fat���е�index, Ҳ���Ǵ����������е�index(Ҳ�����غ�)
						4�ֽ����������, �����Ǹ��ص���Ϣ: �ǿ�, ����, ��������һ����
	 */

	//3.4 ��ȡ����xfat�������fat��
	err = xdisk_read_sector(xdisk, (u8_t*)xfat->fat_buffer, xfat->fat_start_sector, xfat->fat_tbl_sectors); //��3������: xfat������ĵ�fat�����ʼ����(����λ��)
	if (err < 0) return err;

	//


	return FS_ERR_OK;
}

//3.3 ����xfat�ṹ����Ϣ, �����������е�cluster_no���صľ���disk��ʼλ�õ�������
u32_t cluster_first_sector(xfat_t* xfat,u32_t cluster_no)
{
	//��������������ʼ������ = fat���ĵ�һ��������(fat_start_sector) + fat���м���fat��(fat_tbl_nr) * fat���м�������(fat_tbl_sectors)
	u32_t data_start_sector = xfat->fat_start_sector + xfat->fat_tbl_nr * xfat->fat_tbl_sectors; //ע��, ������ܻ��д�. ��Ϊ��ʦ��disk��ʹ�þ���, ����fat_start_sector�պþ���fat������ʼ����(��line40: xfat->fat_start_sector = xdisk_part->start_sector + dbr->bpb.BPB_RsvdSecCnt; // ������fat�������disk��ʼλ��, �ǵڼ�������: ���fat32��������ʼ������ + ������ռ�˼�������(dbr->bpb.BPB_RsvdSecCnt)
	
	//����cluster_no��ָ�������еĵڼ�����, �����������Ĵ�(��ָ���ĵ���)�Ǵ�2��ʼ������(��Ϊ0��1���ڱ���ô�), ��������Ҫ����ƫ��2��index
	u32_t sectors = (cluster_no - 2) * xfat->sec_per_cluster; //�ظ��� * ����������

	return data_start_sector + sectors;
}

//3.3 ��ȡ�ص�����
xfat_err_t read_cluster(xfat_t* xfat, u8_t* buffer, u32_t cluster, u32_t count) //xfat: �����ṩһЩ��Ϣ(�����ĸ�disk,�ĸ�fat32����..), buffer: ��ȡ�Ľ���浽buffer, ����ַȡֵ, cluster: �����fat32���������������ĸ��غſ�ʼ��ȡ, count:������ȡ������
{
	xfat_err_t err;
	u32_t i;

	u8_t* curr_buffer = buffer; //curr_buffer: �����ǵ�ǰ��buffer
	u32_t curr_sector = cluster_first_sector(xfat, cluster);//��ǰ��������, ��ʼ����ʱ��, ֵ�ǵ�cluster���صĵ�һ��������(����disk��ʼλ�õ�������). ��������ֻ�д�, ������Ҫ����
	

	for (i = 0; i < count; i++) {
		
		//��ȡ���̽ṹ, �ú궨��
		xdisk_t* disk = xfat_get_disk(xfat);

		//��ȡ
		err = xdisk_read_sector(disk, curr_buffer, curr_sector, xfat->sec_per_cluster); //�ĸ�disk, ����浽curr_buffer, ��������curr_sector��ʼ��, ��xfat->xx��������(��Ϊһ�����ж��ٸ�����, �Ͷ����ٸ�)
		if (err < 0) return err;


		//����buffer�ĵ�ֵַ, Ȼ��غŵ�ֵ
		curr_buffer += xfat->cluster_byte_size; //ע��, ������һ����ռ��xx���ֽ�, ���Ե�ַ��Ҫ+=xx��, ��Ϊ���u8_t*, �ǵ�ַ
		curr_sector += xfat->sec_per_cluster; //��������Ҫ����һ���ص�������, ���Ը������µ�����
	}
	return FS_ERR_OK;
}

//3.4 �жϴغ��ǲ�����Ч��. ���cluster_no��ffff fff8 �� ffff ffff ���ֵҲ����Ч��, ��Ϊ���ʾ����
int is_cluster_valid(u32_t cluster_no) {
	//ֻȡ��28λ:
	cluster_no &= 0x0fffffff;

	//index�ķ�ΧӦ���Ǵ� 000 0002 �� fff ffef, �������Χ֮��ľ��ǷǷ���index(�൱�ڴغ�). ע��, <= 0fff ffef ���� < 0fff ffff
	return (cluster_no < 0x0ffffff0) && (cluster_no >= 2);
}

//3.4 ��ȡ��һ���غ�
xfat_err_t get_next_cluster(xfat_t* xfat, u32_t curr_cluster_no, u32_t* next_cluster) {
	//���жϵ�ǰ�Ĵغ��ǲ�����Ч��
	if (is_cluster_valid(curr_cluster_no)) {
		cluster32_t* cluster32_buf = (cluster32_t*)xfat->fat_buffer; //����fat_buffer����4n\���ֽ�, ˵����������n����. ��Ϊcluster32_t������Ĵ�С��32bit, Ҳ����4���ֽ�, ���Խ�fat_buffer��������ݲ����4�ֽ�4�ֽڵ�
		*next_cluster = cluster32_buf[curr_cluster_no].s.next; //��Ϊ����֮ǰ�õ���ָ��cluster32_t* , �������ڿ��Ե���������ʹ��, ���ǵĴغž��൱��������������, Ȼ������Ҫ��������32bit�еĵ�28bit, ������.s.next
	}
	else { //��ǰ�غ���Ч, ��ô��һ���غ�Ҳ��Ч
		*next_cluster = CLUSTER_INVALID; //��xfat.h
	}

	return FS_ERR_OK;
}
