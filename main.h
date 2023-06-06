#ifndef _MAIN_H
#define _MAIN_H

extern char current_path[256];
	
extern void help(void);				//帮助 
extern void initialize_memory(void);
extern void format(void);
extern void cd(char tmp[9]);		//进入目录 
extern void md(char tmp[9]);//新建目录 
extern void touch(char tmp[9]);	//创建文件 
extern void rd(char tmp[9]);		//删除目录 
extern void del(char tmp[9],int f,unsigned short node_i);		//删除文件 
extern void open_file(char tmp[9]);
extern void close_file(char tmp[9]);
extern void more(char tmp[9]);		//读文件 
extern void write_file(char tmp[9]);//写文件 
extern void my_dir(void);			//显示目录
extern void my_dir_s(void); 
extern void check_disk(void);
extern void help(void);				// 
extern void my_time(void);			//当前时间
extern void file_time(char tmp[9]); //文件最近修改时间 
extern void my_rename(char tmp[9],char new_name[9]);//重命名 
extern void my_copy(char tmp[9],char path[9]);
extern void my_move(char tmp[9],char path[9]);
extern void my_export(char tmp[9],char path[9]);
extern void my_import(char path[9]);
extern void ver();
extern void my_ver(char tmp[9]);
#endif
