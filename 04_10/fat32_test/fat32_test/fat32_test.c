#include<stdio.h>
#include "xdisk.h"
#include "xfat.h" //������xtypes.h��
#include <string.h> //������memset(), memcmp()
#include <stdlib.h> //3.3 ������һЩmalloc()

//1.2 ����: ���������vdisk��ʵ����extern���ù���,
extern xdisk_driver_t vdisk_driver;

//2.2 ����һ�����Դ��̵�·��
const char* disk_path = "disk.img";

//2.2 ����һ��disk
xdisk_t disk;
//3.1 ����һ��disk part���������Ϣ
xdisk_part_t  disk_part;
//3.1 ����buffer
static u32_t read_buffer[160 * 1024]; //���Դ�160*1024���ֽڵĶ���, �൱��160MB
//3.2 ����xfat, ����洢��fat��Ĺؼ���Ϣ
xfat_t xfat;

//2.4 �޸�: ��ȡÿ����������Ϣ //2.2 ������ĺ���
int disk_part_test(void) {
	u32_t count, i;
	xfat_err_t err;

	printf("partition read test...\n");

	//���ü�����������ĺ���
	err = xdisk_get_part_count(&disk, &count);
	if (err < 0) {
		printf("partition count detect failed.\n");
		return err;
	}

	//2.4 �����еķ�����Ϣ���ó���
	for (i = 0; i < count; i++) {
		xdisk_part_t part;

		int err;

		//��ʼ��ÿһ������(����С��Ƭ)����Ϣ�浽part��
		err = xdisk_get_part(&disk, &part, i);//˵������Ҫ��i������
		if (err < 0) {
			printf("read partition failed.\n");
			return -1;
		}

		printf("no: %d, start sector: %d, total sector: %d, capcity:%.0f M\n",
			i, part.start_sector, part.total_sector, part.total_sector * disk.sector_size / 1024 / 1024.0);

	}

	printf("partition count:%d\n", count);
	return 0;
}

//3.3 ��ӡĿ¼�����ϸ��Ϣ
void show_dir_info(diritem_t* diritem) {
	
	char file_name[12];//�����ļ����Ļ���
	memset(file_name, 0, sizeof(file_name)); //��file_name�����ó�0
	memcpy(file_name, diritem->DIR_Name, 11); //��Ŀ¼��diritem�п����ļ���, �ļ�����11�ֽ�
	
	//�ж�,�ļ����ĵĵ�0���ֽ�
	if (file_name[0] == 0x05) {
		file_name[0] = 0xE5; //�ĵ���˵��, �����05, ��Ҫ�ĳ�E5
	}

	//��ӡ�ļ���
	printf("\n name: %s", file_name);
	printf("\n\t");

	//��ӡ����
	u8_t attr = diritem->DIR_Attr;
	if (attr & DIRITEM_ATTR_READ_ONLY)//�����ֻ����
	{
		printf("read only, ");
	}

	if (attr & DIRITEM_ATTR_HIDDEN) //��������ص�
	{
		printf("hidden, ");
	}

	if (attr & DIRITEM_ATTR_SYSTEM) //�����ϵͳ�ļ�
	{
		printf("system, ");
	}

	if (attr & DIRITEM_ATTR_DIRECTORY) //�����Ŀ¼
	{
		printf("direcotry, ");
	}

	if (attr & DIRITEM_ATTR_ARCHIVE) //����ǹ鵵
	{
		printf("archive.");
	}
	printf("\n\t");

	//��ӡ: ��������, Crt:create
	printf("create date: %d-%d-%d\n\t", diritem->DIR_CrtDate.year_from_1980 + 1980,
		diritem->DIR_CrtDate.month, diritem->DIR_CrtDate.day);
	//��ӡ: ����ʱ��
	printf("create time: %d-%d-%d\n\t", diritem->DIR_CrtTime.hour, diritem->DIR_CrtTime.minute,
		diritem->DIR_CrtTime.second_2 * 2 + diritem->DIR_CrtTimeTeenth / 100); //�������Ǻ������˼:DIR_CrtTimeTeenth, Ȼ�����ǵ�����ż����

	//��ӡ: ����޸�����, Wrt:write
	printf("last write date: %d-%d-%d\n\t", diritem->DIR_WrtDate.year_from_1980 + 1980,
		diritem->DIR_WrtDate.month, diritem->DIR_WrtDate.day);
	//��ӡ: ����޸�ʱ��
	printf("last write time: %d-%d-%d\n\t", diritem->DIR_WrtTime.hour, diritem->DIR_WrtTime.minute,
		diritem->DIR_WrtTime.second_2 * 2);


	//��ӡ: ����������, Wrt:write
	printf("last access date: %d-%d-%d\n\t", diritem->DIR_LastAccDate.year_from_1980 + 1980,
		diritem->DIR_LastAccDate.month, diritem->DIR_LastAccDate.day);

	//��ӡ: �ļ���С
	printf("size %d KB\n\t", diritem->DIR_FileSize / 1024);

	//���Ŀ¼��, �������������ʼλ�õĴغ�
	printf("cluster %d\n\t", diritem->DIR_FstClusHI << 16 | diritem->DIR_FstClusL0); //����16λ�͸�16λ���

	printf("\n");
}

//3.3 ���fatĿ¼�Ĳ���
int fat_dir_test(void) {
	u32_t j;
	xfat_err_t err;
	diritem_t* dir_item; //Ŀ¼��. ���ں��潫cluster_bufferǿ��ת��������ṹ��

	//����һ������, ��С��һ���صĴ�С(һ����ӵ�ж����ֽ�, ��512��ż����, ����512, 1024, 2048..) //ע������û����ȫ�ֱ���read_buffer
	u8_t* cluster_buffer;
	cluster_buffer = (u8_t*)malloc(xfat.cluster_byte_size); //�����xfat�Ǳ�.c�ļ���ȫ�ֱ���

	//����һ����ǰ�Ĵ�
	u32_t curr_cluster;
	curr_cluster = xfat.root_cluster; //��ʼֵ: �������ĸ�Ŀ¼�ĵ�һ���� //�����xfat�Ǳ�.c�ļ���ȫ�ֱ���

	//�ڼ���Ŀ¼��
	int index = 0;
	
	//�����ǰ�Ĵغ�����Ч��, ��ô�ö�ȡ����غ�, �����������Ϣ
	while (is_cluster_valid(curr_cluster)) {
		//��ȡ�����
		err = read_cluster(&xfat, cluster_buffer, curr_cluster, 1); //�����xfat�Ǳ�.c�ļ���ȫ�ֱ���
		if (err) {
			printf("read cluster %d failed.\n", curr_cluster);
			return -1;
		}

		//�鿴�����������: Ŀ¼��(�ṹ����diritem_t)
		//Ŀ¼��ĸ���: xfat.cluster_byte_size / sizeof(diritem_t), Ҳ����һ���ص��ֽ��� / Ŀ¼����ֽ��� == Ŀ¼�����
		dir_item = (diritem_t*)cluster_buffer;
		for (j = 0; j < xfat.cluster_byte_size / sizeof(diritem_t); j++) {
			//��ȡ�ļ���
			u8_t* name = (u8_t*)(dir_item[j].DIR_Name);//��ϲ�����д��, Ҳ���ǽ�ָ�뵱����������dir_item[j]

													   //�ļ����ĵ�һ���ַ�(��һ���ַ�DIR_Name,��ʾ: ��Ч, ����, ���߽���), �����0xE5 ���ǿ�. �����0x00,���ǽ���
			if (name[0] == DIRITEM_NAME_FREE) //������0xE5
			{
				continue;//���еĻ�, ����һ��Ŀ¼��
			}
			else if (name[0] == DIRITEM_NAME_END) //������0x00
			{
				break; //�����Ļ�, ����
			}

			//�ߵ�����: ˵������Ч
			printf("No: %d, ", ++index);//�ڼ���Ŀ¼��

										//��ӡĿ¼�����ϸ��Ϣ
			show_dir_info(&dir_item[j]);
		}

		//3.4 ��ȡ��һ���غ�
		err = get_next_cluster(&xfat, curr_cluster, &curr_cluster); //�ڶ�������, ��ǰ�غ�, ����������: ����һ���غŸ��������ַ
		if (err)
		{
			printf("get next cluster failed! current cluster No: %d\n", curr_cluster);
			return -1;
		}
	}
	return 0;
}

//3.5 ��ӡ�ļ���Ϣ
int fat_file_test(void) {
	xfat_err_t err;

	//����һ������, ��С��һ���صĴ�С(һ����ӵ�ж����ֽ�, ��512��ż����, ����512, 1024, 2048..) //ע������û����ȫ�ֱ���read_buffer
	u8_t* cluster_buffer;
	cluster_buffer = (u8_t*)malloc(xfat.cluster_byte_size + 1); //3.5 �޸�, ��Ϊ�������Ǹ�buffer������һ���Լ��Ľ����� //�����xfat�Ǳ�.c�ļ���ȫ�ֱ���

	//����һ����ǰ�Ĵ�
	u32_t curr_cluster;
	curr_cluster = 4565; //3.5 ��ʦ����ѡȡ����64.txt����ļ�, ����ļ���Ŀ¼������ʾ����ʼ�غž�ʱ4565 //curr_cluster = xfat.root_cluster; //��ʼֵ: �������ĸ�Ŀ¼�ĵ�һ���� //�����xfat�Ǳ�.c�ļ���ȫ�ֱ���

	//3.5 ����ռ�õĿռ�(��������Ǵ�ռ�е��ֽ����ı���)
	int size = 0;

	//�����ǰ�Ĵغ�����Ч��, ��ô�ö�ȡ����غ�, �����������Ϣ
	while (is_cluster_valid(curr_cluster)) {
		//��ȡ�����
		err = read_cluster(&xfat, cluster_buffer, curr_cluster, 1); //�����xfat�Ǳ�.c�ļ���ȫ�ֱ���
		if (err) {
			printf("read cluster %d failed.\n", curr_cluster);
			return -1;
		}

		//3.5 ��ӡbuffer�������Ϣ
		cluster_buffer[xfat.cluster_byte_size + 1] = '\0'; //�������Ͻ�����
		printf("%s", (char*)cluster_buffer);

		//3.5 ��������ļ�ռ�е��ֽ�. ��Ϊ�����ļ����ɢ����ͬ�Ĵ���, ����size�ǴصĴ�С�ı���
		size += xfat.cluster_byte_size;

		//3.4 ��ȡ��һ���غ�
		err = get_next_cluster(&xfat, curr_cluster, &curr_cluster); //�ڶ�������, ��ǰ�غ�, ����������: ����һ���غŸ��������ַ
		if (err)
		{
			printf("get next cluster failed! current cluster No: %d\n", curr_cluster);
			return -1;
		}
	}

	//3.5 ��ӡsize
	printf("\nfile size: %d", size);

	return 0;
}

//4.1 
int fs_open_test(void) {
	//4.2 ���岻���ڵ��ļ�
	const char* not_exist_path = "/file_not_exist.txt";
	//4.2 ������ڵ��ļ�
	const char* exist_path = "/12345678ABC"; //��Ӧ��ʵ�ʵ��ļ���: 12345678.abc. ����/�Ǹ�Ŀ¼
	//4.3 ����·����
	const char* file1 = "/open/file.txt"; //���ڵ�
	const char* file2 = "/open/a0/a1/a2/a3/a4/a5/a6/a7/a8/a9/a10/a11/a12/a13/a14/a15/a16/a17/a18/a19/file.txt"; //���ڵ�

	xfat_err_t err;
	xfile_t file;

	printf("fs open test...\n");

	//�򿪸�Ŀ¼:
	err = xfile_open(&xfat, &file, "/"); //"/�Ǹ�Ŀ¼����˼, �����linux"
	if (err) {
		printf("open file failed./ \n");
		return -1;
	}
	xfile_close(&file);

	//4.2 �򿪲����ڵ��ļ�
	err = xfile_open(&xfat, &file, not_exist_path);
	if (err == 0) { //Ϊ�˵������, �������ǻ��ô����ͨ��, ������� err != 0
		printf("open file succeed./ \n");
		return -1;
	}
	xfile_close(&file);
	
	//4.2 �򿪴��ڵ��ļ�
	err = xfile_open(&xfat, &file, exist_path);
	if (err) {
		printf("open file failed./ \n");
		return -1;
	}
	xfile_close(&file);

	//4.3 ���ļ�file1
	err = xfile_open(&xfat, &file, file1);
	if (err) {
		printf("open file failed./ \n");
		return -1;
	}
	xfile_close(&file);

	//4.3 ���ļ�file2
	err = xfile_open(&xfat, &file, file2);
	if (err) {
		printf("open file failed./ \n");
		return -1;
	}
	xfile_close(&file);


	printf("file open test ok.\n");
	return 0;
}

//4.4 ��ӡ�ļ���Ϣ
void show_file_info(xfileinfo_t* info) {
	//����
	printf("\n\n name: %s, ", info->file_name); 
	
	//����
	switch (info->type) {
	case FAT_FILE:
		printf("file, ");
		break;
	case FAT_DIR:
		printf("dir, ");
		break;
	case FAT_VOL:
		printf("vol, ");
		break;
	default:
		printf("Unknown,");
		break;
	}

	//ʱ��
	printf("\n\tcreate date: %d-%d-%d, ", info->create_time.year, info->create_time.month, info->create_time.day);
	printf("\n\tcreate time: %d-%d-%d, ", info->create_time.hour, info->create_time.minute, info->create_time.second);

	printf("\n\twrite date: %d-%d-%d, ", info->modify_time.year, info->modify_time.month, info->modify_time.day);
	printf("\n\twrite time: %d-%d-%d, ", info->modify_time.hour, info->modify_time.minute, info->modify_time.second);

	printf("\n\tlast access date: %d-%d-%d, ", info->last_acctime.year, info->last_acctime.month, info->last_acctime.day);

	//�ļ���С
	printf("\n\tsize %d kB, ", info->size / 1024);

	printf("\n");

}

//4.6 ��ӡfile����������ļ���Ŀ¼
int list_sub_file(xfile_t* file, int curr_depth) {
	int i;
	int err = 0;
	xfileinfo_t fileinfo;

	//����ǰĿ¼�����ļ�����
	err = xdir_first_file(file, &fileinfo);
	if (err) return err;

	//������Ŀ¼�����
	do {
		xfile_t sub_file;

		if (fileinfo.type == FAT_DIR) { //�����Ҳ�Ǹ�Ŀ¼, �Ǿͼ����о���Ŀ¼(�ݹ�)
			//��ӡ���ṹ:
			for (i = 0; i < curr_depth; i++) {
				printf("-");
			}

			printf("%s\n", fileinfo.file_name);
			
			//�򿪵�ǰĿ¼�����������Ŀ¼
			err = xfile_open_sub(file, fileinfo.file_name, &sub_file);
			if (err < 0) return err;

			//���������Ϣ�г���
			err = list_sub_file(&sub_file, curr_depth + 1); //�ݹ�
			if (err < 0) return err;

			//�ر�
			xfile_close(&sub_file);
		}
		else { //��ͨ�ļ�
			//��ӡ���ṹ: (ע��, �����ӡ���ļ�ǰ��û��-, ˵������ļ��ڸ�Ŀ¼����.)
			for (i = 0; i < curr_depth; i++) {
				printf("-");
			}

			printf("%s\n", fileinfo.file_name);
		}
	} while ((err = xdir_next_file(file, &fileinfo)) == 0);

	return err;
}


//4.4 Ŀ¼�ı�������
int dir_trans_test(void) {
	xfat_err_t err;
	xfile_t top_dir;
	xfileinfo_t fileinfo;

	printf("\ntrans dir test begin.\n");

	//err = xfile_open(&xfat, &top_dir, "/");//������Ŀ¼�µ��ļ�����Ŀ¼
	//err = xfile_open(&xfat, &top_dir, "/read");//����readĿ¼�µ��ļ�����Ŀ¼
	err = xfile_open(&xfat, &top_dir, "/read/..");//����read���ϲ�: ��Ŀ¼���ļ�����Ŀ¼
	if (err < 0)
	{
		printf("Open Directory Failed.\n");
		return -1;
	}

	//��ȡ��һ���ļ�
	err = xdir_first_file(&top_dir, &fileinfo);
	if (err < 0) {
		printf("get first file info failed.\n");
		return -1;
	}

	//��ʾ�ļ���Ϣ
	show_file_info(&fileinfo);

	//������ȡ�����Ŀ¼�µ������ļ�
	while ((err = xdir_next_file(&top_dir, &fileinfo)) == 0) {
		show_file_info(&fileinfo);
	}
	if (err < 0) {
		printf("get next file info failed.\n");
		return -1;
	}

	//4.6 ��ӡtop_dir����������ļ���Ŀ¼. 
	printf("\n try to list all sub files.\n");
	err = list_sub_file(&top_dir, 0); //���Ǹ��ݹ麯��, �����0
	if (err < 0) {
		printf("list sub file failed.\n");
		return -1;
	}


	err = xfile_close(&top_dir);
	if (err < 0)
	{
		printf("close file failed. \n");
		return -1;
	}

	printf("file trans test ok.\n");
	return 0;
}

//4.8 ��ȡ�ļ�
int file_read_and_check(const char* path, xfile_size_t ele_size, xfile_size_t count)
{
	xfile_t file;
	xfile_size_t read_count;

	//���ļ�
	xfat_err_t err = xfile_open(&xfat, &file, path);
	if (err != FS_ERR_OK) {
		printf("open file failed! %s \n", path);
		return -1;
	}

	//��ȡ�ļ�
	if ((read_count = xfile_read(read_buffer, ele_size, count, &file) > 0)) {
		u32_t i = 0;
		xfile_size_t bytes_count = read_count * ele_size; //ʵ�ʶ�ȡ��Ԫ�ظ��� * Ԫ�ص��ֽ���
		u32_t num_start = 0;//Ҫ�Ƚϵ�����

		//��ʼ�Ƚ����Ƕ�ȡ��ֵ�ǲ�����ֵ(��ֵ��ÿ4���ֽ�, ����һ������, ����ǰ4���ֽ���00 00 00 00, ��4���ֽ���01 00 00 00 ����������1)
		for(i = 0; i < bytes_count; i += 4) //32λ�����ͱȽ�, ��Ϊ������i��������ֽ���, ������+=4
		{
			int int_index = i / 4; //���ǵڼ������ֽڵ���˼
			if (read_buffer[int_index] !=  num_start++) //����ÿ����4���ֽ�, ��Ҫ��һ
			{
				printf("number doesn't match.\n");
				return -1;
			}
		}
	}

	//��������if()��read_count == 0˵���д�����
	if (xfile_error(&file) < 0) {
		printf("read file failed.\n");
		return -1;
	}

	//�ر��ļ�
	xfile_close(&file);

	return 0;

}

//4.8 ��ȡ�ļ��Ĳ���
int fs_read_test(void) {
	//disk����һ��readĿ¼, ���Ŀ¼����һ��0�ֽڵ��ļ�:
	const char* file_0b_path = "/read/0b.bin";
	const char* file_1MB_path = "/read/1MB.bin";

	xfat_err_t err;

	printf("\n file read test begins:\n");

	//��ȡ�����������
	memset(read_buffer, 0, sizeof(read_buffer));

	err = file_read_and_check(file_0b_path, 32, 1); //������path, ��ȡ��Ԫ�ش�С:32�ֽ�(������һ������), ��һ��Ԫ��
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	//һ��������
	err = file_read_and_check(file_1MB_path,disk.sector_size - 32, 1); //������path, ��ȡ��Ԫ�ش�С:(��32�ֽھ�һ������: Ҳ����512-32=480�ֽ�)(Ҳ���ǲ�����һ������), ��һ��Ԫ��
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	//һ����������
	err = file_read_and_check(file_1MB_path, disk.sector_size, 1); //������path, ��ȡ��Ԫ�ش�С:һ������(���ǲ�����һ����). ��һ��Ԫ��
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	//һ��������(���ǲ����)
	err = file_read_and_check(file_1MB_path, disk.sector_size + 14, 1); //������path, ��ȡ��Ԫ�ش�С:һ��������(���ǲ�����һ����, ��һ��Ԫ��
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}


	//һ�����
	err = file_read_and_check(file_1MB_path, xfat.cluster_byte_size + 32, 1);
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	//�����
	err = file_read_and_check(file_1MB_path, 2 * xfat.cluster_byte_size + 32, 1);
	if (err < 0) {
		printf("read failed.\n");
		return -1;
	}

	return 0;
}

//4.9 ��λ
int fs_seek(xfile_t* file, xfile_origin_t origin, xfile_ssize_t offset) {
	
	int err;
	xfile_ssize_t expected_pos;//�����������ֵ. 
	int count; //���ں������, ʵ�ʶ�ȡ�˼����ֽ�

	switch (origin) {
	case XFAT_SEEK_SET:
		expected_pos = offset;
		break;

	case XFAT_SEEK_CUR:
		expected_pos = file->pos + offset;
		break;

	case XFAT_SEEK_END:
		expected_pos = file->size + offset;
		break;

	default:
		expected_pos = 0;
		break;
	}

	//����seek����ȥ��λ
	err = xfile_seek(file, offset, origin);
	if (err) {
		printf("seek error01!\n");
		return err;
	}

	//���: file->pos�Ƿ�����ֵ. ��ʵ�һᷢ��,expected_pos��ʵ����һ������λ��, �ܺü���. ������xfile_seek()�е�file->pos�Ǿ����ܸ��ӵļ����, �����Ƿ���Ҫ���´�
	if (xfile_tell(file) != expected_pos) {
		printf("seek error02.\n");
		return -1;
	}

	//�����file->pos��ȡ����
	count = xfile_read(read_buffer, 1, 1, file); //��file->pos��ʼ��ȡ, ��ȡԪ�ش�СΪ1�ֽ�, Ԫ�ظ���Ϊ1��, ���Ծ���һ���ֽ�
	if (count < 1) {
		printf("seek error03\n"); //˵��û�ж����ĸ���
		return -1;
	}

	//��֤, ��������һ���ֽ�, ����ֵ�ǲ���expected % 256, ֮������256, ��Ϊ����ÿ���ֽ���00,01,02...FE,FF.Ȼ���ٴ�ͷ��ʼ//����һ��2^8=256���ֽ�
	if (*(u8_t*)read_buffer != (expected_pos % 256)) {
		printf("seek error04\n"); 
		return -1;
	}

	//printf("sucess: %d\n",offset);
	return 0;
}


//4.9 ��λ����
int fs_seek_test(void) {
	xfat_err_t err;
	xfile_t file;

	printf("\n file seek test!\n");
	err = xfile_open(&xfat, &file, "/seek/1MB.bin"); //���ļ�
	if (err != FS_ERR_OK) {
		printf("open file failed.\n");
		return -1;
	}

	//���Դ���ʼ��ʼ
	err = fs_seek(&file, XFAT_SEEK_SET, 32); //����: һ��������
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_SET, 576); //����: һ��������,һ������
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_SET, 4193); //����: һ������. һ������512*4=2048. ���ﳬ��������:2048*2=4096
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_SET, -1); //����: �Ƿ�
	if (err == FS_ERR_OK) return err;

	//�ӵ�ǰλ�ÿ�ʼ
	err = fs_seek(&file, XFAT_SEEK_CUR, 32);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, 576);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, 4193);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, -32); //����
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, -512);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, -1024);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_CUR, -0xFFFFFFF); //����: ��Ϊ�����ƶ���һ��������
	if (err == FS_ERR_OK) return err;

	//�ӽ�β��ʼ
	err = fs_seek(&file, XFAT_SEEK_END, -32); //����
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_END, -576);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_END, -4193);
	if (err) return err;
	err = fs_seek(&file, XFAT_SEEK_END, 32); //����
	if (err == FS_ERR_OK) return err;


	//�ر�
	xfile_close(&file);
	return 0;
}

//4.10 ����������
xfat_err_t fs_modify_file_test(void) {
	xfat_err_t err;
	xfile_t file;
	const char* dir_path = "/modify/a0/a1/a2/";
	const char* file_name1[] = "ABC.efg";
	const char* file_name2[] = "efg.ABC";
	char curr_path[64];

	printf("modify file attr test..\n");
	printf("\n Before rename:\n");

	err = xfile_open(&xfat, &file, dir_path); //��Ŀ¼
	if (err < 0) {
		printf("open dir failed.\n");
		return err;
	}

	err = list_sub_file(&file, 0);
	if(err < 0)
	{
		return err;
	}

	xfile_close(&file);

	sprintf(curr_path, "%s%s", dir_path, file_name1);
	err = xfile_open(&xfat, &file, curr_path);
}

int main(void)
{
	xfat_err_t err;
	
	//2.2 ��disk�ļ�
	err = xdisk_open(&disk, "vdisk", &vdisk_driver, (void*)disk_path);
	if (err) {
		printf("open disk failed.\n");
		return -1;
	}

	//2.2 �򿪺�, 
	err = disk_part_test();//���öϵ�
	if (err) return err;

	//3.1 ��ȡ��0������(��һ����������512�ֽڵ�mbr)����Ϣ. ע��, ��ʵ����disk_part���潫�ᱣ��ָ���FAT32���ֵ���ʼ��ַ, ��disk_part.start_sector
	err = xdisk_get_part(&disk, &disk_part, 1); //��ȡ��0��
	if (err < 0) {
		printf("read partition failed!\n");
		return -1;
	}

	//3.2 ����
	err = xfat_open(&xfat, &disk_part);
	if (err < 0) return err;

	//3.3 ����: fatĿ¼�Ĳ��� //ע��, �����fat_dir_test(), ����disk_part_test(), ��Ȼ��fat_dir_test(), ��Ҫ������fat_open()����
	//err = fat_dir_test(); //3.3 ����ֻ�Ǽ���˸�Ŀ¼�ĵ�һ���������N��Ŀ¼��, ���Ǹ�Ŀ¼�����еĴ�
	//if (err) return err;

	//3.5 ����: ��ӡ64.txt����Ϣ
	//err = fat_file_test();
	//if (err) return err;

	//4.1 ����
	//err = fs_open_test();
	//if (err < 0) return err;

	//4.4 ����: Ŀ¼�ı�������
	//err = dir_trans_test();
	//if (err < 0) return err;

	//4.8 ����: ��ȡ�ļ�
	//err = fs_read_test();
	//if (err < 0) printf("read test failed.\n");

	//4.9 ����: ��λ
	err = fs_seek_test();
	if (err) return err;

	//4.10 ����: �ļ��޸�, ������
	err = fs_modify_file_test();
	if (err) return err;

	//2.2 �ر�
	err = xdisk_close(&disk);
	if (err) {
		printf("close disk failed.\n");
		return -1;
	}

	printf("Test End");
	return 0;
}