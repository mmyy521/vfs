#ifndef _MAIN_H
#define _MAIN_H

extern char current_path[256];
	
extern void help(void);				//���� 
extern void initialize_memory(void);
extern void format(void);
extern void cd(char tmp[9]);		//����Ŀ¼ 
extern void md(char tmp[9]);//�½�Ŀ¼ 
extern void touch(char tmp[9]);	//�����ļ� 
extern void rd(char tmp[9]);		//ɾ��Ŀ¼ 
extern void del(char tmp[9],int f,unsigned short node_i);		//ɾ���ļ� 
extern void open_file(char tmp[9]);
extern void close_file(char tmp[9]);
extern void more(char tmp[9]);		//���ļ� 
extern void write_file(char tmp[9]);//д�ļ� 
extern void my_dir(void);			//��ʾĿ¼
extern void my_dir_s(void); 
extern void check_disk(void);
extern void help(void);				// 
extern void my_time(void);			//��ǰʱ��
extern void file_time(char tmp[9]); //�ļ�����޸�ʱ�� 
extern void my_rename(char tmp[9],char new_name[9]);//������ 
extern void my_copy(char tmp[9],char path[9]);
extern void my_move(char tmp[9],char path[9]);
extern void my_export(char tmp[9],char path[9]);
extern void my_import(char path[9]);
extern void ver();
extern void my_ver(char tmp[9]);
#endif
