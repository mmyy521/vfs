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
        if(!strcmp(command,"cd")) 		//һ ���뵱ǰĿ¼��
        {
            scanf("%s",temp);
            cd(temp);
        }
        else if(!strcmp(command,"md"))  //�� ������Ŀ¼
        {
            scanf("%s",temp);
            md(temp);
        }
        else if(!strcmp(command,"touch")) //�� �����ļ�
        {
            scanf("%s",temp);
            touch(temp);
        }
        else if(!strcmp(command,"rd"))  //�� ɾ����Ŀ¼�����ܵݹ�ɾ�� 
        {
            scanf("%s",temp);
            rd(temp);
        }
        else if(!strcmp(command,"del"))//�� ɾ���ļ�
        {
            scanf("%s",temp);
            del(temp,0,1);
        }
        else if(!strcmp(command,"more"))    //�� ��һ���ļ�
        {
            scanf("%s",temp);
            more(temp);
        }
        else if(!strcmp(command,"write"))   //�� дһ���ļ�
        {
            scanf("%s",temp);
            write_file(temp);
        }
        else if(!strcmp(command,"dir"))      //�� ��ʾ��ǰĿ¼
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
			if(ch=='\n')//ֱ�ӻس��Ļ�����ʾϵͳʱ�� 
			{
				my_time();
				continue;	
			}
        	scanf("%s",temp);
			file_time(temp);	//��ֱ�ӻس�����ʾ�ļ�����ʱ�� 
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
        else if(!strcmp(command,"format"))  //��ʽ��Ӳ��
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
        else if(!strcmp(command,"ckdisk"))  //���Ӳ��
        {
        	check_disk();
        }
        else if(!strcmp(command,"exit"))    //�˳�ϵͳ
        {
        	break;
        }
        else printf("No this Command,Please check!\n");
        getchar();
        //while((getchar())!='\n');
    }
    return 0;
}

