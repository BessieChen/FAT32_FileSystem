#include "xfat.h"
#include "xdisk.h" //��Ϊfat������disk֮��
#include "stdlib.h" //��Ϊ��malloc()
#include <string.h> //memcmp()
#include <ctype.h> //toupper()

//3.2 ʹ��ȫ�ֱ���u8_t temp_buffer[512], ��Ϊ�����temp_buffer��xdisk.c��, ��������Ҫ���ļ�ʹ�õĻ�, Ҫô��#include "xdisk.h", Ҫô��extern, �����������ᱨ��,��Ϊ����������ȫ�ֱ���������temp_buffer
extern u8_t temp_buffer[512];

//3.3 �궨��, ֮���Խ�����������óɺ궨��, ��Ϊ��������Ĺ��ܼ�, �ú��һ��
#define xfat_get_disk(xfat) ((xfat)->disk_part->disk) //Ҳ���Ǹ���һ��xfat�ṹ, �ҵ�����fat32����, �ڴ�fat32�����ҵ������ڵ�disk����ʼ��ַ
//4.2 �ж�ch�Ƿ��Ƿָ���: '/'���� '\'
#define is_path_sep(ch)		( ((ch) == '\\') ||  ((ch) == '/') ) //����\\�Ƿ�б��/, 
//4.2 
#define to_sector(disk, offset) ((offset) / (disk)->sector_size) //����sector_sizeָ����disk��ÿ�������Ĵ�С(�ֽ���), ���ǵ���������512�ֽ� ////��ʼ�ҵĵط�, �Ǵ�����ĵڼ�������, ʹ�ú꺯��
#define to_sector_offset(disk, offset)  ((offset) % (disk)->sector_size) //��ʼ�ҵĵط�, ����������ľ����ĸ��ֽ�
//4.4 
#define to_cluster_offset(xfat, pos) ((pos) % (xfat->cluster_byte_size)) //posλ��, ����ڴ��ڲ���ƫ��λ��
//4.8 ��ȡfile���ڵ�disk
#define file_get_disk(file)		((file)->xfat->disk_part->disk)



//3.2 ��ȡfat32�����ĵڶ�������: fat��, �ѹؼ���Ϣ������xfat��
static xfat_err_t parse_fat_header(xfat_t* xfat, dbr_t* dbr) //���ﴫ���˱�����dbr����Ϣ, ��Ϊdbr��ʵ�ǰ�����fat��������������Ϣ��. ע��, xfat��ֻ��xfat->disk_part��ӵ�����ݵ�, ���඼�Ǵ���ַȡֵ
{
	xdisk_part_t* xdisk_part = xfat->disk_part; //���fat���������ĸ�fat32����
	
	//��xfat�洢�ؼ���Ϣ:
	//3.3 ��ʼ����Ŀ¼�ĵ�һ���غ�
	xfat->root_cluster = dbr->fat32.BPB_RootClus; //ֱ�Ӵ�fat32.xxx���

	xfat->fat_tbl_sectors = dbr->fat32.BPB_FATSz32; //ÿ��fat��ռ�ö��ٸ�����. ��Ϊdbr�д洢��fat����Ϣ

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


//4.10 �����غźʹ���ƫ��, ������Ե������� //������warning,��Ϊ��֮ǰ���������������//3.3 cluster_first_sector()�������ǰ��
u32_t to_phy_sector(xfat_t* xfat, u32_t cluster, u32_t cluster_offset) {
	xdisk_t* disk = xfat_get_disk(xfat);
	return cluster_first_sector((xfat), (cluster)) + (u32_t)to_sector((disk), (cluster_offset));
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

//4.2 ����·���ķָ���: '/'
static const char* skip_first_path_sep(const char* path) {
	const char* c = path;
	
	while(is_path_sep(*c))//���c��һ���ָ���. ����is_path_sep��һ���꺯��
	{
		c++; //�����ǰcָ����Ƿָ���, �������ָ�������һ��
	}
	return c;//��ǰcָ��ľͲ��Ƿָ���
}

//4.2 ���������Ŀ���ǻ����Ŀ¼, ���紫�����/aa/bb/cc.txt, ��ô����ֵ��bb/cc.txt, ע��,��ǰ�涼�ǲ�/, ����aa/.., bb/...
static const char* get_child_path(const char* dir_path) {
	
	const char* c = skip_first_path_sep(dir_path);	//�������Ǵ����dir_path��ֵ��/a/b/c.txt, ��ô����������һ��(����ָ���)֮��,�ͻ���a/b/c.txt

	//�������Ǵ�a/b/c.txt���/b/c.txt
	while ((*c != '\0') && !is_path_sep(*c)) //��Ϊ����cָ����a/b/c.txt�е�a(�Ƿָ���), ����Ҫ����.//���ȿ�, c�ǲ��ǵ������ļ�����(�������), ֮�������ǲ���һ��ķ���aa,bb
	{
		c++;//���¼�����
	}

	//�˳�while��ʱ��, cҪô��ָ����\0, Ҫô��ָ����һ���ָ���
	return (*c != '\0') ? c + 1 : (const char*)0; //�����ָ��ָ���, �Ǿ�ָ����һλ(�����һλһ����һ���Ƿָ���, ��Ϊ�ָ���ֻռ1λ), ����ǽ�����, �Ǿͷ��ؿ�
}

//4.2 ��ȡ�ļ�����
static xfile_type_t get_file_type(const diritem_t* diritem) {
	xfile_type_t type;

	u8_t attr = diritem->DIR_Attr;
	if (attr & DIRITEM_ATTR_VOLUME_ID) //�ǲ��Ǿ��
	{
		type = FAT_VOL;
	}
	else if (attr & DIRITEM_ATTR_DIRECTORY) //Ŀ¼
	{
		type = FAT_DIR;
	}
	else {
		type = FAT_FILE;
	}

	return type;
}

//4.2 ��ȡ�ļ��Ĵغ�, ��16λ�͵�16λ���
static u32_t get_diritem_cluster(diritem_t* item) {
	return (item->DIR_FstClusHI << 16) | item->DIR_FstClusL0;
}

//4.3 �ļ���ת��: �����ǵ��ļ���123TXTת����8+3�ļ���. �������һ������01�Ǵ�Сд������(0��������: �ļ���, ��չ����Ϊ��д)
static xfat_err_t to_sfn(char* dest_name, const char* source_name, u8_t case_config) { //������ʦ˵case_config û���õ�, ���Ǽ���Ĭ�ϵĴ�д
	int name_len; //�ļ�������
	const char* ext_dot; //�ָ���
	const char* p;	//����ָ��
	char* dest = dest_name;
	
	int ext_existed;//��չ���Ƿ����

	//��dest_nameȫ�����, ��Ҫ���0, ������ո�
	memset(dest_name, ' ', SFN_LEN);

	//������б��, ��Ϊ��һ������ʱ��, ���ܻ��� aa/bb/cc/123.txt
	while (is_path_sep(*source_name)) {
		source_name++;
	}
	//����while֮��, source_nameָ����Ǹ��ַ�, ���ǷǷ�б��

	//�жϵ�ָ���������, ���ļ�������չ���ָ��
	ext_dot = source_name; //��ʼ��λmy_name�Ŀ�ͷ, ע��source_name��ָ��
	p = source_name;  //p���ڱ�����������
	name_len = 0;

	//��ʼ����, ��ʦ˵, ��һ����·��, �������while����Ƚ�, ���������ָ���������
	//��֮: ���while���ǽ�/xxx/�м��Ŀ¼��xxx, ����/xxx.txt���ļ���ȡ����, ���ǲ�����/��
	while ((*p != '\0') && !is_path_sep(*p)) 
	{
		if (*p == '.') {
			ext_dot = p;// ����չ���ָ���ָ��p
		}

		p++;
		name_len++; //���ȵļ���++
	}

	ext_existed = (ext_dot > source_name) && (ext_dot < (source_name + name_len - 1)); //��������, �����ļ����Ŀ�ͷ, С���ļ����Ľ���

	p = source_name; //ָ��source_name�Ŀ�ͷ
	int i;
	for (i = 0; (i < SFN_LEN) && (*p != '\0') && !is_path_sep(*p); i++) {
		if (ext_existed) {
			//���ļ��� + ��չ��
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
			//ֻ��Ŀ¼��
			*dest++ = toupper(*p++); //�ͽ�pָ�������ֱ�Ӽ���ͺ���
		}
	}

	return FS_ERR_OK;

}
//4.3 �޸� //4.2 �ļ�����ƥ��
static u8_t is_filename_match(const char* name_in_dir, const char* target_name) {
	//4.3 ת������ļ���
	char temp_name[SFN_LEN]; //����Ϊ8���ַ�����

	//4.3 tiny test case
	char* path = "open.txt";
	to_sfn(temp_name, path, 0);

	to_sfn(temp_name, target_name, 0); //�����ǵ��ļ���123TXTת����8+3�ļ���. ����0�Ǵ�Сд������(0��������: �ļ���, ��չ����Ϊ��д) //����Ƚϵ���, ת���������: temp_Name, ��������Ҫ������ target_name�Ƚ�


	return memcmp(name_in_dir, temp_name, SFN_LEN) == 0;//���ļ���һ��11�ֽڵıȽ�
}

//4.5 �жϵ�ǰĿ¼��������ǲ���������Ҫ��. ��ʦ��˼·: ����Ϊ����match��, һ���������attr������Ҫ��, �Ҿ���Ϊ�㲻match
//��ʦΪʲô��: ����Ϊ����match, ����Ϊmatch. ��ʵ������Ȳ�Match��matach��ΪӦ��Ҳ�ǿ��Ե�, 
static u8_t is_locate_match(diritem_t* dir_item, u8_t locate_type) {
	u8_t match = 1;
	//if (locate_type & XFILE_LOCATE_ALL) return match; //bug! ������ôд!, ��Ϊ����������locate_type��ʲô, ������ͨ��.

//�궨��
#define	DOT_FILE		".          " //char[12], Ҳ����8+3+һ��������
#define	DOT_DOT_FILE	"..         " //char[12], Ҳ����8+3+һ��������

	if ((dir_item->DIR_Attr & DIRITEM_ATTR_SYSTEM) && !(locate_type & XFILE_LOCATE_SYSTEM)) { //ʵ����ϵͳ�ļ� && �Ҳ�Ҫϵͳ�ļ�
		match = 0;
	}
	else if ((dir_item->DIR_Attr & DIRITEM_ATTR_HIDDEN) && !(locate_type & XFILE_LOCATE_HIDDEN)) { //ʵ����XX && �Ҳ�ҪXX
		match = 0;
	}
	else if ((dir_item->DIR_Attr & DIRITEM_ATTR_VOLUME_ID) && !(locate_type & XFILE_LOCATE_VOL)) {
		match = 0;
	}
	else if ((memcmp(DOT_FILE, dir_item->DIR_Name, SFN_LEN) == 0) //����dir_name��һ����
		|| (memcmp(DOT_DOT_FILE, dir_item->DIR_Name, SFN_LEN) == 0) )   //����dir_name��һ�����
	{
		if(!(locate_type & XFILE_LOCATE_DOT)) //������ǲ�Ҫ��/���, ��match
			match = 0;
	}
	else if (!(locate_type & XFILE_LOCATE_NORMAL)) //ʣ�µ�˵��dir_name����ͨ�ļ�(�ɶ�, ��д, ���ļ���...), ������ǲ�Ҫ��ͨ�ļ�, ͬ����match
	{
		match = 0;
	}
	return match;
}

//4.5 �޸�: ֻ����ָ�����ļ�(ͨ����־λ) //4.2 ���������ʵ���е㸴��. //�ӵ�dir_cluster���ؿ�ʼ��, ��ʼɨ��Ŀ¼��. ����cluster_offset���ֽ�����ƫ��, Ҳ����˵��ʼ�ҵĵط�, �ǴӴص���ʼλ�ÿ�ʼ��cluster_offset�ֽڸ�ƫ��. (��������ֽ�ƫ��: ���ǿ�������Ǵصĵڼ�������, ���Ǹ������еĵڼ����ֽ���)ɨ���Ŀ��: Path, �ҵ���, ���־�����ʼλ����Moved_bytes, Ȼ�����浽r_diritem
static xfat_err_t locate_file_dir_item(xfat_t* xfat, u32_t locate_type, u32_t* dir_cluster, u32_t* cluster_offset, const char* path, u32_t* moved_bytes, diritem_t** r_diritem) {
	
	//��ǰɨ������ĸ���
	u32_t curr_cluster = *dir_cluster;
	
	//����һ��disk,�Ժ���õ�
	xdisk_t* xdisk = xfat_get_disk(xfat);

	//����:
	/*
	1. ��Ϊ���ǵ�ȫ�ֻ���buffer��512�ֽ�, �������ֻ�ܱ���һ������������
	2. ����: ������һ������һ��������ɨ��.
	2. ����: һ�����ж������, һ�����������ж��Ŀ¼��
	3. ���ǲ�������offset(����ڴ���ʼλ��),��������Ҫ���: ��ʼ�ҵĵط�, Ӧ���Ǵ�����ĵڼ�������, �Լ�����������ĵڼ����ֽ�
	*/
	u32_t initial_sector = to_sector(xdisk, *cluster_offset); //��ʼ�ҵĵط�, �Ǵ�����ĵڼ�������, ʹ�ú꺯��
	u32_t initial_offset = to_sector_offset(xdisk, *cluster_offset); //��ʼ�ҵĵط�, ����������ľ����ĸ��ֽ�. ����ֽ�����Ӧ���ǵڼ���Ŀ¼��, ���Ǻ������:  initial_offset / sizeof(diritem_t), Ҳ���ǳ���Ŀ¼����ֽ���

	//ͳ�����˶���bytes, ��ʼֵ��0
	u32_t r_moved_bytes = 0;
	
	//ɨ�����, ��Ϊ��ʼ�Ĵ�������Ϊ������Ч��(����δ��ʹ��, ���߻���), ������do while, ������while
	do {
		xfat_err_t err;
		u32_t i;

		//���Ǻ���Ҫ�����ݱ��浽������, ���Ƕ�ȡ������Ҫ����ȫ�ֵ�������, ��������֪����ֻ������ڴص�������, ���Ծ��������� = �غ�*һ���ص��������� + ���������
		//��������: �ôصĵ�һ�������ľ���������
		u32_t start_sector = cluster_first_sector(xfat, curr_cluster); //curr_cluster�ǵ�ǰ�غ�, ����غ�����while����᲻�ϸ��µ�

		//���for loop�Ǳ���һ���ص���������
		for (i = initial_sector; i < xfat->sec_per_cluster; i++) //��intial_sector�����������ʼɨ��, һֱ��ɨ�赽һ���صĽ�β����
		{
			u32_t j;

			//��������ȡ��ȫ�ֻ���buffer(512�ֽ�)��, ��ȡһ������
			err = xdisk_read_sector(xdisk, temp_buffer, start_sector + i, 1);//start_sector + i��������˵��: ���������� = �صĵ�һ�������ľ��������� + ���������
			if (err < 0) return err;

			//���for loop�Ǳ���һ������������Ŀ¼��
			//��ȡ��, �����������һ����Ŀ¼����бȶ�, ���Ƿ������ǵ�Ŀ��path
			//ע��, offset��һ�������Ƕ�ȡ��buffer�ĵ�һ��Ŀ¼��, �������м��, ���������Ǵ�buffer�ĵ�offset��Ŀ¼�ʼ��ȡ, �����Ǵӵ�һ����ʼ��ȡ
			//�����for loop��: ƫ����offset(������ֽ���, ����Ҫ����Ŀ¼��directory_item: dir_item���ֽ�����С), һ��������Ŀ¼�������: һ���������ֽ���/һ��Ŀ¼����ֽ���
			for (j = initial_offset / sizeof(diritem_t); j < xdisk->sector_size / sizeof(diritem_t); j++) {

				//��ȡ��ǰ��Ŀ¼��, ��Ϊbuffer����ȫ����Ŀ¼��, ����ֻ��Ҫ�ҵ�j��
				diritem_t* dir_item = ((diritem_t*)temp_buffer) + j; //��ϲ��������,��Ϊ���ǰ�buffer�еĶ����ֳ���Ŀ¼��Ĵ�С, ������ָ��ļӼ�, ����+j�൱��λ����j*Ŀ¼��Ĵ�С

				if (dir_item->DIR_Name[0] == DIRITEM_NAME_END)//����û����Ч��Ŀ¼����
				{
					return FS_ERR_EOF;
				}
				else if (dir_item->DIR_Name[0] == DIRITEM_NAME_FREE)//����ǿ��е�,˵�����Լ�����, ���ǲ��������Ŀ¼����, ����J++��һ��Ŀ¼��
				{
					r_moved_bytes += sizeof(diritem_t); //����˵���Ѿ�����һ��, ����������Ҫ��, Ŀ¼��, �����ߵ��ֽ�����Ҫ��¼
					continue;
				}
				else if (!is_locate_match(dir_item, locate_type))//4.5 �ж��ļ������ǲ���������Ҫ��
				{
					r_moved_bytes += sizeof(diritem_t); //����˵���Ѿ�����һ��, ����������Ҫ��, Ŀ¼��, �����ߵ��ֽ�����Ҫ��¼
					continue;
				}

				//�����ǰ���ǿ��л�����Ч, �Ǿ�ȥ�����ǲ��Ǻ�����pathһ��. ����˵����: ��ȷ���ҵ���, bingo
				if ((path == (const char*)0) || (*path == 0) || is_filename_match((const char*)dir_item->DIR_Name, path))
				{
					u32_t total_offset = i * xdisk->sector_size + j * sizeof(diritem_t);//�ܵ�ƫ���ֽ��� = �Ӵ���ʼλ�ÿ�ʼ,ǰ���м������� * һ�����������ֽ� + ���ǵڼ���Ŀ¼�� * Ŀ¼����ֽ��� 
					*dir_cluster = curr_cluster;	//�غ�, �����ǵ�ǰ�Ĵغ�, ��������ڷ��ص�
					*cluster_offset = total_offset;	//���е��ֽ�ƫ��, ��������ڷ��ص�
					*moved_bytes = r_moved_bytes + sizeof(diritem_t); 

					if (r_diritem)//���������������һ����Ч��ָ��, ���Ǿ������洫������Ŀ¼��
					{
						*r_diritem = dir_item;
					}
					return FS_ERR_OK;
				}
			}
		}

		//����������һ����
		err = get_next_cluster(xfat, curr_cluster, &curr_cluster);
		if (err < 0) return err;

		//������,ƫ�Ƹ�����, ����һ���ؿ�ʼʹ��
		initial_sector = 0;
		initial_sector = 0;

	} while (is_cluster_valid(curr_cluster));

	//�����������еĴ�, ���ǻ���û���ҵ�
	return FS_ERR_EOF;
}
//4.2 �޸� //4.1 �򿪺���
static xfat_err_t open_sub_file(xfat_t* xfat, u32_t dir_cluster, xfile_t* file, const char* path)//��һ���ļ�, ����ļ��ĸ�Ŀ¼�Ĵغ���dir_cluster(Ҳ����˵, ȥ���dir_cluster����, ����������кܶ�Ŀ¼��, ����һ��Ŀ¼����������file, ����ͨ���ж�path���ĸ�Ŀ¼���·��һ��, ���ж����file�����ĸ�Ŀ¼��, �Ӷ��ҵ����file�Ĵغ�)
{
	u32_t parent_cluster = dir_cluster;
	u32_t parent_cluster_offset = 0;

	path = skip_first_path_sep(path); //������һ���ָ���, ����/abc.txt, ������һ���ָ���֮��ͱ����abc.txt
	if ((path != '\0') && (*path != '\0')) //���˸�Ŀ¼�����Ŀ¼
	{
		//����·��
		const char* curr_path = path;

		//�ҵ���diritem_t
		diritem_t* dir_item = (diritem_t*)0;

		//����غ�
		u32_t file_start_cluster = 0;

		while (curr_path != (const char*)0)  //���·��û�е�ͷ
		{
			//�ƶ����ֽ���
			u32_t moved_bytes = 0;

			//4.5 ���ó�: ֻҪdot�ļ�����ͨ�ļ�.Ϊʲô? ��Ϊ������Ҫ�ҵ�Ŀ¼������/read/./a/../b/c, �����Ҫdot�Ļ�, ��ᷢ��ִ�е�/read��ִ�в���ȥ��.  //�ҵ��ļ�����. ����Ǹ�Ŀ¼�µ��ļ�, ��Ҫ�Ӹ�Ŀ¼�Ĵ���(parent_cluster)��ʼ��
			xfat_err_t err = locate_file_dir_item(xfat, XFILE_LOCATE_DOT | XFILE_LOCATE_NORMAL, &parent_cluster, &parent_cluster_offset, curr_path, &moved_bytes, &dir_item);
			//����:
			/*
			1. parent_cluster: ���ڴغ�
			2. parent_cluster_offset: �ô����ڵľ���λ��(�Ҿ��ÿ������ֽ�Ϊ��λ), ������ָ��ʼ�ҵľ���λ��, moved_bytesָ�����ҵ���λ�þ�����ʼ�ҵ�λ�õ�ƫ����
			3. curr_path: ������֤, �ҵ��Ķ����ǲ��Ǻ�����Ҫ�ҵ�·����ƥ���
			4. moved_bytes: moved_bytesָ�����ҵ���λ�þ�����ʼ�ҵ�λ�õ�ƫ����
			5. dir_item: �ҵ��Ľ��(�ļ�����Ϣ:��С, ����, λ��..)
			*/
			if (err < 0) return err;

			//�쳵diritem�Ƿ���Ч
			if (dir_item == (diritem_t*)0)//˵����Ч
			{
				return FS_ERR_NONE;
			}

			//4.5  /xxx/..��, ����..֮��, �غŻ��ɸ�Ŀ¼�ĵ�һ����, Ҳ������Դغ���0
			//4.5 ����ע��, �����Ŀ¼�Ǹ�Ŀ¼, ��ô��һ���ؽ���2. ����read����һ���Ǹ�Ŀ¼, ����/read/.., ����..֮��, �غŻ��ɸ�Ŀ¼(��Ŀ¼)�ĵ�һ����, Ҳ������Դغ���2
			//4.5 �޸�, ���µ�else
			


			//��ȡ��·��
			curr_path = get_child_path(curr_path); //����֮ǰcurr_path��a/b/c.txt, ����curr_path���b/c.txt
			if (curr_path != (const char*)0) { //˵��������Ŀ¼
				parent_cluster = get_diritem_cluster(dir_item); //��Ŀ¼��, ��ȡ��Ӧ�Ĵغ�
				parent_cluster_offset = 0;
			}
			else { //�Ѿ������һ��c.txt��?
				//��¼�ļ��Ĵغ�:
				file_start_cluster = get_diritem_cluster(dir_item); //��¼�ļ��Ĵغ�

				//4.5 �ж��Ƿ���..���������ļ���. ����������if(){}����, ��ӡ/read/..��ʱ��, �������������. ��Ϊ��==0������, ��Ŀ¼�Ĵ��Ǵ�2��ʼ.
				if ((memcmp(DOT_DOT_FILE, dir_item->DIR_Name, SFN_LEN) == 0) && (file_start_cluster == 0)) //�����..�ļ��� && ������һ���Ǹ�Ŀ¼, ����/read/.., ��Ϊread�Ǹ�Ŀ¼��, ����..���ص���һ��Ҳ���Ǹ�Ŀ¼
				{
					file_start_cluster = xfat->root_cluster;
				}
			}
		}

		file->size = dir_item->DIR_FileSize;
		file->type = get_file_type(dir_item); //��Ȼdir_item������Attr����, ������ͨ��8��bit����ʾ��, ���������ú�����ȡ
		file->start_cluster = file_start_cluster;
		file->curr_cluster = file_start_cluster;
	}
	else { //˵��Ҫ�ҵ��Ǹ�Ŀ¼: '/'
		file->size = 0;
		file->type = FAT_DIR;
		file->start_cluster = dir_cluster;
		file->curr_cluster = dir_cluster;
	}

	//��һ���ǹ�ͬ��
	file->xfat = xfat;
	file->pos = 0;
	file->err = FS_ERR_OK;
	file->attr = 0;

	return FS_ERR_OK;
}
//4.5 �޸�, ��������˴���·�� //4.1 �ļ���
xfat_err_t xfile_open(xfat_t* xfat, xfile_t* file, const char* path) {

	//4.5 ����·��: /.., ��Ŀ¼û����һ��Ŀ¼
	path = skip_first_path_sep(path);//������ͷ��/��б��
	if (memcmp(path, "..", 2) == 0) {
		return FS_ERR_PARAM; //���������.., ˵��ԭ����/.., Ҳ���Ǵ������
	}
	else if (memcmp(path, ".", 1) == 0) {
		path++; //���������., ˵��֮ǰ��/., ֱ�������ͺ���
	}

	return open_sub_file(xfat, xfat->root_cluster, file, path); //��Ϊ���������ǽ���Ŀ¼����Ҫ�ҵ�file ,���Ǹ�Ŀ¼û�и�Ŀ¼, ���Եڶ�������(��Ŀ¼�Ĵغ�), ���Ǿ�ֱ���ø�Ŀ¼�ĴغŴ���

}

//4.6 ����Ŀ¼. ���ڲ�֧��: ͬһ��·�����ļ�, �򿪶��
xfat_err_t xfile_open_sub(xfile_t* dir, const char* sub_path, xfile_t* sub_file) //��ǰĿ¼:dir, Ҫ�򿪵���·��: sub_path, ��·�����ļ�: sub_file
{
	//4.5 ����·��: /.., ��Ŀ¼û����һ��Ŀ¼
	sub_path = skip_first_path_sep(sub_path);//������ͷ��/��б��

	//4.5 ɾ�������, ��Ϊ����������Ŀ¼, ���Կ϶����ڸ�Ŀ¼, ���Բ��õ���/..���������
	/*if (memcmp(path, "..", 2) == 0) {
	//	return FS_ERR_PARAM; //���������.., ˵��ԭ����/.., Ҳ���Ǵ������
	}*/

	//4.5 ��ʦ˵, ��Ϊdir���Ѿ��򿪵���, ���ǲ�ϣ���ٴ�һ��, ������Ϊ /dir/.�ǲ�������
	if (memcmp(sub_path, ".", 1) == 0) {
		return FS_ERR_PARAM;
	}

	//����Ϊ: ��������Ϣ��������xfat��, ��Ŀ¼�ĸ�Ŀ¼�Ĵغű�������dir->start_cluster��
	return open_sub_file(dir->xfat, dir->start_cluster, sub_file, sub_path); //��Ϊ���������ǽ���Ŀ¼����Ҫ�ҵ�file ,���Ǹ�Ŀ¼û�и�Ŀ¼, ���Եڶ�������(��Ŀ¼�Ĵغ�), ���Ǿ�ֱ���ø�Ŀ¼�ĴغŴ���

}


//4.1 �ļ��ر�
xfat_err_t xfile_close(xfile_t* file)
{
	return	FS_ERR_OK;
}

//4.4 ����ʱ��
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

//4.4 ���û��ӽǵ��ļ���, ת�����ڲ����ļ���
static void sfn_to_myname(char* dest_name, const diritem_t* diritem) { //���е���Ϣ: dititem�е��ڲ�����123     ABC, ת�����û�����123.abc����dest_name��
	//����ָ��, ָ����������, ���Ե�ʱ�򷽱�۲�
	char* dest = dest_name, *raw_name = (char*)diritem->DIR_Name;

	//�ж���չ���Ƿ����, ���濽����ʱ����õ�
	u8_t ext_exist = (raw_name[8] != 0x20);//��raw_name�ĵھŸ��ֽ�(index==8), ������ǿո�(!= 0x20), ˵����չ������. //�����ȥDIR_Name����, �ڰ˸��ֽ�, ������չ��u8_t Dir_ExtName�ĵ�һ���ֽ�. 

	//�������չ��, ��࿽��12���ֽ�
	// --> 12345678ABC(�ڲ�����) --> 12345678.ABC(�û�����)
	//����ж���û����չ��, ���ǿ���9���ֽ�, �ǲ������ַ�, �еĻ�˵��������չ��
	u8_t scan_len = ext_exist ? SFN_LEN + 1 : SFN_LEN; //���������չ������12, �����ھ���11=8+3

	//��dest_name���
	memset(dest_name, 0, X_FILEINFO_NAME_SIZE); //������32

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
				|| ((i > 8) && (diritem->DIR_NTRes & DIRITEM_NTRES_EXT_LOWER)) ) //�����չ����ҪСд, �����ļ�����ҪСд. ע��, �������û����չ���Ļ�, ����Ϊ��չ���Ͷ��ǿո�, ���ǿո�Ĵ�Сд��һ��
			{
				lower = 1;
			}

			*dest++ = lower ? tolower(*raw_name++) : toupper(*raw_name++);
		}
	}
	*dest++ = '\0'; //���һ���ַ���Ϊ��. 
	
}

//4.4 �����ļ���Ϣ
static void copy_file_info(xfileinfo_t* info, const diritem_t* dir_item) {
	//�洢���ֵ�ʱ��, �Ǹ�Ӧ�ÿ���ABC_____TXT(���Ǹ��û�����abc.txt)
	sfn_to_myname(info->file_name, dir_item);

	info->size = dir_item->DIR_FileSize;
	info->attr = dir_item->DIR_Attr;
	info->type = get_file_type(dir_item);

	copy_date_time(&info->create_time, &dir_item->DIR_CrtDate, &dir_item->DIR_CrtTime, dir_item->DIR_CrtTimeTeenth);
	copy_date_time(&info->last_acctime, &dir_item->DIR_LastAccDate,0,0);
	copy_date_time(&info->modify_time, &dir_item->DIR_WrtDate, &dir_item->DIR_WrtTime, 0);
}

//4.4 ��λ����ǰ�ļ����ڵĴ� //todo, Ŀ�ĵ�����ʲô? 
xfat_err_t xdir_first_file(xfile_t* file, xfileinfo_t* info)//����file�ṹ, ���д���ַȡֵ��info
{
	u32_t cluster_offset;
	u32_t moved_bytes = 0; //�ڴ������ƶ����˶����ֽ�
	xfat_err_t err;
	diritem_t* diritem = (diritem_t*)0;

	//�ж�file�Ƿ�ָ����Ŀ¼
	if (file->type != FAT_DIR) {
		return FS_ERR_PARAM;
	}

	//ָ����Ŀ¼�ͼ���������


	//����Ŀ¼�ĵ�һ��
	file->curr_cluster = file->start_cluster;
	file->pos = 0;

	cluster_offset = 0; //�ӵ�ǰ�ص��ʼ��ʼ��
	
	//4.5 �޸� //��ȡdir_item
	err = locate_file_dir_item(file->xfat, XFILE_LOCATE_NORMAL, &file->curr_cluster, &cluster_offset, "", &moved_bytes, &diritem);	//�ļ�·��: ����"", Ϊ��, Ҳ����˵, ���ǿ���ͨ��xfat.c�е�if(path == (const char*)0), Ҳ�����ҵ��������Ч��Ŀ¼��, �������ļ�����Ŀ¼������ //�ҵ�diritem��, �ڴ������ƶ���moved_bytes
	if(err < 0) return err;

	if (diritem == (diritem_t*)0)//˵��û�ж�ȡ�ɹ�: û���ҵ��κ���Ч���ļ�
	{
		return FS_ERR_EOF;	
	}

	file->pos += moved_bytes;

	copy_file_info(info, diritem); //��diritem����Ϣ����
	
	return err;
}

//4.4 ��ǰ�ļ�����һ����
xfat_err_t xdir_next_file(xfile_t* file, xfileinfo_t* info) //����file�ṹ, ���д���ַȡֵ��info
{
	u32_t cluster_offset;
	u32_t moved_bytes = 0; //�ڴ������ƶ����˶����ֽ�
	xfat_err_t err;
	diritem_t* diritem = (diritem_t*)0;

	//�ж�file�Ƿ�ָ����Ŀ¼
	if (file->type != FAT_DIR) {
		return FS_ERR_PARAM;
	}

	//ָ����Ŀ¼�ͼ���������


	//�ӵ�ǰ��λ�ü�������ȥ, ���Բ���Ҫ��λ���ļ��Ŀ�ͷ, ����comment��������
	//file->curr_cluster = file->start_cluster;
	//file->pos = 0;

	cluster_offset = to_cluster_offset(file->xfat, file->pos); //��posת���ɵ�ǰ�ص�ƫ��

	//4.5 �޸� //��ȡdir_item
	err = locate_file_dir_item(file->xfat, XFILE_LOCATE_NORMAL, &file->curr_cluster, &cluster_offset, "", &moved_bytes, &diritem);	//�ļ�·��: ����"", Ϊ��, Ҳ����˵, ���ǿ���ͨ��xfat.c�е�if(path == (const char*)0), Ҳ�����ҵ��������Ч��Ŀ¼��, �������ļ�����Ŀ¼������ //�ҵ�diritem��, �ڴ������ƶ���moved_bytes
	if (err < 0) return err;

	if (diritem == (diritem_t*)0)//˵��û�ж�ȡ�ɹ�: û���ҵ��κ���Ч���ļ�
	{
		return FS_ERR_EOF;
	}

	file->pos += moved_bytes;

	//��ԭ�е�cluster_offset���ƫ������, �ƶ�һ��Ŀ¼����ֽ���֮��, �ж��Ƿ񳬳��صı߽�, �������, ��������������һ����
	if (cluster_offset + sizeof(diritem_t) >= file->xfat->cluster_byte_size) {
		err = get_next_cluster(file->xfat, file->curr_cluster, &file->curr_cluster);
		if (err < 0) return err;
	}

	copy_file_info(info, diritem); //��diritem����Ϣ����

	return err;
}

//4.7 ��ȡ������
xfat_err_t xfile_error(xfile_t* file) 
{
	return file->err;
}

//4.7 ���������
void xfile_clear_err(xfile_t* file)
{
	file->err = FS_ERR_OK;
}

//4.8 
xfile_size_t xfile_read(void* buffer, xfile_size_t ele_size, xfile_size_t count, xfile_t* file)//��ȡԪ�صĴ�С(ele_size), ��ȡ���ٸ�������Ԫ��(count)
{

	xfile_size_t bytes_to_read = count * ele_size; //Ҫ�����ֽ���
	u8_t* read_buffer = (u8_t*)buffer; //����һ��ָ��, ָ��buffer
	xfile_size_t r_count_readed = 0; //ʵ�ʶ�ȡ���ֽ���, ע��, ���ܻ�С��Ҫ�����ֽ���bytes_to_read

	if (file->type != FAT_FILE) { //������ͨ�ļ��Ļ�, �ǲ��������
		file->err = FS_ERR_FSTYPE; //�ļ����ʹ���Ĵ�����
		return 0;
	}

	if (file->pos >= file->size) //��ǰ�Ķ�дλ��, �������ļ���ĩ��, pos�ǵ�ǰ�Ƕ��ٸ��ֽ�
	{
		file->err = FS_ERR_EOF; //˵���Ѿ�����������
		return 0;
	}

	if (file->pos + bytes_to_read > file->size) { //��ȡ������: �������ļ���С
		bytes_to_read = file->size - file->pos;	//����ȡ��������С. Ҳ���ǵ�ǰλ��pos������ֹλ��size֮��Ĳ��
	}

	//��֪����: posֻ�ǵ�ǰλ�õ��ֽ�, start_cluster�ǿ�ʼ�Ĵغ�.
	//������Ҫ֪��pos�����ǵڼ����صĵڼ��������ĵڼ���ƫ��
	xdisk_t* disk = file_get_disk(file); //��file���disk
	u32_t sector_in_cluster, sector_offset;
	
	//pos��Ӧ���Ǵ��������һ������
	sector_in_cluster = to_sector(disk, to_cluster_offset(file->xfat, file->pos)); //pos��ȫ�����ֽ�, ��ĳһ���������ƫ��:to_cluster_offset(file->xfat, file->pos), ����������ƫ��,��Ӧ���Ǵ��������һ������: to_sector(disk, xxx); //����sector_in_cluster��ֵ��0,1,2,3
	//pos��Ӧ���������������һ���ֽڵ�ƫ��
	sector_offset = to_sector_offset(disk, file->pos); //����sector_offset��ȡֵ��0-511

	//�Ƿ���Ҫ����,����, �жϵ�ǰ���Ƿ�����Ч��
	while ((bytes_to_read > 0)  &&  is_cluster_valid(file->curr_cluster)) {
		xfat_err_t err;
		xfile_size_t curr_read_bytes = 0; //������ʵ��u32_t. �����ܹ�����2^32 = 4g���ֽ� = 4GB
		u32_t sector_count = 0;
		u32_t start_sector = cluster_first_sector(file->xfat, file->curr_cluster) + sector_in_cluster; //�����ǰ��ȡ��λ��(����������) = ��ǰ�صĵ�һ��������(Ҳ�Ǿ���������) + ��ǰ��ȡ��λ�õ�����ڴ���ʼλ�õ�������

		//Ϊ�˷������:Լ���������
		/*
		1. ���1: ��һ����������, ������м�, �յ����м�
		2. ���2: ��һ����������, �������λ, �յ����м�
		3. ���3: ��һ����������, ������м�, �յ���ĩβ
		4. ���4: ռ��һ��������
		5. ���5: ռ�˶����������,������������һ��������
		6. ���6: ռ�˶����������,�����������ڶ���������Ĵ�����

		��֮: �����������������
		1 
		2
		3
		4
		2+(4/5/6)+3
		2+(4/5/6)
		(4/5/6)+3
		4/5/6
		*/

		if ((sector_offset != 0) || (!sector_offset && (bytes_to_read < disk->sector_size))) { //��Ҫ��Ӧ����: 1,3 || 2, 2+(4/5/6)+3, 2+(4/5/6)
			//����
			/*
			1. sector_offset != 0, ˵�������һ���������м��ĳ���ֽ�
			2. (!sector_offset && (bytes_to_read < disk->sector_size), ����!sector_offset�൱��sector_offset == 0, ���, bytes_to_read�����Ŵ�sector_offset��ʼ��ȡbytes_to_read���ֽ�. Ȼ������˵��������������ĵ�һ���ֽ�, �����յ���һ������֮��
			3/ ��֮�����������������: Ҫô��㲻����, Ҫô�յ㲻����
			4. ��������: ��㲻����(���ܴ��ڿ�����), �������յ㲻����(��֤�˲�������)
			5. ������㲻�����ҿ�����������, ������� if (sector_offset != 0 && (sector_offset + bytes_to_read > disk->sector_size))
			*/
			sector_count = 1; //��ȡһ������
			curr_read_bytes = bytes_to_read; //���˵: �������յ㲻����(��֤�˲�������) + ��㲻����(Ҳ��������), ��curr_read_bytes�ĺ������, ����Ҫ��ȡcurr_read_bytes���ֽ�

			if (sector_offset != 0 && (sector_offset + bytes_to_read > disk->sector_size)) { //��㲻�����ҿ�����������
				curr_read_bytes = disk->sector_size - sector_offset; //����ȥ���ľ��ǲ���������㵽���������յ��ⲿ��, ��������Ĳ���覴ö�
			}

			err = xdisk_read_sector(disk, temp_buffer, start_sector, 1);//��һ����������512�ֽڶ�����temp_bufferȫ�ֱ���
			if (err < 0)
			{
				file->err = err;
				return 0; 
			}

			//����512�ֽ���, ��ѡ��������Ҫ����һ����, �ŵ�read_buffer��. ����memcpy()��, �����temp_buffer + sector_offset, Ȼ������￪ʼ, ����curr_read_bytes���ֽ�
			memcpy(read_buffer, temp_buffer + sector_offset, curr_read_bytes);

			read_buffer += curr_read_bytes; //ע��: read_bufferû��������С����, ����ļӷ���ָ��ļӷ�

			bytes_to_read -= curr_read_bytes; //��Ϊ�Ѿ�����curr_read_bytes, ����Ҫ��ȥ
		}
		else {  //��Ӧ����: 4/5/6, (4/5/6)+3   //���ﴦ����������벢���յ�����������, ע��: ���������,����Ҳ���Էָ��: n��һ�������� + ����һ������ //��֮,���else����ľ��ǽ�һ����������read_buffer, ������һ����������һ���ָ�read_buffer
			sector_count = to_sector(disk, bytes_to_read); //��Ҫ�����ٸ�����. ע��, �����Ѿ���ȥ�˲���һ�������Ĳ���. ��Ϊ to_sector(disk, offset) ((offset) / (disk)->sector_size). ���Բ���һ�������Ĳ���, ����while����һ���ж�

			//���ǿ���һ���Զ�ȡ��������, ����Ҫ1.) �ж��Ƿ���, (��Ϊ��غ�, ��һ���غܿ��ܸ����ڵĴز���������), 2.) ������ܴ��ڽ�β������һ�������Ĳ���
			if ((sector_in_cluster + sector_count) > file->xfat->sec_per_cluster)//�����: ��ǰ��������(����ڴ���ʼ������) + Ҫ������������ ���� һ���ص�����
			{ 
				sector_count = file->xfat->sec_per_cluster - sector_in_cluster; //ֻ��ȡ��sector_in_cluster(ȡֵ0,1,2,3)�� file->xfat->sec_per_cluster(ֵ==4)������
			}

			//��ʼ��ȡ����: (��Ϊ���Ǳ�֤������������ͬһ���������)
			err = xdisk_read_sector(disk, read_buffer, start_sector, sector_count); //���ﱣ֤��: ��ȥ�Ķ���һ�����ڵ�������������(ע��, �Ͼ�else���� ������������벢���յ�����������)
			if (err != FS_ERR_OK) {
				file->err = err;
				return 0;
			}

			curr_read_bytes = sector_count * disk->sector_size;//��ǰ��ȡ���ֽ��� = ������ * һ���������ֽ�
			read_buffer += curr_read_bytes;//read_bufferָ��Ҫ���ӷ�
			bytes_to_read -= curr_read_bytes;
		}

		r_count_readed += curr_read_bytes; //����ʵ�ʶ�ȡ���ֽ���
		sector_offset += curr_read_bytes; //֮ǰ���ǵ�offset��û�ж�ȡ��ʱ�����������ƫ�� (sector_offsetָ����ǵ�һ��δ��ȡ���ֽ�), �����ȼ��������Ѿ���ȡ���ֽ���

		//��ȡ��֮��, ����Ҫ�ж�, ��һ����ȡ�Ĵغ���ʲô(�ǲ���Ҫ����, ������Ҫ����). ��һ����ȡ��������ʲô(�Ƿ���Ҫ��0,1,2,3�д��¹�Ϊ��0), ��һ��Ҫ��ȡ��������offset��ʲô(ע��, ����offsetֻ��2�����: 1.offsetû�г���һ��������С,�����ǵ�ǰ�������м��ֽ�, 2.offset�������һ�������ĵ�һ���ֽ�). ����������: offset��Ϊ��һ���������м��ֽ�)
		if (sector_offset >= disk->sector_size) { //����ֽ������������� (ע��: ϸ������, �����sector_offsetָ����ǵ�һ��δ��ȡ���ֽ�, ��������һ��������3���ֽ�, �����ֱ���0,1,2, �ڶ�ȡ֮ǰsector_offset = 1, ˵����2���ֽ�û�б���ȡ, ����curr_read_bytes = 2, ˵����ȡ�������ֽ�, Ҳ����index==1,2����ȡ��. ��ʱsector_offset += curr_read_bytes = 3, Ҳ����ָ������һ�������ĵ�һ���ֽ�, Ҳ����ָ���˵�һ��û�ж�ȡ���ֽ�.
			
			//����, ����ߵ�����, ˵�������Ǿ����������: 2,4,5,6. Ҳ��������sector_offsetҪô<sector_size, Ҫôsector_offset == sector_size, �����if( >= )Ӧ��ֻ�Ǳ���д��
			sector_offset = 0; //��Ȼ���µ������ĵ�һ���ֽ�, ����Ҫ���¹�Ϊ0
			sector_in_cluster += sector_count;	//����ҲҪ�������ڵ���һ�����еĵڼ�������, sector_in_cluster��ȡֵ��0,1,2,3

			if (sector_in_cluster >= file->xfat->sec_per_cluster) { //�ж������������Ѿ�Խ����(���� sector_in_cluster��ȡֵ�����4. ע��, ���ﲻ������5,6,7.., ��Ϊsector_count�����ֵֻ������4, ������line355��ȡ������ʱ��, �����Ǳ�֤��ȡ�Ķ������������ͬһ���������), Ҳ����˵Ӧ�ö���һ������
				
				sector_in_cluster = 0; //��Ȼ���´صĵ�һ������, ���Ǿ͹�Ϊ0
				err = get_next_cluster(file->xfat, file->curr_cluster, &file->curr_cluster); //֪�������е���һ���صĴغ��Ƕ���
				if (err != FS_ERR_OK) {
					file->err = err;
					return 0;
				}
			}
		}

		//����
		file->pos += curr_read_bytes;

		//֮��ص�while
	}

	//��while����֮��, ������֪������Ϊ����Ҫ�����ֽ�̫����, ����û����ô��ɶ�(Ҳ�������Ĵغ���Ч), ��������Ҫ�����ֽڶ�������.
	file->err = is_cluster_valid(file->curr_cluster) ? FS_ERR_OK : FS_ERR_EOF;

	return r_count_readed / ele_size; //���ն�ȡ����Ч�ֽ���/Ԫ�ص��ֽ��� = ʵ�ʶ�ȡ��Ԫ������
}

//4.9 �ж��ļ��ĵ�ǰ�Ķ�дλ���Ƿ����ļ�����
xfat_err_t xfile_eof(xfile_t* file) {
	return (file->pos >= file->size) ? FS_ERR_EOF : FS_ERR_OK;
}

//4.9 ��ȡ�ļ���ǰ��λ��
xfile_size_t xfile_tell(xfile_t* file) {
	return file->pos;
}

//4.9 �ļ���дλ�õĶ�λ
xfat_err_t xfile_seek(xfile_t* file, xfile_ssize_t offset, xfile_origin_t origin) {
	xfile_ssize_t final_pos; //Ҳ��������ʵ����ȥ��λ��
	u32_t curr_cluster, curr_pos; //��ǰ��λ��: ��, λ��
	xfat_err_t err;
	xfile_size_t offset_to_move; //

	switch (origin) {
	case XFAT_SEEK_SET:
		final_pos = offset; //Ҳ���Ǵӳ�ʼ + offset
		break;
	case XFAT_SEEK_CUR:
		final_pos = file->pos + offset; //Ҳ���Ǵӵ�ǰ��λ�� + offset
		break;
	case XFAT_SEEK_END:
		final_pos = file->size + offset; //Ҳ���Ǵ�ĩβ + offset (ע��, һ������offset��������,����Ǹ����Ļ�,���Ǻ���ᱨ��)
		break;
	default:
		final_pos = -1; //��Ϊ�û�û�д�����ȷ�Ĳ���
		break;
	}

	//�������final_pos���Ϸ�, ����
	if (final_pos < 0 || final_pos >= file->size) { //ע�����==file->sizeҲ���������
		return FS_ERR_PARAM;
	}

	offset = final_pos - file->pos; 
	if (offset > 0) //Ҳ���������ƶ�: ������˳��������
	{
		curr_cluster = file->curr_cluster;
		curr_pos = file->pos;

		//ƫ�Ƶľ���ֵ:(Ҳ���Ǵӵ�ǰλ�ÿ�ʼ, Ӧ���������߶��ٲ�)
		offset_to_move = (xfile_size_t)offset;
	}
	else { //�����ƶ�. ��Ϊ���ǵĴ����ǵ���(���ҵ�), ����Ϊ������, ���Ǹɴ���ʼ�Ĵ�, �����ʼ��λ�ÿ�ʼ
		curr_cluster = file->start_cluster;
		curr_pos = 0;

		//ƫ�Ƶľ���ֵ: �����Ǵӿ�ͷ��ʼ, ����ƫ��������offset, ����final_pos
		offset_to_move = (xfile_size_t)final_pos;
	}

	while (offset_to_move > 0 ) { //˵��������Ҫ�ƶ��Ĳ���, �����offset_to_move��������: ��Ҫ�ƶ����ֽ�. ����offset_to_move == 0��ʱ��,�Ѿ��ǲ���Ҫ�ƶ���ʱ��

		//���������ж��Ƿ��ƶ��ľ��볬��һ����

		//֮ǰ˵�����ִ���: �û���Ҫ�����ƶ�(�ӵ�0��λ�ÿ�ʼ+�����ƶ�) / �û���Ҫ�����ƶ�(�ӵ�ǰλ�ÿ�ʼ+�����ƶ�)
		u32_t cluster_offset = to_cluster_offset(file->xfat, curr_pos); //�����curr_pos������0,�����ǵ�ǰλ��.(���Ƕ��ǴӾ���λ��, ת��ɵ�ǰ����������λ��)
		xfile_size_t curr_move = offset_to_move;						//
		u32_t final_offset = cluster_offset + curr_move;
		
		if (final_offset < file->xfat->cluster_byte_size) //˵���ƶ���֮��, Ҳ��û�г���һ����. ����ֻ��offset��Ҫ����, ���ǴغŲ���Ҫ����
		{
			curr_pos += curr_move; //Ҳ����ֱ�ӻ�����յ�pos(����ƫ��)
			break;
		}
		else {//˵���ƶ���֮��, �ͻ���. (���������ȴ���û�п�ص��ǲ���), ���Ҹ�����һ���غ�
			curr_move = file->xfat->cluster_byte_size - cluster_offset; //��������Ҫ�ƶ��Ĳ���: Ҳ��������Ϊֹ����ǰ�ؽ�������һ��

			curr_pos += curr_move; //����һ�μ���curr_pos
			offset_to_move -= curr_move; //˵����������Ҫ�ƶ��Ĳ���,��Ҫ���ٵ�curr_pos

			//��ȡ�µĴغ�
			err = get_next_cluster(file->xfat, curr_cluster, &curr_cluster);
			if (err < 0) {
				file->err = err;
				return err;
			}
		}

		//�ߵ�����, ˵����Ҫ������һ�ֵ��ж�.
	}

	//�ߵ�����, ˵���Ѿ�֪���û���Ҫȥ��λ��������
	file->pos = curr_pos;
	file->curr_cluster = curr_cluster;
	return FS_ERR_OK;
}

//4.10 ��ǰ�Ĵغ�curr_cluster,���д���ƫ��curr_offset֪����֮��. �ƶ�move_btyes���ֽ�֮��, ����Ҫ������յĴغźʹ���ƫ��
xfat_err_t move_cluster_pos(xfat_t* xfat, u32_t curr_cluster, u32_t curr_offset, u32_t move_bytes, u32_t* next_cluster, u32_t* next_offset) {
	if ((curr_offset + move_bytes) >= xfat->cluster_byte_size) { //bug!4.10��bug, �Ҿ�Ȼд����curr_cluster + curr_offset, Ӧ����cur_offset + move_bytes. �ƶ���֮��, ����һ����. (ע������==Ҳ�㳬��, ˵��curr_clusterָ����ǵ�һ��δ��ȡ��Ԫ��)
		xfat_err_t err = get_next_cluster(xfat, curr_cluster, next_cluster);
		if (err < 0) return err;

		*next_offset = 0; //��ʦ˵, ��4.10Ϊֹ, �����������ֻ�Ƿ�����һ��һ��Ŀ¼����ƶ�,������� >= ������, �϶���offset = 0, Ҳ���ǴصĿ�ͷ
	}
	else {
		*next_cluster = curr_cluster;
		*next_offset = curr_offset + move_bytes; //ֻ��ƫ�Ƹ���
	}
	return FS_ERR_OK;
}

//4.10 �ҵ���һ��Ŀ¼�� (�����ҵ��Ľ�����typeƥ���,����һ����·����һ��)
//����type��������Ҫ��Ŀ¼�������. start_cluster, start_offsetָ���ǿ�ʼ�ҵĴغźʹ���ƫ��. found_cluster��found_offset���ҵ���Ŀ¼��Ĵغźʹ���ƫ��. next_cluster��next_offsetָ������һ��Ŀ¼���... . ���һ�������Ǵ���ַȡֵ(��ΪҪȡ��ֵ��һ����ַ,����������**, ��һ��*�����Ŵ���ַȡֵ, �ڶ���*����Ҫȡ������Ҳ��һ����ַ)
xfat_err_t get_next_diritem(xfat_t* xfat, u8_t type, u32_t start_cluster, u32_t start_offset, u32_t* found_cluster, u32_t* found_offset, u32_t* next_cluster, u32_t* next_offset, u8_t* temp_buffer, diritem_t** diritem) { 
	
	xfat_err_t err;
	diritem_t* r_diritem;

	//�жϵ�ǰ��start_cluster�Ƿ���Ч
	while (is_cluster_valid(start_cluster)) {
		u32_t sector_offset;

		//��Ϊ�����ڵ�һ��while��ʱ��,���ҵ���type,Ȼ��ͷ�����. ��������Ӧ���ڿ��ܷ���֮ǰ,�Ͱ�next_cluster��next_offset�����. ����Ҫ��Ҫ��curr_ָ������, ��ȷ�������ص�ʱ��, �Ż�����
		err = move_cluster_pos(xfat, start_cluster, start_offset, sizeof(diritem_t), next_cluster, next_offset);
		if (err < 0) 
			return err;

		//��Ϊ���Ƕ�ȡ�ĺ�����ֻ��������ȡһ��������. ����start_offset�Ǵ���ƫ��, �����������ڵ�ƫ��. ��������Ҫ���ڳ������ڵ�ƫ��
		sector_offset = to_sector_offset(xfat_get_disk(xfat), start_offset);

		//�������ƫ��==0, ˵�����ǾͿ���ֱ�Ӷ�ȡ��һ����������
		if (sector_offset == 0) {
			//��Ϊ��ȡ�ĺ���, ��Ҫ���������Ǿ���λ�õ�������. �������ǽ��غźʹ���ƫ��->����������
			u32_t curr_sector = to_phy_sector(xfat, start_cluster, start_offset);

			err = xdisk_read_sector(xfat_get_disk(xfat), temp_buffer, curr_sector, 1); //��������������ݶ���temp_buffer
			if (err < 0) 
				return err;
		} 

		//��Ϊtemp_buffer����һ��������, ����֮ǰ�õ���������ƫ����sector_offset, ��������r_diritemָ����������Ҫ��Ŀ¼��
		r_diritem = (diritem_t*)(temp_buffer + sector_offset);
		switch (r_diritem->DIR_Name[0]) {//����Ҫ�ж����Ŀ¼���е�DIR_Name�еĵ�һ���ֽ�
		case DIRITEM_NAME_END: //˵����ǰ���Ŀ¼���ǽ���
			if (type & DIRITEM_GET_END) { //�������Ҫ��Ŀ¼����ǽ���
				*diritem = r_diritem;
				*found_cluster = start_cluster;
				*found_offset = start_offset;
				return FS_ERR_OK;
			}
			break;
		case DIRITEM_NAME_FREE: //˵�����Ŀ¼���ǿ��е�, û�б��ļ�����Ŀ¼ʹ�õ�
			if (type & DIRITEM_GET_FREE) { 
				*diritem = r_diritem;
				*found_cluster = start_cluster;
				*found_offset = start_offset;
				return FS_ERR_OK;
			}
			break;
		default: //�������Ŀ¼���Ǳ�ʹ����, ���ļ�����Ŀ¼ʹ��
			if (type & DIRITEM_GET_USED) {
				*diritem = r_diritem;
				*found_cluster = start_cluster;
				*found_offset = start_offset;
				return FS_ERR_OK;
			}
			break;
		}

		//�ߵ�����˵��switch���涼��ifûͨ��:Ҳ���ǿ�����Ŀ¼���������Ҫ��
		//����Ϊ����һ��while,����Ҫ��start_����.
		start_cluster = *next_cluster;
		start_offset = *next_offset;


	}

	//˵��û���ҵ�
	*diritem = (diritem_t*)0;
	return FS_ERR_EOF; //˵���Ѿ��������˽�β
}

//4.10 ���ô�Сд
static u8_t get_sfn_case_cfg(const char* new_name) { 
	u8_t case_cfg; //����ԭ�е��ļ����Ĵ�Сд����
	int name_len; //�ļ�������
	const char* ext_dot; //�ָ���
	const char* p;	//����ָ��
	const char* source_name = new_name;

	int ext_existed;//��չ���Ƿ����

	//��dest_nameȫ�����, ��Ҫ���0, ������ո�
	//memset(source_name, ' ', SFN_LEN); ��仰ɾ��,��Ϊsource_name�Ѿ�����new_name

	//������б��, ��Ϊ��һ������ʱ��, ���ܻ��� aa/bb/cc/123.txt
	while (is_path_sep(*source_name)) {
		source_name++;
	}
	//����while֮��, source_nameָ����Ǹ��ַ�, ���ǷǷ�б��

	//�жϵ�ָ���������, ���ļ�������չ���ָ��
	ext_dot = source_name; //��ʼ��λmy_name�Ŀ�ͷ, ע��source_name��ָ��
	p = source_name;  //p���ڱ�����������
	name_len = 0;

	//��ʼ����, ��ʦ˵, ��һ����·��, �������while����Ƚ�, ���������ָ���������
	//��֮: ���while���ǽ�/xxx/�м��Ŀ¼��xxx, ����/xxx.txt���ļ���ȡ����, ���ǲ�����/��
	while ((*p != '\0') && !is_path_sep(*p))
	{
		if (*p == '.') {
			ext_dot = p;// ����չ���ָ���ָ��p
		}
		p++;
		name_len++; //���ȵļ���++
	}

	ext_existed = (ext_dot > source_name) && (ext_dot < (source_name + name_len - 1)); //��������, �����ļ����Ŀ�ͷ, С���ļ����Ľ���

	//����source_name, ���Ҳ���
	for (p = source_name; p < source_name + name_len; p++) {
		if (ext_existed) { //�����չ������
			if (p < ext_dot) {
				case_cfg |= islower(*p) ? DIRITEM_NTRES_BODY_LOWER : 0;//�����ļ����Ĵ�Сд����
			}
			else if (p > ext_dot) {
				case_cfg |= islower(*p) ? DIRITEM_NTRES_EXT_LOWER : 0;//������չ���Ĵ�Сд����
			}
		}
		else { //������
			   //�����ļ����Ĵ�Сд����
			case_cfg |= islower(*p) ? DIRITEM_NTRES_BODY_LOWER : 0; //����ȫ����p,�����һ��p��Сд,Ҳ�����ļ�����һ���ַ���Сд, �Ͱ�ȫ�����Сд
		}
	}

	return case_cfg;
}
//4.10	�޸��ļ���
xfat_err_t xfile_rename(xfat_t* xfat, const char* path, const char* new_name) { //����Ҫ�ҵ�·��(�ļ�)�Ǿ���·��,�Ӹ�Ŀ¼��ʼ
	//�ҵ�ƥ��type��Ŀ¼��, ���ǻ�����֤·����������Ҫ��
	diritem_t* diritem = (diritem_t*)0;
	u32_t curr_cluster, curr_offset; //��ǰ���ҵ�λ��
	u32_t next_cluster, next_offset; //��һ�ֲ��ҵ�λ��
	u32_t found_cluster, found_offset; //�ҵ�ʱ��λ��

	const char* curr_path; //��ǰ���ҵ�·��. ���е���־���Ϊʲô��const, ֮�󲻶���������

	curr_cluster = xfat->root_cluster; //��Ϊ����Ĭ��path�Ǿ���·��, ���ԴӸ�Ŀ¼�Ĵغſ�ʼ (todo: ����Ժ�֧�����·��, ���������һ����)
	curr_offset = 0;

	//���������·���� /a/b/c/d, �������ǵ�curr_path�Ὣ����·���зֳ�һ��һ��: /a, /b, /c
	for (curr_path = path; curr_path != '\0'; curr_path = get_child_path(curr_path)) //��ͣ������·��, ֱ��ȫ��������. Ӧ�ò�����ǰbreak
	{
		do {
			//��curr_path�в��ҷ���type��Ŀ¼��. type��: �Ѿ����ڵ�Ŀ¼��(��Ϊ���ǵ�Ŀ����������,������Ҫ�Ѵ��ڵ�)
			xfat_err_t err = get_next_diritem(xfat, DIRITEM_GET_USED, curr_cluster, curr_offset, &found_cluster, &found_offset, &next_cluster, &next_offset, temp_buffer, &diritem);
			if (err < 0) return err;//ע��, ����֮����Ҫ�ṩfound_��next_��������Ϊ: found_���������ҵ��˷���type��,����������������ǵ�·��Ҫ���ļ�, ������Ҫͨ��next_����Ѱ����һ��Ŀ¼��diritem
			
			//�����ߵ�����, Ҳ�п��ܴ���dirtiem�ǿյ����, ��get_next_diritem()��ĩβ����
			if (diritem == (diritem_t*)0) {
				return FS_ERR_NONE; //˵��������Ҫ�ҵ��ļ�
			}

			if (is_filename_match((char*)diritem->DIR_Name, curr_path)) { //Ҳ���൱��dir_name�����ǵ�/a, ����/b, /cȥ�Ƚ�, ���ҷ���ƥ����
				if (get_file_type(diritem) == FAT_DIR) { //����������Ǹ�Ŀ¼, ˵��Ϊ���ҵ����ǵ�Ŀ���ļ�, ������Ҫ��������Ŀ¼. �����պ����ļ�, bingo�ҵ���
					curr_cluster = get_diritem_cluster(diritem); //Ϊʲô��������� &next_cluster, &next_offset? ��Ϊnext_cluster������diritem������path��ȥ��һ��λ����Ŀ¼���õ�. ��������, diritem����������Ҫ��, ��������curr_��Ҫ����diritem��get_diritem_cluster(), �����������˼��: �õ�diritem����Ӧ���ļ�/Ŀ¼���ڵĴغ�. ������: ��ȡ�ļ��Ĵغ�, ��16λ�͵�16λ���
					curr_offset = 0; //���ǴӸ��ļ�/Ŀ¼�ĴصĿ�ͷ��ʼ��
				}

				break; 
				//����while: 
				//1. ���֮ǰ��fat_dir, ��������curr_path������Ŀ¼/�ļ���, �Ǿͼ��� (һ����˵, curr_path���ǻ�����Ŀ¼/�����ļ�����,���û�еĻ�,�����û���������)
				//2. ���֮ǰ���ļ�,bingo
			}
			
			//˵���������type��Ŀ¼��,����������Ҫ��,�Ǿ�ȥ����һ������type��Ŀ¼��.
			curr_cluster = next_cluster;
			curr_offset = next_offset;

		} while (1);
	}

	//����û�м��: �ļ������µ�Ŀ¼�����Ƿ����(��ʦʡ��)

	if (diritem && !curr_path) { //˵��diritem != (diritem_t*)0, ����curr_path�Ѿ��ߵ������, ˵�������ҵ���diritem��������Ҫ���ļ�
		//����Ŀ¼�������(����˵��������)
		u32_t dir_sector = to_phy_sector(xfat, found_cluster, found_offset);//�ҵ�diritem������disk�еľ���������. ����֮������found_cluster����Ϊ, next_��ʹ�õ������(diritem������path), get_diritem_cluster(diritem)��ʹ�õ������(diritem��������Ŀ¼, ����Ҫȡȥ���Ŀ¼���ڵĴ�)
		to_sfn((char*)diritem->DIR_Name, new_name, 0); //������: set file name. ע�����һ������0����Ϲд��,�������û���õ�

		//�����ļ���,��չ���Ĵ�Сд
		diritem->DIR_NTRes &= ~DIRITEM_NTRES_CASE_MASK;//�û�����
		diritem->DIR_NTRes |= get_sfn_case_cfg(new_name); //����, �������: ͨ��new_name�Ĵ�Сд����

		return xdisk_write_sector(xfat_get_disk(xfat), temp_buffer, dir_sector, 1); //��diritem������, ��д��temp_buffer��.
	}
	
	return FS_ERR_OK;

}

