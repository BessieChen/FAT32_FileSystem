#include "xfat.h"
#include "xdisk.h" //��Ϊfat������disk֮��

//3.2 ʹ��ȫ�ֱ���u8_t temp_buffer[512], ��Ϊ�����temp_buffer��xdisk.c��, ��������Ҫ���ļ�ʹ�õĻ�, Ҫô��#include "xdisk.h", Ҫô��extern, �����������ᱨ��,��Ϊ����������ȫ�ֱ���������temp_buffer
extern u8_t temp_buffer[512];


//3.2 ��ȡfat32�����ĵڶ�������: fat��, �ѹؼ���Ϣ������xfat��
static xfat_err_t parse_fat_header(xfat_t* xfat, dbr_t* dbr) //���ﴫ���˱�����dbr����Ϣ, ��Ϊdbr��ʵ�ǰ�����fat��������������Ϣ��. ע��, xfat��ֻ��xfat->disk_part��ӵ�����ݵ�, ���඼�Ǵ���ַȡֵ
{
	xdisk_part_t* xdisk_part = xfat->disk_part; //���fat���������ĸ�fat32����
	
	//��xfat�洢�ؼ���Ϣ:
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

	return FS_ERR_OK;
}


//static xfat_err_t parse_fat_header(xfat_t * xfat, dbr_t * dbr) {
//	xdisk_part_t * xdisk_part = xfat->disk_part;
//
//	// ����DBR���������������õĲ���
//	xfat->fat_tbl_sectors = dbr->fat32.BPB_FATSz32;
//
//	// �����ֹFAT����ֻˢ��һ��FAT��
//	// disk_part->start_blockΪ�÷����ľ������������ţ����Բ���Ҫ�ټ���Hidden_sector
//	if (dbr->fat32.BPB_ExtFlags & (1 << 7)) {
//		u32_t table = dbr->fat32.BPB_ExtFlags & 0xF;
//		xfat->fat_start_sector = dbr->bpb.BPB_RsvdSecCnt + xdisk_part->start_sector + table * xfat->fat_tbl_sectors;
//		xfat->fat_tbl_nr = 1;
//	}
//	else {
//		xfat->fat_start_sector = dbr->bpb.BPB_RsvdSecCnt + xdisk_part->start_sector;
//		xfat->fat_tbl_nr = dbr->bpb.BPB_NumFATs;
//	}
//
//	xfat->total_sectors = dbr->bpb.BPB_TotSec32;
//
//	return FS_ERR_OK;
//}
//
///**
//* ��ʼ��FAT��
//* @param xfat xfat�ṹ
//* @param disk_part �����ṹ
//* @return
//*/
//xfat_err_t xfat_open(xfat_t * xfat, xdisk_part_t * xdisk_part) {
//	dbr_t * dbr = (dbr_t *)temp_buffer;
//	xdisk_t * xdisk = xdisk_part->disk;
//	xfat_err_t err;
//
//	xfat->disk_part = xdisk_part;
//
//	// ��ȡdbr������
//	err = xdisk_read_sector(xdisk, (u8_t *)dbr, xdisk_part->start_sector, 1);
//	if (err < 0) {
//		return err;
//	}
//
//	// ����dbr�����е�fat�����Ϣ
//	err = parse_fat_header(xfat, dbr);
//	if (err < 0) {
//		return err;
//	}
//
//	return FS_ERR_OK;
//}