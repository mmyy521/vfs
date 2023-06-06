#include <stdio.h>
#include <string.h>
#include "main.h"

int main(int argc,char **argv)
{
    char command[10],temp[9];
    initialize_memory();
    while(1)
    {
        printf("%s]#",current_path);
        scanf("%s",command);
        //printf("%s \n",command);
        if(!strcmp(command,"cd")) 		//一 进入当前目录下
        {
            scanf("%s",temp);
            cd(temp);
        }
        else if(!strcmp(command,"md"))  //二 创建空目录
        {
            scanf("%s",temp);
            md(temp);
        }
        else if(!strcmp(command,"touch")) //三 创建文件
        {
            scanf("%s",temp);
            touch(temp);
        }
        else if(!strcmp(command,"rd"))  //四 删除空目录，不能递归删除 
        {
            scanf("%s",temp);
            rd(temp);
        }
        else if(!strcmp(command,"del"))//五 删除文件
        {
            scanf("%s",temp);
            del(temp,0,1);
        }
        else if(!strcmp(command,"more"))    //六 读一个文件
        {
            scanf("%s",temp);
            more(temp);
        }
        else if(!strcmp(command,"write"))   //七 写一个文件
        {
            scanf("%s",temp);
            write_file(temp);
        }
        else if(!strcmp(command,"dir"))      //八 显示当前目录
        {
        	char ch=getchar();
			if(ch=='\n')
			{
				my_dir();
				continue;	
			}
        	scanf("%s",temp);
        	if(!strcmp(temp,"/s"))
			my_dir_s();
			else
			my_dir_single(temp);
        }
        else if(!strcmp(command,"time"))   
        {
        	char ch=getchar();
			if(ch=='\n')//直接回车的话就显示系统时间 
			{
				my_time();
				continue;	
			}
        	scanf("%s",temp);
			file_time(temp);	//不直接回车就显示文件更改时间 
        }
        else if(!strcmp(command,"ver"))   
        {
        	char ch=getchar();
			if(ch=='\n')
			{
				ver();
				continue;	
			}
        	scanf("%s",temp);
			my_ver(temp);	
        }
        else if(!strcmp(command,"help"))  // help 
        {
        	help();
        }
        else if(!strcmp(command,"rename"))  // 
        {
        	char new_name[9];
        	scanf("%s",temp); 
			getchar();
			scanf("%s",new_name);
        	my_rename(temp,new_name);
        }
        else if(!strcmp(command,"copy"))
        {
        	char path[9];
        	scanf("%s",temp); 
			getchar();
			scanf("%s",path);
        	my_copy(temp,path);
		}
		else if(!strcmp(command,"move"))
        {
        	char path[9];
        	scanf("%s",temp); 
			getchar();
			scanf("%s",path);
        	my_move(temp,path);
		}
		else if(!strcmp(command,"export"))
        {
        	char path[9];
        	scanf("%s",temp); 
			getchar();
			scanf("%s",path);
        	my_export(temp,path);
		}
		else if(!strcmp(command,"import"))
        {
        	char path[9];
			scanf("%s",path);
        	my_import(path);
		}
        else if(!strcmp(command,"format"))  //格式化硬盘
        {
            char tempch;
            printf("Format will erase all the data in the Disk\n");
            printf("Are you sure?y/n:\n");
            fflush(stdin);
            scanf(" %c",&tempch);
            if(tempch=='Y'||tempch=='y')
            {
                format();
            }
            else
            {
            	printf("Format Disk canceled\n");
            }
        }
        else if(!strcmp(command,"ckdisk"))  //检查硬盘
        {
        	check_disk();
        }
        else if(!strcmp(command,"exit"))    //退出系统
        {
        	break;
        }
        else printf("No this Command,Please check!\n");
        getchar();
        //while((getchar())!='\n');
    }
    return 0;
}

