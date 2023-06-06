#include <stdio.h>		 
#include <string.h>
#include <time.h>
#include "main.h"

#define VOLUME_NAME	"EXT2FS"   // �ļ�ϵͳ��
#define BLOCK_SIZE 512	       // ���С
#define DISK_SIZE	4612	   //�����ܿ���

#define DISK_START 0	       // ���̿�ʼ��ַ
#define SB_SIZE	32	           //�������С��32B

#define	GD_SIZE	32	           // ������������С��32B
#define GDT_START	(0+512)    // ������������ʼ��ַ

#define BLOCK_BITMAP (512+512) // ��λͼ��ʼ��ַ
#define INODE_BITMAP (1024+512)// inode λͼ��ʼ��ַ

#define INODE_TABLE (1536+512) // �����ڵ����ʼ��ַ 4*512
#define INODE_SIZE 64	       // ÿ��inode�Ĵ�С��64B
#define INODE_TABLE_COUNTS	4096 // inode entry ��

#define DATA_BLOCK (263680+512)	// ���ݿ���ʼ��ַ 4*512+4096*64
#define DATA_BLOCK_COUNTS	4096	// ���ݿ���

#define BLOCKS_PER_GROUP	4612 // ÿ���еĿ���

struct super_block // 32 B
{
	char sb_volume_name[16]; 			//�ļ�ϵͳ��
	unsigned short sb_disk_size; 		//�����ܴ�С
	unsigned short sb_blocks_per_group; //ÿ���еĿ���
	unsigned short sb_size_per_block;	//���С
	char sb_pad[10];   					//���� 
};

struct group_desc // 32 B
{
    char bg_volume_name[16]; //�ļ�ϵͳ��---16 
    unsigned short bg_block_bitmap; //��λͼ����ʼ���---2 
    unsigned short bg_inode_bitmap; //inodeλͼ����ʼ���---2 
    unsigned short bg_inode_table; //inode�����ʼ���---2 
    unsigned short bg_free_blocks_count; //���п�ĸ���---2 
    unsigned short bg_free_inodes_count; //����inode�ĸ���---2 
    unsigned short bg_used_dirs_count; //�����Ŀ¼�ܽ����---2 
    char bg_pad[4]; //���� 
};

struct inode // 64 B
{
    unsigned short i_mode;   //�ļ����ͼ�����Ȩ��---2
    unsigned short i_blocks; //�ļ���ռ�����ݿ����(0~7), ���Ϊ7��--2
    unsigned short i_flags;  //���ļ��ķ�ʽ---2
    unsigned long i_size;    // �ļ���Ŀ¼��С(��λ byte)---4
    char i_ctime[19];   //20����ʱ��----19
    char i_mtime[19];   //20�޸�ʱ��----19
    unsigned short i_block[8]; //�洢���ݿ��---16
};
struct dir_file //16B
{
    unsigned short inode; //�����ڵ��---2 
    unsigned short rec_len; //Ŀ¼���---2 
    unsigned short name_len; //�ļ�������---2 
    char file_type; //�ļ�����(1 ��ͨ�ļ� 2 Ŀ¼.. )---1 
    char name[9]; //�ļ���
};


static unsigned short last_alloc_inode; // ��������inode�� 
static unsigned short last_alloc_block; // �����������ݿ�� 
static unsigned short current_dir;   // ��ǰĿ¼��inode�� 
char par_path[9];
static unsigned short current_dirlen; 

static short fopen_table[16]; 

static struct super_block sb_block[1];	// �����黺����
static struct group_desc gdt[1];	// ��������������
static struct inode inode_area[1];  // inode������
static unsigned char bitbuf[512]={0}; // λͼ������
static unsigned char ibuf[512]={0};	//���ݿ�λͼ������ 
static struct dir_file dir[32];   //Ŀ¼����� 32*16=512
static char Buffer[512];  // ������ݿ�Ļ�����
static char tempbuf[4096];	// �ļ�д�뻺����
static FILE *fp;	// �������ָ��


char current_path[256];    // ��ǰ·���� 
//�ڵ�ǰĿ¼�²�ѯ�ļ� 
static unsigned short find_file(char tmp[9],int file_type,unsigned short *inode_num,unsigned short *block_num,unsigned short *dir_num);//�����ļ�
//��ָ��Ŀ¼�²�ѯ�ļ� 
static unsigned short find_file_in(char tmp[9],int file_type,unsigned short node_i,unsigned short *inode_num,unsigned short *block_num,unsigned short *dir_num);//�����ļ�
//��ʼ��Ŀ¼���ļ���Ϣ 
static void dir_file_prepare(unsigned short node_i,unsigned short node_i_len,unsigned short tmp,unsigned short len,int type);
static void remove_block(unsigned short del_num);//ɾ�����ݿ�
static void remove_inode(unsigned short del_num);//ɾ��inode�ڵ�
static unsigned short search_file(unsigned short Ino);//�ڴ��ļ����в����Ƿ��Ѵ��ļ�

static void initialize_disk(void);//��ʼ������
static void get_time(char my_time[19]); 
//-----------��ȡʱ��---------------- 
static void get_time(char my_time[19]) 
{
    time_t timep;
    time(&timep);
    struct tm *p;
    p = gmtime(&timep);
	if(p->tm_hour+8>23)
    {
    	p->tm_hour=0;
    	p->tm_mday+=1;
	}
	else
	p->tm_hour+=8;
    snprintf(my_time, 19, "%d-%d-%d %d:%d", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min);
}
void reversestring(char *b)
{
	char c=0;
	int i=0,size;
	int count=strlen(b);
	//ע���һ���±��0��ʼ
	size=count-1;
    if (size==0 || size==1)
		//��ַ����
		b=b;
	for(;i<=size;i++)
	{ 
		// �ַ��������������ȼ� ��һ���ַ����
		c=b[i];
		b[i]=b[size];
		b[size]=c;
		size--;
	}
}
//-----------�������д-------------- 
static void update_super_block(void) //д������
{
    fp=fopen("./my_file","r+");
    fseek(fp,DISK_START,SEEK_SET);//���̿�ʼ��ַ0,
    //int fseek(FILE *stream, long offset, int fromwhere);
    //stream��ָ����fromwhereΪ��׼��ƫ��offset���ֽڵ�λ�á�
    //SEEK_SET�� �ļ���ͷ
	//SEEK_CUR�� ��ǰλ��
	//SEEK_END�� �ļ���β
	/*size_t fwrite(const void* buffer,size_t size, size_t count, FILE* stream);
	���ļ���д��һ�����ݿ�
	1) buffer��һ��ָ�롣��fwrite��˵��Ҫд�����ݵĵ�ַ��
	2) size��Ҫд�����ݵĵ��ֽ�����
	3) count��Ҫ����д��size�ֽڵ�������ĸ�����
	4) streamΪĿ���ļ�ָ��
	5) ����ʵ��д������������count��*/
    fwrite(sb_block,SB_SIZE,1,fp);
    fflush(fp); //���̽��������������������֤�����ڴ����ݵ�һ����
    //fwrite[1]����д���û��ռ仺��������δͬ�����ļ��У�
	//�޸ĺ�Ҫ���ڴ����ļ�ͬ��
}

static void reload_super_block(void) //��������
{
    fseek(fp,DISK_START,SEEK_SET);		//0,��ͷ�� 
    fread(sb_block,SB_SIZE,1,fp);//��ȡ���ݵ������黺������
}
//-----------����������д-------------- 
static void update_group_desc(void) //д��������
{
    fp=fopen("./my_file","r+");
    fseek(fp,GDT_START,SEEK_SET);//����������ʼ��ַ512 ����ͷ��ʼ�� 
    fwrite(gdt,GD_SIZE,1,fp);	//������������С��32B
    fflush(fp);
}

static void reload_group_desc(void) // ����������
{

    fseek(fp,GDT_START,SEEK_SET);
    fread(gdt,GD_SIZE,1,fp);
}
//-----------inode��д-------------- 
static void update_inode_entry(unsigned short i) // д��i��inode
{
    fp=fopen("./my_file","r+");// �����ڵ����ʼ��ַ 4*512
    fseek(fp,INODE_TABLE+(i-1)*INODE_SIZE,SEEK_SET);//ƫ������iNode����ʼ��ַ+i��iNode��С 64
    fwrite(inode_area,INODE_SIZE,1,fp);
    fflush(fp);
}

static void reload_inode_entry(unsigned short i) // ����i��inode
{
    fseek(fp,INODE_TABLE+(i-1)*INODE_SIZE,SEEK_SET);
    fread(inode_area,INODE_SIZE,1,fp);
}
//-----------Ŀ¼���ݿ��д-------------- 
static void update_dir(unsigned short i) //   д��i�� ���ݿ�
{
    fp=fopen("./my_file","r+");//���ݿ���ʼ��ַ 4*512+4096*64 
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fwrite(dir,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_dir(unsigned short i) // ����i�� ���ݿ�
{
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fread(dir,BLOCK_SIZE,1,fp);
    //fclose(fp);
}
//-----------blackλͼ��д--------------
static void update_block_bitmap(void) //дblockλͼ
{
    fp=fopen("./my_file","r+");
    fseek(fp,BLOCK_BITMAP,SEEK_SET);//��λͼ��ʼ��ַ512+512 
    fwrite(bitbuf,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_block_bitmap(void) //��blockλͼ
{
    fseek(fp,BLOCK_BITMAP,SEEK_SET);
    fread(bitbuf,BLOCK_SIZE,1,fp);
}
//-----------iNodeλͼ��д--------------
static void update_inode_bitmap(void) //дinodeλͼ
{
    fp=fopen("./my_file","r+");
    fseek(fp,INODE_BITMAP,SEEK_SET);// 1024+512 ��inode λͼ��ʼ��ַ
    fwrite(ibuf,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_inode_bitmap(void) // ��inodeλͼ
{
    fseek(fp,INODE_BITMAP,SEEK_SET);
    fread(ibuf,BLOCK_SIZE,1,fp);
}
//-----------���ݿ��д--------------
static void update_block(unsigned short i) // д��i�����ݿ�
{
    fp=fopen("./my_file","r+");
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    //fseek(fp,0,SEEK_SET);
    fwrite(Buffer,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_block(unsigned short i) // ����i�����ݿ�
{
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fread(Buffer,BLOCK_SIZE,1,fp);
}

static int alloc_block(void) // ����һ�����ݿ�,�������ݿ��
{
	//�����ʱ���������������ݿ��/8�õ�������ֽں� 
    //λͼ������ bitbuf����512���ֽڣ���ʾ512��8=4096�����ݿ顣
	//����last_alloc_block/8��������bitbuf����һ���ֽ�
    unsigned short cur=last_alloc_block;	// �����������ݿ�� 
    //printf("cur: %d\n",cur);
    unsigned char con=128; // 1000 0000b
    int flag=0;
    if(gdt[0].bg_free_blocks_count==0)//û�пɷ���Ŀ��� �����������������п�ĸ�����0 
    {
        printf("There is no block to be alloced!\n");
        return(0);
    }
    reload_block_bitmap();		//��ȡλͼ������������ 
    cur/=8;		//��������һ���ֽ�   bit---�ֽ� 
    //��λͼ������bitbuf�ĵ�cur���ֽڿ�ʼ�ж� 
    while(bitbuf[cur]==255)	//���ֽڵ�8��bit����������   
    {			//2^8
        if(cur==511)
		cur=0; 					//���һ���ֽ�Ҳ�Ѿ�������ͷ��ʼѰ��
        else cur++;
    }
    while(bitbuf[cur]&con) 		//�������룬��Ϊ1��Ϊ1����һ���ֽ����Ҿ����ĳһ��bit
    {
        con=con/2;
        flag++;
    }
    bitbuf[cur]=bitbuf[cur]+con;    //bitbuf��ֵʱ����λռ�������ֵ �� 
    last_alloc_block=cur*8+flag;	//���������������ݿ�� 

    update_block_bitmap();		//����λͼ 
    gdt[0].bg_free_blocks_count--;	//���»��������п��� 
    update_group_desc();		//������ 
    return last_alloc_block;	//�����ҵ������ݿ�� 
}

static int get_inode(void) // ����һ��inode
{
    unsigned short cur=last_alloc_inode;
    unsigned char con=128;
    int flag=0;
    if(gdt[0].bg_free_inodes_count==0)
    {
        printf("There is no Inode to be alloced!\n");
        return 0;
    }
    reload_inode_bitmap();
    cur=(cur-1)/8;   //��һ�������1�����Ǵ洢�Ǵ�0��ʼ��
    while(ibuf[cur]==255) //�ȿ����ֽڵ�8��λ�Ƿ��Ѿ�����
    {
        if(cur==511)	//���һ���ֽ�Ҳ�Ѿ�������ͷ��ʼѰ��
		cur=0;
        else cur++;
    }
    while(ibuf[cur]&con)  //�ٿ�ĳ���ֽڵľ�����һλû�б�ռ��
    {
        con=con/2;
        flag++;
    }
    ibuf[cur]=ibuf[cur]+con;
    last_alloc_inode=cur*8+flag+1;   // 
    update_inode_bitmap();
    gdt[0].bg_free_inodes_count--;
    update_group_desc();
    //printf("%d\n",last_alloc_inode);
    return last_alloc_inode;
}

//��ǰĿ¼�в����ļ���Ŀ¼Ϊtmp�����õ����ļ��� inode �ţ�
//�������ϼ�Ŀ¼�е����ݿ���Լ����ݿ���Ŀ¼�����
static unsigned short find_file(char tmp[9],int file_type,unsigned short *inode_num,
														unsigned short *block_num,unsigned short *dir_num)
{
    unsigned short j,k;
    reload_inode_entry(current_dir); //���뵱ǰĿ¼����ǰĿ¼�Ľڵ��  
    j=0;
    while(inode_area[0].i_blocks>j)//inode���������ļ���ռ�����ݿ����(0~7), ���Ϊ7
    {
        reload_dir(inode_area[0].i_block[j]);///�������ݿ���ҵ�Ŀ¼���λ�� 
        k=0;
        while(k<32)
        {	//����ڵ㲻���ڻ������Ͳ�һ�»��߲��Ǵ��ļ��� 
            if(!dir[k].inode||dir[k].file_type!=file_type||strcmp(dir[k].name,tmp))
            {
            	k++;
            }
            else
            {
                *inode_num=dir[k].inode;		//�ҵ����� 
                *block_num=j;
                *dir_num=k;
                return 1;
            }
        }
        j++;
    }
    return 0;
}
static unsigned short find_file_in(char tmp[9],int file_type,unsigned short node_i,unsigned short *inode_num,
														unsigned short *block_num,unsigned short *dir_num)
{
    unsigned short j,k;
    reload_inode_entry(node_i); //���뵱ǰĿ¼����ǰĿ¼�Ľڵ��  
    j=0;
    while(inode_area[0].i_blocks>j)//inode���������ļ���ռ�����ݿ����(0~7), ���Ϊ7
    {
        reload_dir(inode_area[0].i_block[j]);///�������ݿ���ҵ�Ŀ¼���λ�� 
        k=0;
        while(k<32)
        {	//����ڵ㲻���ڻ������Ͳ�һ�»��߲��Ǵ��ļ��� 
            if(!dir[k].inode||dir[k].file_type!=file_type||strcmp(dir[k].name,tmp))
            {
            	k++;
            }
            else
            {
                *inode_num=dir[k].inode;		//�ҵ����� 
                *block_num=j;
                *dir_num=k;
                return 1;
            }
        }
        j++;
    }
    return 0;
}
/*Ϊ����Ŀ¼���ļ����� dir_file
���������ļ���ֻ�����һ��inode��
��������Ŀ¼������inode���⣬����Ҫ�����������洢.��..����Ŀ¼��*/
						//�����ӵ�inode�� ���ļ�/Ŀ¼�ĳ��� ������ 
static void dir_file_prepare(unsigned short node_i,unsigned short node_i_len,unsigned short tmp,unsigned short len,int type)
{//�˲����ϼ�Ŀ¼ ԭ������".." �洢���󣬳��� 
    reload_inode_entry(tmp);//tmp��iNode��
	char time[19];
	get_time(time);
	strcpy(inode_area[0].i_ctime,time);	//����ʱ�丳ֵ 
	strcpy(inode_area[0].i_mtime,time); 
    if(type==2) // Ŀ¼
    {
        inode_area[0].i_size=32;  //��С32B ,���".",".." 
        inode_area[0].i_blocks=1;
        inode_area[0].i_block[0]=alloc_block();//��������ݿ�� 
        //Ŀ¼����� 
        dir[0].inode=tmp;			//�����ӵ�iNode�� 
        dir[1].inode=node_i;	//�ϼ�Ŀ¼��iNode�� 
        dir[0].name_len=len;
        dir[1].name_len=node_i_len;
        dir[0].file_type=dir[1].file_type=2;

        for(type=2;type<32;type++)//�Ѵ�Ŀ¼������λ�ø�Ϊ0 
            dir[type].inode=0;
        strcpy(dir[0].name,".");
        strcpy(dir[1].name,"..");
        update_dir(inode_area[0].i_block[0]);//���´�inode��һ�����ݿ��dir��Ϣ 

        inode_area[0].i_mode=01006;//˵����Ŀ¼�ļ� 
    }
    else
    {
        inode_area[0].i_size=0;
        inode_area[0].i_blocks=0;
        inode_area[0].i_mode=0407;//˵������ͨ�ļ� 
    }
//  printf("\ndirprepare\nblocks.--%d\t",inode_area[0].i_blocks);// 
//	printf("ctime.--%s\t",inode_area[0].i_ctime);//
//	printf("mtime.--%s\t",inode_area[0].i_mtime); //
//	printf("flags.--%d\t",inode_area[0].i_flags);//
//	printf("mode.--%d\t",inode_area[0].i_mode); //
//	printf("size.--%d\t\n",inode_area[0].i_size);  //96
    update_inode_entry(tmp);
}

//ɾ��һ�����

static void remove_block(unsigned short del_num)
{
    unsigned short tmp;
    tmp=del_num/8;
    reload_block_bitmap();
    switch(del_num%8) // ����blockλͼ �������λ��Ϊ0
    {
        case 0:bitbuf[tmp]=bitbuf[tmp]&127;break; // bitbuf[tmp] & 0111 1111b
        case 1:bitbuf[tmp]=bitbuf[tmp]&191;break; //bitbuf[tmp]  & 1011 1111b
        case 2:bitbuf[tmp]=bitbuf[tmp]&223;break; //bitbuf[tmp]  & 1101 1111b
        case 3:bitbuf[tmp]=bitbuf[tmp]&239;break; //bitbbuf[tmp] & 1110 1111b
        case 4:bitbuf[tmp]=bitbuf[tmp]&247;break; //bitbuf[tmp]  & 1111 0111b
        case 5:bitbuf[tmp]=bitbuf[tmp]&251;break; //bitbuf[tmp]  & 1111 1011b
        case 6:bitbuf[tmp]=bitbuf[tmp]&253;break; //bitbuf[tmp]  & 1111 1101b
        case 7:bitbuf[tmp]=bitbuf[tmp]&254;break; // bitbuf[tmp] & 1111 1110b
    }
    update_block_bitmap();
    gdt[0].bg_free_blocks_count++;
    update_group_desc();
}


//ɾ��һ��inode ��

static void remove_inode(unsigned short del_num)
{
    unsigned short tmp;
    tmp=(del_num-1)/8;
    reload_inode_bitmap();
    switch((del_num-1)%8)//����blockλͼ
    {
        case 0:bitbuf[tmp]=bitbuf[tmp]&127;break;
        case 1:bitbuf[tmp]=bitbuf[tmp]&191;break;
        case 2:bitbuf[tmp]=bitbuf[tmp]&223;break;
        case 3:bitbuf[tmp]=bitbuf[tmp]&239;break;
        case 4:bitbuf[tmp]=bitbuf[tmp]&247;break;
        case 5:bitbuf[tmp]=bitbuf[tmp]&251;break;
        case 6:bitbuf[tmp]=bitbuf[tmp]&253;break;
        case 7:bitbuf[tmp]=bitbuf[tmp]&254;break;
    }
    update_inode_bitmap();
    gdt[0].bg_free_inodes_count++;
    update_group_desc();
}

//�ڴ��ļ����в����Ƿ��Ѵ��ļ�
static unsigned short search_file(unsigned short Inode)
{
    unsigned short fopen_table_point=0;//����ļ��򿪱��е�inode�� 
    while(fopen_table_point<16&&fopen_table[fopen_table_point++]!=Inode);
    //�ҵ��ļ��򿪱����ļ�λ�� 
    if(fopen_table_point==16)
    {
    	return 0;
    }
    return 1;
}

void initialize_disk(void)  //��ʼ������
{
    int i=0;
    last_alloc_inode=1;//inode��1��ʼ 
    last_alloc_block=0;//���ݿ��0��ʼ 
    for(i=0;i<16;i++)
    {
    	fopen_table[i]=0; //��ջ����
    }
    for(i=0;i<BLOCK_SIZE;i++)//���С��512B 
    {
    	Buffer[i]=0; // ��ջ����������ݿ�ȫ�����0 
    }
    if(fp!=NULL)
    {
    	fclose(fp);
    }
    fp=fopen("./my_file","w+"); //���ļ���С��4612*512=2361344B���ô��ļ���ģ���ļ�ϵͳ
    fseek(fp,DISK_START,SEEK_SET);//���ļ�ָ���0��ʼ
    for(i=0;i<DISK_SIZE;i++)
    {
    	fwrite(Buffer,BLOCK_SIZE,1,fp); // ����ļ�������մ���ȫ����0��� BufferΪ��������ʼ��ַ��BLOCK_SIZE��ʾ��ȡ��С��1��ʾд�����ĸ���*/
    }
    // ��ʼ������������
    reload_super_block();
    strcpy(sb_block[0].sb_volume_name,VOLUME_NAME);
    sb_block[0].sb_disk_size=DISK_SIZE;  //�����ܿ���4612 
    sb_block[0].sb_blocks_per_group=BLOCKS_PER_GROUP; //ÿ�����ݿ���4612,1�� 
    sb_block[0].sb_size_per_block=BLOCK_SIZE;//512
    update_super_block();
    // ��Ŀ¼��inode��Ϊ1
    reload_inode_entry(1);
	//��Ŀ¼�����ݿ��Ϊ0 
    reload_dir(0);
    strcpy(current_path,"[root@mcf /");  // �޸�·����Ϊ��Ŀ¼
    // ��ʼ��������������
    reload_group_desc();

    gdt[0].bg_block_bitmap=BLOCK_BITMAP; //��һ����λͼ����ʼ��ַ
    gdt[0].bg_inode_bitmap=INODE_BITMAP; //��һ��inodeλͼ����ʼ��ַ
    gdt[0].bg_inode_table=INODE_TABLE;   //inode�����ʼ��ַ
    gdt[0].bg_free_blocks_count=DATA_BLOCK_COUNTS; //�������ݿ���
    gdt[0].bg_free_inodes_count=INODE_TABLE_COUNTS; //����inode��
    gdt[0].bg_used_dirs_count=0; // ��ʼ�����Ŀ¼�Ľڵ�����0
    update_group_desc(); // ����������������

    reload_block_bitmap();
    reload_inode_bitmap();

    inode_area[0].i_mode=518;
    inode_area[0].i_blocks=0;
    inode_area[0].i_size=32;
    char time[19];
    get_time(time);
	strcpy(inode_area[0].i_ctime,time);
	//printf("time:%s\n",inode_area[0].i_ctime);
    strcpy(inode_area[0].i_mtime,time);
    inode_area[0].i_block[0]=alloc_block(); //�������ݿ�� 
 	inode_area[0].i_blocks++;
    current_dir=get_inode();//����һ��inode�� 
    update_inode_entry(current_dir);

    //��ʼ����Ŀ¼��Ŀ¼��
    dir[0].inode=dir[1].inode=current_dir;
    dir[0].name_len=0;
    dir[1].name_len=0;
    dir[0].file_type=dir[1].file_type=2;
    strcpy(dir[0].name,".");
    strcpy(dir[1].name,"..");
    update_dir(inode_area[0].i_block[0]);
    check_disk();
    fclose(fp);
}
//������״̬
void check_disk(void)
{
	reload_super_block();
	printf("File established!\n"); 
	printf("disk size    : %d(blocks)\n", sb_block[0].sb_disk_size);
	printf("file size    : %d(kb)\n", sb_block[0].sb_disk_size*sb_block[0].sb_size_per_block/1024);
	printf("block size   : %d(kb)\n", sb_block[0].sb_size_per_block);
}
//��ʼ���ڴ�
void initialize_memory(void)
{
    int i=0;
    last_alloc_inode=1;
    last_alloc_block=0;
    for(i=0;i<16;i++)
    {
    	fopen_table[i]=0;
    }
    strcpy(current_path,"[root@mcf /");
    current_dir=1;
    fp=fopen("./my_file","r+");
    if(fp==NULL)
    {
        initialize_disk();
        return ;
    }
    reload_super_block();
    if(strcmp(sb_block[0].sb_volume_name,VOLUME_NAME))
    {
    	printf("The File system [%s] is not suppoted yet!\n", sb_block[0].sb_volume_name);
    	printf("The File system loaded error!\n");
    	initialize_disk();
    	return ;
    }
    reload_group_desc();
}

//��ʽ��
void format(void)
{
    initialize_disk();
    initialize_memory();
}
//����ĳ��Ŀ¼��ʵ�����Ǹı�current_dir 
void cd(char tmp[9])
{
    unsigned short i,j,k,flag;
    flag=find_file(tmp,2,&i,&j,&k);//�����ҵ����Ŀ¼��inode��
							//�������ϼ�Ŀ¼�е����ݿ���Լ����ݿ���Ŀ¼�����
    if(flag)
    {
        current_dir=i;//���¸�ֵ��ǰĿ¼��inode�� 
        //printf("current:%d\n",i);
        if(!strcmp(tmp,"..")&&dir[k-1].name_len) ///�˵���һ��Ŀ¼
        {		//��ȥ
//        printf("-------------------------\n");
//        printf("k=%d\n",k);//1 
//        printf("name[k-1]:%s\n",dir[k-1].name);	//. 
//        printf("name_len[k-1]:%d\n",dir[k-1].name_len); 
//        printf("name[k]:%s\n",dir[k].name);		//..
//        printf("name_len[k]:%d\n",dir[k].name_len);
//        printf("current:%s\n",current_path);
            current_path[strlen(current_path)-dir[k-1].name_len-1]='\0';
            current_dirlen=dir[k].name_len;//����·������ 
//        printf("current:%s\n",current_path);
//        printf("currentlen:%d\n",current_dirlen);
//        printf("-------------------------\n");
        }
        else if(!strcmp(tmp,"."))//��ʾͣ���ڵ�ǰĿ¼ 
        {
        	return;
        }
        else if(strcmp(tmp,"..")) // cd ����Ŀ¼
        {
            current_dirlen=strlen(tmp);	//�ı�Ŀ¼���� 
            strcpy(par_path,tmp);
            strcat(current_path,tmp);	//�ı䵱ǰĿ¼ 
            strcat(current_path,"/");	//����/ 
        }
    }
    else
    {
    	printf("The directory %s not exists!\n",tmp);
    }
}

// �ڵ�ǰĿ¼�´���Ŀ¼������ָ��Ŀ¼�´���Ŀ¼
unsigned short mkdir(char tmp[9],char parent_path[9],unsigned short node_i,int f)
{
    unsigned short tmpno,i,j,k,flag,type=2;
    // ��ǰĿ¼������Ŀ¼���ļ�
    reload_inode_entry(node_i);			//��ָ��Ŀ¼��Ϣ������������ 
    if(f==1)		//��ָ��inode
	flag=find_file_in(tmp,type,node_i,&i,&j,&k);
	else			//�ǵ�ǰĿ¼�� 
	flag=find_file(tmp,type,&i,&j,&k);
    if(!flag) 	//���û������Ŀ¼ 
    {
        if(inode_area[0].i_size==4096) // Ŀ¼������
        {
            printf("Directory has no room to be alloced!\n");
            return;
        }
        flag=1;
        if(inode_area[0].i_size!=inode_area[0].i_blocks*512) // Ŀ¼����ĳЩ����32�� dir_file δ��
        {
            i=0;
            while(flag&&i<inode_area[0].i_blocks)
            {
                reload_dir(inode_area[0].i_block[i]);//�����ݿ���������� 
                j=0;
                while(j<32)
                {
                    if(dir[j].inode==0)
                    {
                        flag=0; //�ҵ�ĳ��δװ�������ݿ�
                        break;
                    }
                    j++;
                }
                i++;
            }
            tmpno=dir[j].inode=get_inode();//Ϊ����Ŀ¼����inode�� 
            dir[j].name_len=strlen(tmp);//Ŀ¼���ֳ� 
            dir[j].file_type=type;		//����Ŀ¼2	 
            strcpy(dir[j].name,tmp);	//Ŀ¼���� 
            update_dir(inode_area[0].i_block[i-1]);//���ĵ�dir��Ϣд������ 
        }
        else // ȫ�������ӿ�
        {
            inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
            inode_area[0].i_blocks++;
            reload_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
            tmpno=dir[0].inode=get_inode();
            dir[0].name_len=strlen(tmp);
            dir[0].file_type=type;
            strcpy(dir[0].name,tmp);
            // ��ʼ���¿������Ŀ¼��
            for(flag=1;flag<32;flag++)
            {
            	dir[flag].inode=0;
            }
            update_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
        }
        inode_area[0].i_size+=16;//�����ӵ�һ��dir_file��С��16B 

        update_inode_entry(node_i);//���µ�ǰĿ¼��inode 

        // �������ӵ�Ŀ¼inode��Ϣ��iNode�ţ�Ŀ¼�����ȣ�����2 
        dir_file_prepare(node_i,strlen(parent_path),tmpno,strlen(tmp),type); 
        return tmpno;
    }
    else  // �Ѿ�����ͬ���ļ���Ŀ¼
    {
        printf("Directory has already existed!\n");
    }

}
void md(char tmp[9])
{		//��Ŀ¼������ 
	mkdir(tmp,par_path,current_dir,0);
}
//�����ļ�
unsigned short cat(char tmp[9],unsigned short node_i,int f)
{
	unsigned short type=1;//�ļ����� 
    unsigned short tmpno,i,j,k,flag;
    reload_inode_entry(node_i);			//��ָ��Ŀ¼��Ϣ������������ 
    if(f==1)		//��ָ��inode
	flag=find_file_in(tmp,type,node_i,&i,&j,&k);
	else			//�ǵ�ǰĿ¼�� 
	flag=find_file(tmp,type,&i,&j,&k);
    if(!flag) 	//���û�������ļ� 
    {
        if(inode_area[0].i_size==4096)
        {
            printf("Directory has no room to be alloced!\n");
            return;
        }
        flag=1;
        if(inode_area[0].i_size!=inode_area[0].i_blocks*512)
        {//��ǰĿ¼�ܴ�С����ռȫ�����СС���ʹ����ҵ�û�з�ƥ��ĵط� 
            i=0;//i�ǿ�� 
            while(flag&&i<inode_area[0].i_blocks)
            {
                reload_dir(inode_area[0].i_block[i]);
                j=0;			//j��dir�� 
                while(j<32)
                {
                    if(dir[j].inode==0)//�ҵ���δ�����Ŀ¼��
                    {
                        flag=0;
                        break;
                    }
                    j++;
                }
                i++;
            }
            tmpno=dir[j].inode=get_inode();//����һ���µ�inode��
            dir[j].name_len=strlen(tmp);//��ֵ���ֳ��� 
            dir[j].file_type=type;
            strcpy(dir[j].name,tmp);
            update_dir(inode_area[0].i_block[i-1]);//�������ݿ����� 
        }
        else //����һ���µ����ݿ�
        {
            inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
            inode_area[0].i_blocks++;
            reload_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
            tmpno=dir[0].inode=get_inode();//��ʱ��ŷ��䵽�µ�inode�� 
            dir[0].name_len=strlen(tmp);
            dir[0].file_type=type;
            strcpy(dir[0].name,tmp);
            //��ʼ���¿�������ĿΪ0
            for(flag=1;flag<32;flag++)
            {
                dir[flag].inode=0;
            }
            update_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
        }
        inode_area[0].i_size+=16;
        char time[20];
	    get_time(time);
	    strcpy(inode_area[0].i_mtime,time);	//�������inode�Ǹ������ʱ�� 
        update_inode_entry(node_i);	//���µ�ǰinode��Ϣ 
        //�������ļ���inode�ڵ��ʼ��
        dir_file_prepare(node_i,strlen(tmp),tmpno,strlen(tmp),type);
        return tmpno;
    }
    else
    {
        printf("File has already existed!\n");
    }
}
void touch(char tmp[9])
{
	cat(tmp,current_dir,0);
} 
//ɾĿ¼
int  rmdir(char tmp[9],unsigned short node_i)
{
    unsigned short i,j,k,flag;
    unsigned short m,n,par_inode;
    int tflag=0;
    flag=find_file_in(tmp,2,node_i,&i,&j,&k);
    //printf("i:%d,,dir[k].inode:%d\n",i,dir[k].inode);��� 
    par_inode=dir[k].inode;
    if(flag)
    {
        reload_inode_entry(dir[k].inode); // ����Ҫɾ���Ľڵ�
        if(inode_area[0].i_size==32) 		//�������һ����Ŀ¼ 
        {
        	tflag=1;
            inode_area[0].i_size=0;
            inode_area[0].i_blocks=0;
			
            remove_block(inode_area[0].i_block[0]);
            // ���� tmp ���ڸ�Ŀ¼
            reload_inode_entry(node_i);
            reload_dir(inode_area[0].i_block[j]);
            remove_inode(dir[k].inode);
            dir[k].inode=0;
            update_dir(inode_area[0].i_block[j]);
            inode_area[0].i_size-=16;
            //��Ŀ¼���½��� 
            flag=0;
            /*ɾ��32 �� dir_file ȫΪ�յ����ݿ�
            ���� inode_area[0].i_block[0] ����Ŀ¼ . �� ..
            ����������ݿ�ķǿ� dir_file ������Ϊ0*/

            //printf("rm: %d\n",inode_area[0]);
            m=1;
            while(flag<32&&m<inode_area[0].i_blocks)
            {
                flag=n=0;
                reload_dir(inode_area[0].i_block[m]);
                while(n<32)
                {
                    if(!dir[n].inode)//�����0��������dir++ 
                    {
                    	flag++;
                    }
                    n++;
                }
                //���ɾ�������������ݿ��Ŀ¼��ȫ��Ϊ�ա���������������ɾ��ĳһ��λ��
                if(flag==32)
                {
                    remove_block(inode_area[0].i_block[m]);
                    inode_area[0].i_blocks--;
                    while(m<inode_area[0].i_blocks)//�����λ 
                    {
                    	inode_area[0].i_block[m]=inode_area[0].i_block[m+1];
                    	++m;
                    }
                }
            }
            update_inode_entry(node_i);//���µ�ǰĿ¼��Ϣ 
            return;
        }
        else//���ǿ�Ŀ¼���ݹ�ɾ�� 
        {
            int l;
            for(l=0;l<inode_area[0].i_blocks;l++)
            {
                reload_dir(inode_area[0].i_block[l]);
                int a;
                for(a=0;a<32;a++)
                {
                    if(!strcmp(dir[a].name,".")||!strcmp(dir[a].name,"..")||dir[a].inode==0)
                        continue;
                    //printf("type:%d\n",dir[a].file_type); 
                    if(dir[a].file_type==2)//��Ŀ¼ 
                    {
                        //printf("%s\n",dir[a].name);
                        rmdir(dir[a].name,par_inode);
                    }//�����Ǵ�ɾ���ļ������ֺ�������һ���ļ���inode 
                    else if(dir[a].file_type==1)//���ļ� 
                    {
                        //printf("%s\n",dir[a].name);
                        del(dir[a].name,1,par_inode);
                    }
                }
            }
        }
        // 
        inode_area[0].i_size=0;
            inode_area[0].i_blocks=0;
			
            remove_block(inode_area[0].i_block[0]);
            // ���� tmp ���ڸ�Ŀ¼
            reload_inode_entry(node_i);
            reload_dir(inode_area[0].i_block[j]);
            remove_inode(dir[k].inode);
            dir[k].inode=0;
            update_dir(inode_area[0].i_block[j]);
            inode_area[0].i_size-=16;
            //��Ŀ¼���½��� 
            flag=0;
            /*ɾ��32 �� dir_file ȫΪ�յ����ݿ�
            ���� inode_area[0].i_block[0] ����Ŀ¼ . �� ..
            ����������ݿ�ķǿ� dir_file ������Ϊ0*/

            //printf("rm: %d\n",inode_area[0]);
            m=1;
            while(flag<32&&m<inode_area[0].i_blocks)
            {
                flag=n=0;
                reload_dir(inode_area[0].i_block[m]);
                while(n<32)
                {
                    if(!dir[n].inode)//�����0��������dir++ 
                    {
                    	flag++;
                    }
                    n++;
                }
                //���ɾ�������������ݿ��Ŀ¼��ȫ��Ϊ�ա���������������ɾ��ĳһ��λ��
                if(flag==32)
                {
                    remove_block(inode_area[0].i_block[m]);
                    inode_area[0].i_blocks--;
                    while(m<inode_area[0].i_blocks)//�����λ 
                    {
                    	inode_area[0].i_block[m]=inode_area[0].i_block[m+1];
                    	++m;
                    }
                }
            }
            update_inode_entry(node_i);//���µ�ǰĿ¼��Ϣ 
        return tflag;
    }
    else
    {
    	printf("Directory to be deleted not exists!\n");
    }
}
void rd(char tmp[9])
{
	int flag=rmdir(tmp,current_dir);
}
//ɾ���ļ�
//�����Ǵ�ɾ���ļ����������ļ���inode 
void del(char tmp[9],int f,unsigned short node_i)
{
    unsigned short i,j,k,m,n,flag;
    if(f==1)
    flag=find_file_in(tmp,1,node_i,&i,&j,&k);
    else
    {
    	flag=find_file(tmp,1,&i,&j,&k);
		node_i=current_dir;	
	}
    m=0;
    if(flag)
    {
        reload_inode_entry(i); // ����ɾ���ļ� inode
        //ɾ���ļ���Ӧ�����ݿ�
        while(m<inode_area[0].i_blocks)
        {
        	remove_block(inode_area[0].i_block[m++]);//--->copy_block()
        }
        inode_area[0].i_blocks=0;
        inode_area[0].i_size=0;
        remove_inode(i);
        // ���¸�Ŀ¼
        reload_inode_entry(node_i);//����ɾ���ļ���inode�����Ϣ 
        reload_dir(inode_area[0].i_block[j]);//�ѵ�ǰĿ¼��inode�е�j block���ݿ���� 
        dir[k].inode=0; 				//ɾ��inode�ڵ�

        update_dir(inode_area[0].i_block[j]);
        inode_area[0].i_size-=16;//��Ϊ���ļ�����Ϣ��16B��dir 
        m=1;
        //ɾ��һ����������ݿ�Ϊ�գ��򽫸����ݿ�ɾ��
        while(m<inode_area[i].i_blocks)
        {
            flag=n=0;
            reload_dir(inode_area[0].i_block[m]);
            while(n<32)
            {
                if(!dir[n].inode)//����ǿյ�dir�� 
                {
                	flag++;
                }
                n++;
            }
            if(flag==32)			//ɾ����һ�����ݿ� 
            {
                remove_block(inode_area[i].i_block[m]);	//������ݿ� 
                inode_area[i].i_blocks--;
                while(m<inode_area[i].i_blocks)		//�ѿ�λ���� 
                {
                	inode_area[i].i_block[m]=inode_area[i].i_block[m+1];
                	++m;
                }
            }
        }
        update_inode_entry(node_i);//���µ�ǰĿ¼inode�� 
    }
    else
    {
    	printf("The file %s not exists!\n",tmp);
    }
}


//���ļ�
void open_file(char tmp[9])
{
    unsigned short flag,i,j,k;
    flag=find_file(tmp,1,&i,&j,&k);//�ҵ��ļ���λ�� 
    if(flag)
    {
        if(search_file(dir[k].inode))		// 
        {
        	printf("The file %s has opened!\n",tmp);
        }
        else
        {
            flag=0;
            while(fopen_table[flag])
            {
            	flag++;
            }
            fopen_table[flag]=dir[k].inode;
            //printf("File %s opened!\n",tmp);
        }
    }
    else printf("The file %s does not exist!\n",tmp);
}

//�ر��ļ�
void close_file(char tmp[9])
{
    unsigned short flag,i,j,k;
    flag=find_file(tmp,1,&i,&j,&k);

    if(flag)
    {
        if(search_file(dir[k].inode))
        {
            flag=0;
            while(fopen_table[flag]!=dir[k].inode)
            {
            	flag++;
            }
            fopen_table[flag]=0;
    //        printf("File %s closed!\n",tmp);
        }
        else
        {
        	printf("The file %s has not been opened!\n",tmp);
        }
    }
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    }
}

// ���ļ�
void more(char tmp[9])
{
	open_file(tmp);
    unsigned short flag,i,j,k,t;
    flag=find_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        if(search_file(dir[k].inode)) //���ļ���ǰ���Ǹ��ļ��Ѿ���
        {
            reload_inode_entry(dir[k].inode);
            //�ж��Ƿ��ж���Ȩ��
            if(!(inode_area[0].i_mode&4)) // i_mode:111b:��,д,ִ��
            {
                printf("The file %s can not be read!\n",tmp);
                return;
            }
            for(flag=0;flag<inode_area[0].i_blocks;flag++)
            {
                reload_block(inode_area[0].i_block[flag]);//�����ݿ�����д��buffer 
                for(t=0;t<inode_area[0].i_size-flag*512;++t)
                {
                	printf("%c",Buffer[t]);
                }
            }
            if(flag==0)
            {
            	printf("The file %s is empty!\n",tmp);
            }
            else
            {
            	printf("\n");
            }
        }
        else
        {
        	printf("The file %s has not been opened!\n",tmp);
        }
    }
    else printf("The file %s not exists!\n",tmp);
    close_file(tmp);
}

//�ļ��Ը��Ƿ�ʽд��
void write_file(char tmp[9]) // д�ļ�
{
	open_file(tmp);
    unsigned short flag,i,j,k,size=0,need_blocks,length;
    flag=find_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        if(search_file(dir[k].inode))
        {
            reload_inode_entry(dir[k].inode);//�ж��Ƿ���д��Ȩ�� 
            if(!(inode_area[0].i_mode&2)) // i_mode:111b:��,д,ִ��
            {
                printf("The file %s can not be writed!\n",tmp);
                return;
            }
            printf("(��'#'��ʾ����)\n"); 
            getchar();
            //fflush(stdin);�ᵼ�µ�һ��д����ȥ��Ϊ�� 
            while(1)
            {
                tempbuf[size]=getchar();
                if(tempbuf[size]=='#')
                {
                    tempbuf[size]='\0';
                    break;
                }
                if(size>=4095)
                {
                    printf("Sorry,the max size of a file is 4KB!\n");
                    break;
                }
                size++;
            }
            if(size>=4095)
            {
            	length=4096;
            }
            else
            {
            	length=strlen(tempbuf);
            }
            //������Ҫ�����ݿ���Ŀ
            need_blocks=length/512;
            if(length%512)
            {
            	need_blocks++;
            }
            if(need_blocks<9) // �ļ���� 8 �� blocks(512 bytes)
            {
                // �����ļ��������Ŀ
                //��Ϊ�Ը���д�ķ�ʽд��Ҫ���ж�ԭ�е����ݿ���Ŀ
                if(inode_area[0].i_blocks<=need_blocks)//���ԭ���ݿ�������д����٣����������� 
                {
                    while(inode_area[0].i_blocks<need_blocks)
                    {
                        inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
                        inode_area[0].i_blocks++;
                    }
                }
                else
                {
                    while(inode_area[0].i_blocks>need_blocks)//ԭ���ݿ�������д��Ķ࣬���ͷŶ���� 
                    {
                        remove_block(inode_area[0].i_block[inode_area[0].i_blocks - 1]);
                        inode_area[0].i_blocks--;
                    }
                }
                j=0;
                while(j<need_blocks)//����������д��ÿ�����ݿ� 
                {
                    if(j!=need_blocks-1)
                    {
                        reload_block(inode_area[0].i_block[j]);
                        //printf("���ݿ�д�룺%d",inode_area[0].i_block[j]);
                        memcpy(Buffer,tempbuf+j*BLOCK_SIZE,BLOCK_SIZE);
                        update_block(inode_area[0].i_block[j]);
                    }
                    else
                    {
                        reload_block(inode_area[0].i_block[j]);
                        //printf("���ݿ�д�룺%d",inode_area[0].i_block[j]);
                        memcpy(Buffer,tempbuf+j*BLOCK_SIZE,length-j*BLOCK_SIZE);
                        inode_area[0].i_size=length;
                        update_block(inode_area[0].i_block[j]);
                    }
                    j++;
                }
                char time[20];
			    get_time(time);
				strcpy(inode_area[0].i_mtime,time);
                update_inode_entry(dir[k].inode);
            }
            else
            {
            	printf("Sorry,the max size of a file is 4KB!\n");
            }
        }
        else
        {
        	printf("The file %s has not opened!\n",tmp);
        }
    }
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    }
    close_file(tmp);
}

//�鿴Ŀ¼�µ�����
void my_dir(void)
{
    printf("items          type           mode           size\n"); /* 15*4 */
    unsigned short i,j,k,flag;
    i=0;
    reload_inode_entry(current_dir);	//��ȡ��ǰĿ¼·�� 
    while(i<inode_area[0].i_blocks)     //��ǰĿ¼�´���ļ�/Ŀ¼���� 
    {
        k=0;
        reload_dir(inode_area[0].i_block[i]);//��ȡ��һ��inode�ڵ���Ϣ 
        while(k<32)
        {
            if(dir[k].inode)
            {
                printf("%s",dir[k].name);
                if(dir[k].file_type==2)		//����� 
                {
                    j=0;
                    reload_inode_entry(dir[k].inode);
                    if(!strcmp(dir[k].name,".."))
                    {
                    	while(j++<13)
                    	{
                    		printf(" ");
                    	}
                    	flag=1;
                    }
                    else if(!strcmp(dir[k].name,"."))
                    {
                    	while(j++<14)
                    	{
                    		printf(" ");
                    	}
                    	flag=0;
                    }
                    else
                    {
                    	while(j++<15-dir[k].name_len)
                    	{
                    		printf(" ");
                    	}
                    	flag=2;
                    }
                    printf("<DIR>          ");
                    switch(inode_area[0].i_mode&7)
                    {
                        case 1:printf("____x");break;
                        case 2:printf("__w__");break;
                        case 3:printf("__w_x");break;
                        case 4:printf("r____");break;
                        case 5:printf("r___x");break;
                        case 6:printf("r_w__");break;
                        case 7:printf("r_w_x");break;
                    }
                    if(flag!=2)
                    {
                    	printf("          ----");
                    }
                    else
                    {
                    	printf("          ");
                    	printf("%4ld bytes",inode_area[0].i_size);
                    }
                }
                else if(dir[k].file_type==1)
                {
                    j=0;
                    reload_inode_entry(dir[k].inode);
                    while(j++<15-dir[k].name_len)printf(" ");
                    printf("<FILE>         ");
                    switch(inode_area[0].i_mode&7)
                    {
                        case 1:printf("____x");break;
                        case 2:printf("__w__");break;
                        case 3:printf("__w_x");break;
                        case 4:printf("r____");break;
                        case 5:printf("r___x");break;
                        case 6:printf("r_w__");break;
                        case 7:printf("r_w_x");break;
                    }
                    printf("          ");
                    printf("%4ld bytes",inode_area[0].i_size);
                }
                printf("\n");
            }
            k++;
            reload_inode_entry(current_dir);
        }
        i++;
    }
}
int judge_su(char name[9],char tmp[9])
{
	reversestring(name);
	reversestring(tmp);
	//printf("name:%s\n",name);
	//printf("tmp:%s\n",tmp);
	int i,flag=0;	
	for(i=0;i<strlen(tmp)-1;i++)
	{
		if(name[i]!=tmp[i])
		{
			flag=1;
			break;
		}
	}
	reversestring(name);
	reversestring(tmp);
	if(flag==0)
	return 1;
	else
	return 0;
} 
//dir *.txt 
void my_dir_single(char tmp[9])
{
	printf("items          type           mode           size\n"); /* 15*4 */
    unsigned short i,j,k,flag;
    i=0;
    reload_inode_entry(current_dir);	//��ȡ��ǰĿ¼·�� 
    while(i<inode_area[0].i_blocks)     //��ǰĿ¼�´���ļ�/Ŀ¼���� 
    {
        k=0;
        reload_dir(inode_area[0].i_block[i]);//��ȡ��һ��inode�ڵ���Ϣ 
        while(k<32)
        {
            if(dir[k].inode&&dir[k].file_type==1)
            {
            	flag=judge_su(dir[k].name,tmp); 
            	if(flag)
            	{
            		printf("%s",dir[k].name);
            		j=0;
                    reload_inode_entry(dir[k].inode);
                    while(j++<15-dir[k].name_len)printf(" ");
                    printf("<FILE>         ");
                    switch(inode_area[0].i_mode&7)
                    {
                        case 1:printf("____x");break;
                        case 2:printf("__w__");break;
                        case 3:printf("__w_x");break;
                        case 4:printf("r____");break;
                        case 5:printf("r___x");break;
                        case 6:printf("r_w__");break;
                        case 7:printf("r_w_x");break;
                    }
                    printf("          ");
                    printf("%4ld bytes\n",inode_area[0].i_size);
				}
            }
            k++;
        }
        i++;
    }
}
// dir /s
void dir_s(unsigned short node_i)
{
	unsigned short i,j,k,flag;
    i=0;
    reload_inode_entry(node_i);	
	struct inode current_inode=inode_area[0];
    while(i<inode_area[0].i_blocks)     //��ǰĿ¼�´���ļ�/Ŀ¼���� 
    {
    	k=0;
        reload_dir(inode_area[0].i_block[i]);//��ȡ��һ��inode�ڵ���Ϣ 
        struct dir_file current_d[32];
        int num;
        for(num=0;num<32;num++)
        {
        	current_d[num]=dir[num];
		}
        while(k<32)
        {
        	if(current_d[k].inode)
        	{
            	if(current_d[k].file_type==2)		//�����Ŀ¼ 
                {
                    reload_inode_entry(current_d[k].inode);
                    if(strcmp(current_d[k].name,"..")&&strcmp(current_d[k].name,"."))
                    {
                    	printf("%s",current_d[k].name);
                    	j=0; 
                    	while(j++<15-current_d[k].name_len)printf(" ");
                    	printf("<DIR>          ");
	                    switch(inode_area[0].i_mode&7)
	                    {
	                        case 1:printf("____x");break;
	                        case 2:printf("__w__");break;
	                        case 3:printf("__w_x");break;
	                        case 4:printf("r____");break;
	                        case 5:printf("r___x");break;
	                        case 6:printf("r_w__");break;
	                        case 7:printf("r_w_x");break;
	                    }
	                    printf("          ");
	                    printf("%4ld bytes\n",inode_area[0].i_size);
	                    dir_s(current_d[k].inode);
                    }
                    //�����Ŀ¼֮�����Ŀ¼�е�����	
                }
                else if(current_d[k].file_type==1)
                {
                	printf("%s",current_d[k].name);
                    j=0;
                    reload_inode_entry(current_d[k].inode);
                    while(j++<15-current_d[k].name_len)printf(" ");
                    printf("<FILE>         ");
                    switch(inode_area[0].i_mode&7)
                    {
                        case 1:printf("____x");break;
                        case 2:printf("__w__");break;
                        case 3:printf("__w_x");break;
                        case 4:printf("r____");break;
                        case 5:printf("r___x");break;
                        case 6:printf("r_w__");break;
                        case 7:printf("r_w_x");break;
                    }
                    printf("          ");
                    printf("%4ld bytes\n",inode_area[0].i_size);
                }
            }
            k++;
            //reload_inode_entry(node_i);
        }
        i++; 
	}

}
void my_dir_s(void)
{
	printf("items          type           mode             size\n"); /* 15*4 */
	dir_s(current_dir);
} 
void my_time(void)
{
	char time[20];
    get_time(time);
    printf("%s\n",time);
}
void file_time(char tmp[9])
{
	unsigned short i,j,k; 
	if(find_file(tmp,1,&i,&j,&k)) 
	{
		reload_inode_entry(i); // �ҵ��ļ� inode
    	//printf("�ҵ��ˣ�%d\n",i);	
	}
	else if(find_file(tmp,2,&i,&j,&k))
	{
		reload_inode_entry(i);	//�ҵ�Ŀ¼inode 
	}
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    	return;
	}
    printf("%s\n",inode_area[0].i_mtime);//����ʱ�� 
}
void my_rename(char tmp[9],char new_name[9])
{
	unsigned short tmpno,i,j,k,type;
    reload_inode_entry(current_dir);//��ǰĿ¼��inode�� ����ǰĿ¼����Ϣ���ڻ����� 
    if((find_file(tmp,1,&i,&j,&k)&&find_file(new_name,1,&i,&j,&k))||(find_file(tmp,2,&i,&j,&k)&&find_file(new_name,2,&i,&j,&k)))
    {
    	printf("Directory/File has already existed!\n");
	}
    if(find_file(tmp,1,&i,&j,&k)||find_file(tmp,2,&i,&j,&k)) // �ҵ���ͬ���ļ� 
    {
    	//printf("name:%s",dir[k].name);
        dir[k].name_len=strlen(new_name);//���ֳ� 
        strcpy(dir[k].name,new_name);	// 
        update_dir(inode_area[0].i_block[j]);//���ĵ�dir��Ϣд������ 
    }
    else  // �Ѿ�����ͬ���ļ���Ŀ¼
    {
        printf("Directory/File does not existed!\n");
    }

} 
int judge(char tmp[9])
{
	int len=strlen(tmp);
	if(tmp[len-1]=='\\')
	return 1;
	else 
	return 0;
}
//void copy_f(char tmp[9],char path[9])//�����ļ� 
void copy_f(char tmp[9],unsigned short file_i,unsigned short dir_i)
{
//	1.�ҵ���ǰ�ļ���inode��
//	2.�ҵ��ļ������һ���ļ���inode��
//	3.�ж�size������0�������ݣ����½��ļ���д�ļ��ķ�ʽд��ȥ��
	unsigned short new_inode,flag;
    reload_inode_entry(file_i);
	struct inode file_inode=inode_area[0];//��file ��inode��Ϣ�ݴ� 
	//��Ҫ���Ƶ����ļ��е�inode���½��ļ�
	new_inode=cat(tmp,dir_i,1);//�����������inode�� 
//	printf("file:%s\n",file_inode.i_ctime);
//	printf("file:%d\n",file_inode.i_size); 
	// ������Ϣ 
	int t;
	if(file_inode.i_size)
	{
		reload_inode_entry(new_inode);//���ļ�inode���ػ����� 
		while(inode_area[0].i_blocks<file_inode.i_blocks)//�������ݿ� 
        {
            inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
            inode_area[0].i_blocks++;
        }
		for(flag=0;flag < file_inode.i_blocks;flag++)
        {
            reload_block(file_inode.i_block[flag]);//�����ݿ�����д��buffer 
            //��ʱÿ�����ݿ�����ݶ���buffer 
           //���Ƶ�Ŀ¼�£��������ݿ鲢д�룬�����������inode��Ϣ 
           	update_block(inode_area[0].i_block[flag]);
        }
        inode_area[0].i_flags = file_inode.i_flags ;
        inode_area[0].i_mode  = file_inode.i_mode ;
        inode_area[0].i_size = file_inode.i_size  ;
        char time[20];
		get_time(time);
		strcpy(inode_area[0].i_mtime,time);
		strcpy(inode_area[0].i_ctime,time);
        update_inode_entry(new_inode);
 	}      
}
void copy_d(unsigned short r_inode,unsigned short d_inode,char p_name[9])//����Ŀ¼ 
{
//�������ļ�����ͬһ��Ŀ¼ 
//	1.���tmp��inode��
//	2.���path��inode��
//	3.��path���½�һ��tmp,����tmp�����θ��ƣ�
//	������ļ�ֱ�Ӹ��ƣ�������ļ��о͵ݹ���ú���������������inode�š� 
	char name[9];
	unsigned short i,new_inode;
    reload_inode_entry(r_inode);	 //��ȡԴ�ļ���inode��Ϣ 
    i=0;
    while(i<inode_area[0].i_blocks)     //ԴĿ¼�´�����ݿ��� 
    {
        unsigned short k=0;
        reload_dir(inode_area[0].i_block[i]);//��ȡ��һ��inode�ڵ���Ϣ 
        while(k<32)
        {
            if(dir[k].inode)
            {
            	if((!strcmp(dir[k].name,"."))||(!strcmp(dir[k].name,"..")))
	        	{
	        		k++;
	        		continue; 	
				}
            	strcpy(name,dir[k].name);
                if(dir[k].file_type==2)	 //������ļ��У��½�Ŀ¼ 
                {
					new_inode=mkdir(name,p_name,d_inode,1);//ָ��inode�½�Ŀ¼
					copy_d(dir[k].inode,new_inode,name);
				}
			//��������ļ��У�ֱ�Ӹ����ļ� --Ҫ֪���ļ���ԭ�ļ���inode��Ŀ��Ŀ¼��inode 
				else if(dir[k].file_type == 1)
				{
					copy_f(name,dir[k].inode,d_inode);
				}
				else
				{
					printf("COPY ERROR! \n");
					return;
				}
			}
			k++;
		}
		i++; 
	} 
}

//����������'\'������Ŀ¼������������ͨ�ļ� 
void my_copy(char tmp[9],char path[9])//�ļ����Ƶ�Ŀ¼��---Ŀ¼���Ƶ�Ŀ¼�� 
{
	unsigned short i,j,k,flag,d_inode,r_inode,new_inode=100;
	//���ж��Ƿ���Ŀ¼	
	if(judge(tmp)==1)
	//printf("��Ŀ¼"); 
	{
		//�޸��ļ��� 
		tmp[strlen(tmp)-1]='\0';
		path[strlen(path)-1]='\0';
		flag=find_file(tmp,2,&i,&j,&k);
		if(flag)
		r_inode=i;
		else
		{
			printf("Directory does not existed!\n");
			return;
		}
		flag=find_file(path,2,&i,&j,&k);
		if(flag)
		d_inode=i;
		else
		{
			printf("Directory does not existed!\n");
			return;
		}
		new_inode=mkdir(tmp,path,d_inode,1);
		//printf("dir1:%d\n",new_inode);
		copy_d(r_inode,new_inode,path);
	 } 
	else
	{
		unsigned short tmpno,i,j,k,flag;
		unsigned short file_i,dir_i,new_inode;
	    flag=find_file(tmp,1,&i,&j,&k);//�� 
	    if(flag)
	    {
	        file_i=i;
	        //printf("file:%d\n",file_i);
		}
		path[strlen(path)-1]='\0';
	    flag=find_file(path,2,&i,&j,&k);//�� 
	    if(flag)
	    {
	    	dir_i=i;
	    	//printf("dir:%d\n",dir_i);
		}
		copy_f(tmp,file_i,dir_i);
	}
}
void my_move(char tmp[9],char path[9])
{
	int flag=judge(tmp);
	my_copy(tmp,path);//ϸ�ڣ��ַ����Ѿ��ı䣬��ߵ�\û���� 
	if(flag) 
	rd(tmp);		//ɾ��Ŀ¼ 
	else
	del(tmp,1,current_dir); //ɾ���ļ� 
}
void read_from_file(char tmp[9])
{
	unsigned short flag,i,j,k,t;
	//д���ݵĹ��� 
//	void *memcpy(void *destin, void *source, unsigned n);
//	��sourceָ��ĵ�ַΪ��㣬��������n���ֽ����ݣ�
//	���Ƶ���destinָ��ĵ�ַΪ�����ڴ��С�
    flag=find_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        reload_inode_entry(dir[k].inode);
        for(flag=0;flag<inode_area[0].i_blocks;flag++)
        {
        	if(flag!=inode_area[0].i_blocks-1)
        	{
        		reload_block(inode_area[0].i_block[flag]);//�����ݿ�����д��buffer 
            	memcpy(tempbuf+flag*BLOCK_SIZE,Buffer,BLOCK_SIZE);
			}
            else
            {
            	reload_block(inode_area[0].i_block[flag]);//�����ݿ�����д��buffer 
            	memcpy(tempbuf+flag*BLOCK_SIZE,Buffer,inode_area[0].i_size-flag*BLOCK_SIZE);
			}
        }
// 	  int p=0;
//    for(p=0;p<inode_area[0].i_size;p++)
//    printf("%c",tempbuf[p]);
//    printf("\n");
    }
    else 
	printf("The file %s not exists!\n",tmp);
}
void write_to_local(path)
{
	FILE *fp=NULL;
	fp = fopen(path,"w+");
	if (fp==NULL) // д�����ݵ��ļ�
	{
		printf("canot find the file!\n");
		//exit(0);
	}
	fprintf(fp,"%s",tempbuf);
	fclose(fp);
}
//��������̵�һ���ļ����������ش��� 
void  my_export(char tmp[9],char path[9])
{
//	1.��������̵��ļ������ļ����ݶ���������
//	2.�ѻ����������ݴ浽���ش��� 
	read_from_file(tmp);
	strcat(path,"//");
	strcat(path,tmp);
	//strcpy(path,"..//in.txt");
	write_to_local(path);
} 
void read_from_local(char path[9])
{
	//���ݶ���������
	FILE *fp;
	if ((fp = fopen(path,"r"))==NULL) // д�����ݵ��ļ�
	{
		printf("canot find the file!\n");
	}  
	int flag=0;
	while(fgets(Buffer,1024,fp)!=NULL)//���ж�ȡfp��ָ���ļ��е����ݵ�text��
	{
		memcpy(tempbuf+flag*1024,Buffer,1024);
		flag++;
	}
// 	int p=0;
//    for(p=0;tempbuf[p]!='\0';p++)
//    printf("%c",tempbuf[p]);
//    printf("\n");
	fclose(fp);
}

void get_name(char path[9],char tmp[9])
{
	reversestring(path);//�Ȱ��ַ�����ת 
	int i;
	for(i=0;i<strlen(path);i++)//�ҵ����һ��"/"��λ�� 
	{
		if(path[i]=='/')
		break;
	}
 	//��ǰ�漸λ���Ƶ�tmp
 	int j;
	for(j=0;j<i;j++)
	{
		tmp[j]=path[j];
	 } 
	 tmp[j]='\0';
	reversestring(tmp);				//�ٷ�ת���� 
	//printf("tmp:%s\n",tmp);
}
void write_buffer(char tmp[9])
{
	unsigned short flag,i,j,k,size=0,need_blocks,length;
    flag=find_file(tmp,1,&i,&j,&k);
    if(flag)
    {
            reload_inode_entry(dir[k].inode);
            length=strlen(tempbuf);
            //������Ҫ�����ݿ���Ŀ
            need_blocks=length/512;
            if(length%512)
            {
            	need_blocks++;
            }
            if(need_blocks<9) // �ļ���� 8 �� blocks(512 bytes)
            {
                // �����ļ��������Ŀ
                //��Ϊ�Ը���д�ķ�ʽд��Ҫ���ж�ԭ�е����ݿ���Ŀ
                if(inode_area[0].i_blocks<=need_blocks)//���ԭ���ݿ�������д����٣����������� 
                {
                    while(inode_area[0].i_blocks<need_blocks)
                    {
                        inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
                        inode_area[0].i_blocks++;
                    }
                }
                else
                {
                    while(inode_area[0].i_blocks>need_blocks)//ԭ���ݿ�������д��Ķ࣬���ͷŶ���� 
                    {
                        remove_block(inode_area[0].i_block[inode_area[0].i_blocks - 1]);
                        inode_area[0].i_blocks--;
                    }
                }
                j=0;
                while(j<need_blocks)//����������д��ÿ�����ݿ� 
                {
                    if(j!=need_blocks-1)
                    {
                        reload_block(inode_area[0].i_block[j]);
                        //printf("���ݿ�д�룺%d",inode_area[0].i_block[j]);
                        memcpy(Buffer,tempbuf+j*BLOCK_SIZE,BLOCK_SIZE);
                        update_block(inode_area[0].i_block[j]);
                    }
                    else
                    {
                        reload_block(inode_area[0].i_block[j]);
                        //printf("���ݿ�д�룺%d",inode_area[0].i_block[j]);
                        memcpy(Buffer,tempbuf+j*BLOCK_SIZE,length-j*BLOCK_SIZE);
                        inode_area[0].i_size=length;
                        update_block(inode_area[0].i_block[j]);
                    }
                    j++;
                }
                char time[20];
			    get_time(time);
				strcpy(inode_area[0].i_mtime,time);
                update_inode_entry(dir[k].inode);
            }
            else
            {
            	printf("Sorry,the max size of a file is 4KB!\n");
            }
    }
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    }
}
void write_to_file(char tmp[9])
{
	//�½��ļ�
	cat(tmp,current_dir,0);
	//д����
	write_buffer(tmp);
}
//�ѱ��ش��̵��ļ������������ ,·���а����ļ��� 
void my_import(char path[9])
{
	//strcpy(path,"..//in.txt");
	//�ӵ��ض��ļ� 
	read_from_local(path);
	char tmp[9];
	//�������·����ȡ�ļ��� 
	get_name(path,tmp);
	//printf("tmp:%s\n",tmp);
	//�ѵ��ص��ļ�д��myfile 
	write_to_file(tmp);
}
void ver()
{
	printf("Microsoft Windows [�汾 10.0.18363.1556]\n");  
 } 
 void my_ver(char tmp[9])
 {
 	//��ʾ����ʱ��
	unsigned short flag,i,j,k,t;
    if(find_file(tmp,1,&i,&j,&k)) 
	{
		reload_inode_entry(i); // �ҵ��ļ� inode
	}
	else if(find_file(tmp,2,&i,&j,&k))
	{
		reload_inode_entry(i);	//�ҵ�Ŀ¼inode 
	}
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    	return;
	}
    printf("����ʱ�䣺%s\n",inode_area[0].i_ctime);
    printf("�����ߣ�root\n");
	//printf("�ļ��У�%s\n",); 
  } 
void help(void)
{
	printf("dir            ��ʾһ��Ŀ¼�е��ļ�����Ŀ¼��\n");
	printf("               /s---��ʾ�����ļ�\n");
	printf("               *.*---��ʾָ����׺���ļ�\n");
	printf("md             ����һ��Ŀ¼��\n");
	printf("touch          �����ļ���\n");
	printf("del            ɾ���ļ���\n");
	printf("rd             ɾ��Ŀ¼��\n");
	printf("move           ���ļ����ļ����ƶ�����һ��Ŀ¼��\n");
	printf("               (�ļ��������ֺ�+'\\')\n");
	printf("copy           ���ļ����ļ��и��Ƶ���һ��Ŀ¼��\n");
	printf("               (�ļ��������ֺ�+'\\')\n");
	printf("rename         �������ļ���\n");
	printf("help           �ṩ Windows ����İ�����Ϣ��\n");
	printf("time           ��ʾʱ�䡣\n");
	printf("ver            ��ʾ�ļ��汾��\n");
	printf("more           �ļ���ʾ�����\n");
	printf("export         �ӱ��ش��̸������ݵ�����Ĵ���������\n");
	printf("import         ������Ĵ����������������ݵ����ش���\n");
	printf("exit           �˳� CMD.EXE ����(������ͳ���)��\n");
	printf("");
}

