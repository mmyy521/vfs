#include <stdio.h>		 
#include <string.h>
#include <time.h>
#include "main.h"

#define VOLUME_NAME	"EXT2FS"   // 文件系统名
#define BLOCK_SIZE 512	       // 块大小
#define DISK_SIZE	4612	   //磁盘总块数

#define DISK_START 0	       // 磁盘开始地址
#define SB_SIZE	32	           //超级块大小是32B

#define	GD_SIZE	32	           // 块组描述符大小是32B
#define GDT_START	(0+512)    // 块组描述符起始地址

#define BLOCK_BITMAP (512+512) // 块位图起始地址
#define INODE_BITMAP (1024+512)// inode 位图起始地址

#define INODE_TABLE (1536+512) // 索引节点表起始地址 4*512
#define INODE_SIZE 64	       // 每个inode的大小是64B
#define INODE_TABLE_COUNTS	4096 // inode entry 数

#define DATA_BLOCK (263680+512)	// 数据块起始地址 4*512+4096*64
#define DATA_BLOCK_COUNTS	4096	// 数据块数

#define BLOCKS_PER_GROUP	4612 // 每组中的块数

struct super_block // 32 B
{
	char sb_volume_name[16]; 			//文件系统名
	unsigned short sb_disk_size; 		//磁盘总大小
	unsigned short sb_blocks_per_group; //每组中的块数
	unsigned short sb_size_per_block;	//块大小
	char sb_pad[10];   					//补充 
};

struct group_desc // 32 B
{
    char bg_volume_name[16]; //文件系统名---16 
    unsigned short bg_block_bitmap; //块位图的起始块号---2 
    unsigned short bg_inode_bitmap; //inode位图的起始块号---2 
    unsigned short bg_inode_table; //inode表的起始块号---2 
    unsigned short bg_free_blocks_count; //空闲块的个数---2 
    unsigned short bg_free_inodes_count; //空闲inode的个数---2 
    unsigned short bg_used_dirs_count; //分配给目录总结点数---2 
    char bg_pad[4]; //补充 
};

struct inode // 64 B
{
    unsigned short i_mode;   //文件类型及访问权限---2
    unsigned short i_blocks; //文件所占的数据块个数(0~7), 最大为7块--2
    unsigned short i_flags;  //打开文件的方式---2
    unsigned long i_size;    // 文件或目录大小(单位 byte)---4
    char i_ctime[19];   //20创建时间----19
    char i_mtime[19];   //20修改时间----19
    unsigned short i_block[8]; //存储数据块号---16
};
struct dir_file //16B
{
    unsigned short inode; //索引节点号---2 
    unsigned short rec_len; //目录项长度---2 
    unsigned short name_len; //文件名长度---2 
    char file_type; //文件类型(1 普通文件 2 目录.. )---1 
    char name[9]; //文件名
};


static unsigned short last_alloc_inode; // 最近分配的inode号 
static unsigned short last_alloc_block; // 最近分配的数据块号 
static unsigned short current_dir;   // 当前目录的inode号 
char par_path[9];
static unsigned short current_dirlen; 

static short fopen_table[16]; 

static struct super_block sb_block[1];	// 超级块缓冲区
static struct group_desc gdt[1];	// 组描述符缓冲区
static struct inode inode_area[1];  // inode缓冲区
static unsigned char bitbuf[512]={0}; // 位图缓冲区
static unsigned char ibuf[512]={0};	//数据块位图缓冲区 
static struct dir_file dir[32];   //目录项缓冲区 32*16=512
static char Buffer[512];  // 针对数据块的缓冲区
static char tempbuf[4096];	// 文件写入缓冲区
static FILE *fp;	// 虚拟磁盘指针


char current_path[256];    // 当前路径名 
//在当前目录下查询文件 
static unsigned short find_file(char tmp[9],int file_type,unsigned short *inode_num,unsigned short *block_num,unsigned short *dir_num);//查找文件
//在指定目录下查询文件 
static unsigned short find_file_in(char tmp[9],int file_type,unsigned short node_i,unsigned short *inode_num,unsigned short *block_num,unsigned short *dir_num);//查找文件
//初始化目录或文件信息 
static void dir_file_prepare(unsigned short node_i,unsigned short node_i_len,unsigned short tmp,unsigned short len,int type);
static void remove_block(unsigned short del_num);//删除数据块
static void remove_inode(unsigned short del_num);//删除inode节点
static unsigned short search_file(unsigned short Ino);//在打开文件表中查找是否已打开文件

static void initialize_disk(void);//初始化磁盘
static void get_time(char my_time[19]); 
//-----------获取时间---------------- 
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
	//注意第一个下标从0开始
	size=count-1;
    if (size==0 || size==1)
		//地址不变
		b=b;
	for(;i<=size;i++)
	{ 
		// 字符串不可以用来等价 用一个字符替代
		c=b[i];
		b[i]=b[size];
		b[size]=c;
		size--;
	}
}
//-----------超级块读写-------------- 
static void update_super_block(void) //写超级块
{
    fp=fopen("./my_file","r+");
    fseek(fp,DISK_START,SEEK_SET);//磁盘开始地址0,
    //int fseek(FILE *stream, long offset, int fromwhere);
    //stream将指向以fromwhere为基准，偏移offset个字节的位置。
    //SEEK_SET： 文件开头
	//SEEK_CUR： 当前位置
	//SEEK_END： 文件结尾
	/*size_t fwrite(const void* buffer,size_t size, size_t count, FILE* stream);
	向文件中写入一个数据块
	1) buffer是一个指针。对fwrite来说是要写入数据的地址。
	2) size是要写入内容的单字节数。
	3) count是要进行写入size字节的数据项的个数。
	4) stream为目标文件指针
	5) 返回实际写入的数据项个数count。*/
    fwrite(sb_block,SB_SIZE,1,fp);
    fflush(fp); //立刻将缓冲区的内容输出，保证磁盘内存数据的一致性
    //fwrite[1]函数写到用户空间缓冲区，并未同步到文件中，
	//修改后要将内存与文件同步
}

static void reload_super_block(void) //读超级块
{
    fseek(fp,DISK_START,SEEK_SET);		//0,从头读 
    fread(sb_block,SB_SIZE,1,fp);//读取内容到超级块缓冲区中
}
//-----------组描述符读写-------------- 
static void update_group_desc(void) //写组描述符
{
    fp=fopen("./my_file","r+");
    fseek(fp,GDT_START,SEEK_SET);//组描述符起始地址512 ，从头开始读 
    fwrite(gdt,GD_SIZE,1,fp);	//块组描述符大小是32B
    fflush(fp);
}

static void reload_group_desc(void) // 读组描述符
{

    fseek(fp,GDT_START,SEEK_SET);
    fread(gdt,GD_SIZE,1,fp);
}
//-----------inode读写-------------- 
static void update_inode_entry(unsigned short i) // 写第i个inode
{
    fp=fopen("./my_file","r+");// 索引节点表起始地址 4*512
    fseek(fp,INODE_TABLE+(i-1)*INODE_SIZE,SEEK_SET);//偏移量是iNode表起始地址+i个iNode大小 64
    fwrite(inode_area,INODE_SIZE,1,fp);
    fflush(fp);
}

static void reload_inode_entry(unsigned short i) // 读第i个inode
{
    fseek(fp,INODE_TABLE+(i-1)*INODE_SIZE,SEEK_SET);
    fread(inode_area,INODE_SIZE,1,fp);
}
//-----------目录数据块读写-------------- 
static void update_dir(unsigned short i) //   写第i个 数据块
{
    fp=fopen("./my_file","r+");//数据块起始地址 4*512+4096*64 
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fwrite(dir,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_dir(unsigned short i) // 读第i个 数据块
{
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fread(dir,BLOCK_SIZE,1,fp);
    //fclose(fp);
}
//-----------black位图读写--------------
static void update_block_bitmap(void) //写block位图
{
    fp=fopen("./my_file","r+");
    fseek(fp,BLOCK_BITMAP,SEEK_SET);//块位图起始地址512+512 
    fwrite(bitbuf,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_block_bitmap(void) //读block位图
{
    fseek(fp,BLOCK_BITMAP,SEEK_SET);
    fread(bitbuf,BLOCK_SIZE,1,fp);
}
//-----------iNode位图读写--------------
static void update_inode_bitmap(void) //写inode位图
{
    fp=fopen("./my_file","r+");
    fseek(fp,INODE_BITMAP,SEEK_SET);// 1024+512 ：inode 位图起始地址
    fwrite(ibuf,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_inode_bitmap(void) // 读inode位图
{
    fseek(fp,INODE_BITMAP,SEEK_SET);
    fread(ibuf,BLOCK_SIZE,1,fp);
}
//-----------数据块读写--------------
static void update_block(unsigned short i) // 写第i个数据块
{
    fp=fopen("./my_file","r+");
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    //fseek(fp,0,SEEK_SET);
    fwrite(Buffer,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_block(unsigned short i) // 读第i个数据块
{
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fread(Buffer,BLOCK_SIZE,1,fp);
}

static int alloc_block(void) // 分配一个数据块,返回数据块号
{
	//分配的时候用最近分配的数据块号/8得到分配的字节号 
    //位图缓冲区 bitbuf共有512个字节，表示512乘8=4096个数据块。
	//根据last_alloc_block/8计算它在bitbuf的哪一个字节
    unsigned short cur=last_alloc_block;	// 最近分配的数据块号 
    //printf("cur: %d\n",cur);
    unsigned char con=128; // 1000 0000b
    int flag=0;
    if(gdt[0].bg_free_blocks_count==0)//没有可分配的块了 组描述符缓冲区空闲块的个数是0 
    {
        printf("There is no block to be alloced!\n");
        return(0);
    }
    reload_block_bitmap();		//获取位图缓冲区的数据 
    cur/=8;		//计算在哪一个字节   bit---字节 
    //从位图缓冲区bitbuf的第cur个字节开始判断 
    while(bitbuf[cur]==255)	//该字节的8个bit都已有数据   
    {			//2^8
        if(cur==511)
		cur=0; 					//最后一个字节也已经满，从头开始寻找
        else cur++;
    }
    while(bitbuf[cur]&con) 		//两数相与，均为1即为1，在一个字节中找具体的某一个bit
    {
        con=con/2;
        flag++;
    }
    bitbuf[cur]=bitbuf[cur]+con;    //bitbuf赋值时按照位占用情况赋值 ？ 
    last_alloc_block=cur*8+flag;	//更新最近分配的数据块号 

    update_block_bitmap();		//更新位图 
    gdt[0].bg_free_blocks_count--;	//更新缓冲区空闲块数 
    update_group_desc();		//更新组 
    return last_alloc_block;	//返回找到的数据块号 
}

static int get_inode(void) // 分配一个inode
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
    cur=(cur-1)/8;   //第一个标号是1，但是存储是从0开始的
    while(ibuf[cur]==255) //先看该字节的8个位是否已经填满
    {
        if(cur==511)	//最后一个字节也已经满，从头开始寻找
		cur=0;
        else cur++;
    }
    while(ibuf[cur]&con)  //再看某个字节的具体哪一位没有被占用
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

//当前目录中查找文件或目录为tmp，并得到该文件的 inode 号，
//和它在上级目录中的数据块号以及数据块中目录的项号
static unsigned short find_file(char tmp[9],int file_type,unsigned short *inode_num,
														unsigned short *block_num,unsigned short *dir_num)
{
    unsigned short j,k;
    reload_inode_entry(current_dir); //进入当前目录，当前目录的节点号  
    j=0;
    while(inode_area[0].i_blocks>j)//inode缓冲区，文件所占的数据块个数(0~7), 最大为7
    {
        reload_dir(inode_area[0].i_block[j]);///根据数据块号找到目录项返回位置 
        k=0;
        while(k<32)
        {	//这个节点不存在或者类型不一致或者不是此文件名 
            if(!dir[k].inode||dir[k].file_type!=file_type||strcmp(dir[k].name,tmp))
            {
            	k++;
            }
            else
            {
                *inode_num=dir[k].inode;		//找到返回 
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
    reload_inode_entry(node_i); //进入当前目录，当前目录的节点号  
    j=0;
    while(inode_area[0].i_blocks>j)//inode缓冲区，文件所占的数据块个数(0~7), 最大为7
    {
        reload_dir(inode_area[0].i_block[j]);///根据数据块号找到目录项返回位置 
        k=0;
        while(k<32)
        {	//这个节点不存在或者类型不一致或者不是此文件名 
            if(!dir[k].inode||dir[k].file_type!=file_type||strcmp(dir[k].name,tmp))
            {
            	k++;
            }
            else
            {
                *inode_num=dir[k].inode;		//找到返回 
                *block_num=j;
                *dir_num=k;
                return 1;
            }
        }
        j++;
    }
    return 0;
}
/*为新增目录或文件分配 dir_file
对于新增文件，只需分配一个inode号
对于新增目录，除了inode号外，还需要分配数据区存储.和..两个目录项*/
						//新增加的inode号 ，文件/目录的长度 ，类型 
static void dir_file_prepare(unsigned short node_i,unsigned short node_i_len,unsigned short tmp,unsigned short len,int type)
{//退不回上级目录 原因在于".." 存储错误，长度 
    reload_inode_entry(tmp);//tmp是iNode号
	char time[19];
	get_time(time);
	strcpy(inode_area[0].i_ctime,time);	//创建时间赋值 
	strcpy(inode_area[0].i_mtime,time); 
    if(type==2) // 目录
    {
        inode_area[0].i_size=32;  //大小32B ,存的".",".." 
        inode_area[0].i_blocks=1;
        inode_area[0].i_block[0]=alloc_block();//申请的数据块号 
        //目录项缓冲区 
        dir[0].inode=tmp;			//新增加的iNode号 
        dir[1].inode=node_i;	//上级目录的iNode号 
        dir[0].name_len=len;
        dir[1].name_len=node_i_len;
        dir[0].file_type=dir[1].file_type=2;

        for(type=2;type<32;type++)//把此目录项其他位置赋为0 
            dir[type].inode=0;
        strcpy(dir[0].name,".");
        strcpy(dir[1].name,"..");
        update_dir(inode_area[0].i_block[0]);//更新此inode第一个数据块的dir信息 

        inode_area[0].i_mode=01006;//说明是目录文件 
    }
    else
    {
        inode_area[0].i_size=0;
        inode_area[0].i_blocks=0;
        inode_area[0].i_mode=0407;//说明是普通文件 
    }
//  printf("\ndirprepare\nblocks.--%d\t",inode_area[0].i_blocks);// 
//	printf("ctime.--%s\t",inode_area[0].i_ctime);//
//	printf("mtime.--%s\t",inode_area[0].i_mtime); //
//	printf("flags.--%d\t",inode_area[0].i_flags);//
//	printf("mode.--%d\t",inode_area[0].i_mode); //
//	printf("size.--%d\t\n",inode_area[0].i_size);  //96
    update_inode_entry(tmp);
}

//删除一个块号

static void remove_block(unsigned short del_num)
{
    unsigned short tmp;
    tmp=del_num/8;
    reload_block_bitmap();
    switch(del_num%8) // 更新block位图 将具体的位置为0
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


//删除一个inode 号

static void remove_inode(unsigned short del_num)
{
    unsigned short tmp;
    tmp=(del_num-1)/8;
    reload_inode_bitmap();
    switch((del_num-1)%8)//更改block位图
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

//在打开文件表中查找是否已打开文件
static unsigned short search_file(unsigned short Inode)
{
    unsigned short fopen_table_point=0;//检查文件打开表中的inode号 
    while(fopen_table_point<16&&fopen_table[fopen_table_point++]!=Inode);
    //找到文件打开表中文件位置 
    if(fopen_table_point==16)
    {
    	return 0;
    }
    return 1;
}

void initialize_disk(void)  //初始化磁盘
{
    int i=0;
    last_alloc_inode=1;//inode从1开始 
    last_alloc_block=0;//数据块从0开始 
    for(i=0;i<16;i++)
    {
    	fopen_table[i]=0; //清空缓冲表
    }
    for(i=0;i<BLOCK_SIZE;i++)//块大小是512B 
    {
    	Buffer[i]=0; // 清空缓冲区把数据块全部填充0 
    }
    if(fp!=NULL)
    {
    	fclose(fp);
    }
    fp=fopen("./my_file","w+"); //此文件大小是4612*512=2361344B，用此文件来模拟文件系统
    fseek(fp,DISK_START,SEEK_SET);//将文件指针从0开始
    for(i=0;i<DISK_SIZE;i++)
    {
    	fwrite(Buffer,BLOCK_SIZE,1,fp); // 清空文件，即清空磁盘全部用0填充 Buffer为缓冲区起始地址，BLOCK_SIZE表示读取大小，1表示写入对象的个数*/
    }
    // 初始化超级块内容
    reload_super_block();
    strcpy(sb_block[0].sb_volume_name,VOLUME_NAME);
    sb_block[0].sb_disk_size=DISK_SIZE;  //磁盘总块数4612 
    sb_block[0].sb_blocks_per_group=BLOCKS_PER_GROUP; //每组数据块数4612,1组 
    sb_block[0].sb_size_per_block=BLOCK_SIZE;//512
    update_super_block();
    // 根目录的inode号为1
    reload_inode_entry(1);
	//根目录的数据块号为0 
    reload_dir(0);
    strcpy(current_path,"[root@mcf /");  // 修改路径名为根目录
    // 初始化组描述符内容
    reload_group_desc();

    gdt[0].bg_block_bitmap=BLOCK_BITMAP; //第一个块位图的起始地址
    gdt[0].bg_inode_bitmap=INODE_BITMAP; //第一个inode位图的起始地址
    gdt[0].bg_inode_table=INODE_TABLE;   //inode表的起始地址
    gdt[0].bg_free_blocks_count=DATA_BLOCK_COUNTS; //空闲数据块数
    gdt[0].bg_free_inodes_count=INODE_TABLE_COUNTS; //空闲inode数
    gdt[0].bg_used_dirs_count=0; // 初始分配给目录的节点数是0
    update_group_desc(); // 更新组描述符内容

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
    inode_area[0].i_block[0]=alloc_block(); //分配数据块号 
 	inode_area[0].i_blocks++;
    current_dir=get_inode();//分配一个inode号 
    update_inode_entry(current_dir);

    //初始化根目录的目录项
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
//检查磁盘状态
void check_disk(void)
{
	reload_super_block();
	printf("File established!\n"); 
	printf("disk size    : %d(blocks)\n", sb_block[0].sb_disk_size);
	printf("file size    : %d(kb)\n", sb_block[0].sb_disk_size*sb_block[0].sb_size_per_block/1024);
	printf("block size   : %d(kb)\n", sb_block[0].sb_size_per_block);
}
//初始化内存
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

//格式化
void format(void)
{
    initialize_disk();
    initialize_memory();
}
//进入某个目录，实际上是改变current_dir 
void cd(char tmp[9])
{
    unsigned short i,j,k,flag;
    flag=find_file(tmp,2,&i,&j,&k);//首先找到这个目录的inode号
							//和它在上级目录中的数据块号以及数据块中目录的项号
    if(flag)
    {
        current_dir=i;//重新赋值当前目录的inode号 
        //printf("current:%d\n",i);
        if(!strcmp(tmp,"..")&&dir[k-1].name_len) ///退到上一级目录
        {		//截去
//        printf("-------------------------\n");
//        printf("k=%d\n",k);//1 
//        printf("name[k-1]:%s\n",dir[k-1].name);	//. 
//        printf("name_len[k-1]:%d\n",dir[k-1].name_len); 
//        printf("name[k]:%s\n",dir[k].name);		//..
//        printf("name_len[k]:%d\n",dir[k].name_len);
//        printf("current:%s\n",current_path);
            current_path[strlen(current_path)-dir[k-1].name_len-1]='\0';
            current_dirlen=dir[k].name_len;//更新路径长度 
//        printf("current:%s\n",current_path);
//        printf("currentlen:%d\n",current_dirlen);
//        printf("-------------------------\n");
        }
        else if(!strcmp(tmp,"."))//表示停留在当前目录 
        {
        	return;
        }
        else if(strcmp(tmp,"..")) // cd 到子目录
        {
            current_dirlen=strlen(tmp);	//改变目录长度 
            strcpy(par_path,tmp);
            strcat(current_path,tmp);	//改变当前目录 
            strcat(current_path,"/");	//加上/ 
        }
    }
    else
    {
    	printf("The directory %s not exists!\n",tmp);
    }
}

// 在当前目录下创建目录或者在指定目录下创建目录
unsigned short mkdir(char tmp[9],char parent_path[9],unsigned short node_i,int f)
{
    unsigned short tmpno,i,j,k,flag,type=2;
    // 当前目录下新增目录或文件
    reload_inode_entry(node_i);			//将指定目录信息加载至缓冲区 
    if(f==1)		//是指定inode
	flag=find_file_in(tmp,type,node_i,&i,&j,&k);
	else			//是当前目录下 
	flag=find_file(tmp,type,&i,&j,&k);
    if(!flag) 	//如果没有重名目录 
    {
        if(inode_area[0].i_size==4096) // 目录项已满
        {
            printf("Directory has no room to be alloced!\n");
            return;
        }
        flag=1;
        if(inode_area[0].i_size!=inode_area[0].i_blocks*512) // 目录中有某些块中32个 dir_file 未满
        {
            i=0;
            while(flag&&i<inode_area[0].i_blocks)
            {
                reload_dir(inode_area[0].i_block[i]);//把数据块读到缓冲区 
                j=0;
                while(j<32)
                {
                    if(dir[j].inode==0)
                    {
                        flag=0; //找到某个未装满的数据块
                        break;
                    }
                    j++;
                }
                i++;
            }
            tmpno=dir[j].inode=get_inode();//为新增目录分配inode块 
            dir[j].name_len=strlen(tmp);//目录名字长 
            dir[j].file_type=type;		//类型目录2	 
            strcpy(dir[j].name,tmp);	//目录名字 
            update_dir(inode_area[0].i_block[i-1]);//更改的dir信息写进磁盘 
        }
        else // 全满新增加块
        {
            inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
            inode_area[0].i_blocks++;
            reload_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
            tmpno=dir[0].inode=get_inode();
            dir[0].name_len=strlen(tmp);
            dir[0].file_type=type;
            strcpy(dir[0].name,tmp);
            // 初始化新块的其余目录项
            for(flag=1;flag<32;flag++)
            {
            	dir[flag].inode=0;
            }
            update_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
        }
        inode_area[0].i_size+=16;//新增加的一个dir_file大小事16B 

        update_inode_entry(node_i);//更新当前目录的inode 

        // 更新增加的目录inode信息，iNode号，目录名长度，类型2 
        dir_file_prepare(node_i,strlen(parent_path),tmpno,strlen(tmp),type); 
        return tmpno;
    }
    else  // 已经存在同名文件或目录
    {
        printf("Directory has already existed!\n");
    }

}
void md(char tmp[9])
{		//父目录的名字 
	mkdir(tmp,par_path,current_dir,0);
}
//创建文件
unsigned short cat(char tmp[9],unsigned short node_i,int f)
{
	unsigned short type=1;//文件类型 
    unsigned short tmpno,i,j,k,flag;
    reload_inode_entry(node_i);			//将指定目录信息加载至缓冲区 
    if(f==1)		//是指定inode
	flag=find_file_in(tmp,type,node_i,&i,&j,&k);
	else			//是当前目录下 
	flag=find_file(tmp,type,&i,&j,&k);
    if(!flag) 	//如果没有重名文件 
    {
        if(inode_area[0].i_size==4096)
        {
            printf("Directory has no room to be alloced!\n");
            return;
        }
        flag=1;
        if(inode_area[0].i_size!=inode_area[0].i_blocks*512)
        {//当前目录总大小比所占全部块大小小，就从中找到没有分匹配的地方 
            i=0;//i是块号 
            while(flag&&i<inode_area[0].i_blocks)
            {
                reload_dir(inode_area[0].i_block[i]);
                j=0;			//j是dir号 
                while(j<32)
                {
                    if(dir[j].inode==0)//找到了未分配的目录项
                    {
                        flag=0;
                        break;
                    }
                    j++;
                }
                i++;
            }
            tmpno=dir[j].inode=get_inode();//分配一个新的inode项
            dir[j].name_len=strlen(tmp);//赋值名字长度 
            dir[j].file_type=type;
            strcpy(dir[j].name,tmp);
            update_dir(inode_area[0].i_block[i-1]);//更新数据块内容 
        }
        else //分配一个新的数据块
        {
            inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
            inode_area[0].i_blocks++;
            reload_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
            tmpno=dir[0].inode=get_inode();//这时候才分配到新的inode号 
            dir[0].name_len=strlen(tmp);
            dir[0].file_type=type;
            strcpy(dir[0].name,tmp);
            //初始化新快其他项目为0
            for(flag=1;flag<32;flag++)
            {
                dir[flag].inode=0;
            }
            update_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
        }
        inode_area[0].i_size+=16;
        char time[20];
	    get_time(time);
	    strcpy(inode_area[0].i_mtime,time);	//对于这个inode是更新最近时间 
        update_inode_entry(node_i);	//更新当前inode信息 
        //将新增文件的inode节点初始化
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
//删目录
int  rmdir(char tmp[9],unsigned short node_i)
{
    unsigned short i,j,k,flag;
    unsigned short m,n,par_inode;
    int tflag=0;
    flag=find_file_in(tmp,2,node_i,&i,&j,&k);
    //printf("i:%d,,dir[k].inode:%d\n",i,dir[k].inode);相等 
    par_inode=dir[k].inode;
    if(flag)
    {
        reload_inode_entry(dir[k].inode); // 加载要删除的节点
        if(inode_area[0].i_size==32) 		//本身就是一个空目录 
        {
        	tflag=1;
            inode_area[0].i_size=0;
            inode_area[0].i_blocks=0;
			
            remove_block(inode_area[0].i_block[0]);
            // 更新 tmp 所在父目录
            reload_inode_entry(node_i);
            reload_dir(inode_area[0].i_block[j]);
            remove_inode(dir[k].inode);
            dir[k].inode=0;
            update_dir(inode_area[0].i_block[j]);
            inode_area[0].i_size-=16;
            //父目录更新结束 
            flag=0;
            /*删除32 个 dir_file 全为空的数据块
            由于 inode_area[0].i_block[0] 中有目录 . 和 ..
            所以这个数据块的非空 dir_file 不可能为0*/

            //printf("rm: %d\n",inode_area[0]);
            m=1;
            while(flag<32&&m<inode_area[0].i_blocks)
            {
                flag=n=0;
                reload_dir(inode_area[0].i_block[m]);
                while(n<32)
                {
                    if(!dir[n].inode)//如果是0，无数据dir++ 
                    {
                    	flag++;
                    }
                    n++;
                }
                //如果删除过后，整个数据块的目录项全都为空。类似于在数组中删除某一个位置
                if(flag==32)
                {
                    remove_block(inode_area[0].i_block[m]);
                    inode_area[0].i_blocks--;
                    while(m<inode_area[0].i_blocks)//补齐空位 
                    {
                    	inode_area[0].i_block[m]=inode_area[0].i_block[m+1];
                    	++m;
                    }
                }
            }
            update_inode_entry(node_i);//更新当前目录信息 
            return;
        }
        else//不是空目录，递归删除 
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
                    if(dir[a].file_type==2)//是目录 
                    {
                        //printf("%s\n",dir[a].name);
                        rmdir(dir[a].name,par_inode);
                    }//参数是待删除文件夹名字和所在上一级文件夹inode 
                    else if(dir[a].file_type==1)//是文件 
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
            // 更新 tmp 所在父目录
            reload_inode_entry(node_i);
            reload_dir(inode_area[0].i_block[j]);
            remove_inode(dir[k].inode);
            dir[k].inode=0;
            update_dir(inode_area[0].i_block[j]);
            inode_area[0].i_size-=16;
            //父目录更新结束 
            flag=0;
            /*删除32 个 dir_file 全为空的数据块
            由于 inode_area[0].i_block[0] 中有目录 . 和 ..
            所以这个数据块的非空 dir_file 不可能为0*/

            //printf("rm: %d\n",inode_area[0]);
            m=1;
            while(flag<32&&m<inode_area[0].i_blocks)
            {
                flag=n=0;
                reload_dir(inode_area[0].i_block[m]);
                while(n<32)
                {
                    if(!dir[n].inode)//如果是0，无数据dir++ 
                    {
                    	flag++;
                    }
                    n++;
                }
                //如果删除过后，整个数据块的目录项全都为空。类似于在数组中删除某一个位置
                if(flag==32)
                {
                    remove_block(inode_area[0].i_block[m]);
                    inode_area[0].i_blocks--;
                    while(m<inode_area[0].i_blocks)//补齐空位 
                    {
                    	inode_area[0].i_block[m]=inode_area[0].i_block[m+1];
                    	++m;
                    }
                }
            }
            update_inode_entry(node_i);//更新当前目录信息 
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
//删除文件
//参数是待删除文件名和所属文件夹inode 
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
        reload_inode_entry(i); // 加载删除文件 inode
        //删除文件对应的数据块
        while(m<inode_area[0].i_blocks)
        {
        	remove_block(inode_area[0].i_block[m++]);//--->copy_block()
        }
        inode_area[0].i_blocks=0;
        inode_area[0].i_size=0;
        remove_inode(i);
        // 更新父目录
        reload_inode_entry(node_i);//更新删除文件的inode块的信息 
        reload_dir(inode_area[0].i_block[j]);//把当前目录的inode中第j block数据块更新 
        dir[k].inode=0; 				//删除inode节点

        update_dir(inode_area[0].i_block[j]);
        inode_area[0].i_size-=16;//因为存文件名信息是16B的dir 
        m=1;
        //删除一项后整个数据块为空，则将该数据块删除
        while(m<inode_area[i].i_blocks)
        {
            flag=n=0;
            reload_dir(inode_area[0].i_block[m]);
            while(n<32)
            {
                if(!dir[n].inode)//如果是空的dir项 
                {
                	flag++;
                }
                n++;
            }
            if(flag==32)			//删除这一个数据块 
            {
                remove_block(inode_area[i].i_block[m]);	//清除数据块 
                inode_area[i].i_blocks--;
                while(m<inode_area[i].i_blocks)		//把空位补上 
                {
                	inode_area[i].i_block[m]=inode_area[i].i_block[m+1];
                	++m;
                }
            }
        }
        update_inode_entry(node_i);//更新当前目录inode块 
    }
    else
    {
    	printf("The file %s not exists!\n",tmp);
    }
}


//打开文件
void open_file(char tmp[9])
{
    unsigned short flag,i,j,k;
    flag=find_file(tmp,1,&i,&j,&k);//找到文件的位置 
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

//关闭文件
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

// 读文件
void more(char tmp[9])
{
	open_file(tmp);
    unsigned short flag,i,j,k,t;
    flag=find_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        if(search_file(dir[k].inode)) //读文件的前提是该文件已经打开
        {
            reload_inode_entry(dir[k].inode);
            //判断是否有读的权限
            if(!(inode_area[0].i_mode&4)) // i_mode:111b:读,写,执行
            {
                printf("The file %s can not be read!\n",tmp);
                return;
            }
            for(flag=0;flag<inode_area[0].i_blocks;flag++)
            {
                reload_block(inode_area[0].i_block[flag]);//将数据块数据写进buffer 
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

//文件以覆盖方式写入
void write_file(char tmp[9]) // 写文件
{
	open_file(tmp);
    unsigned short flag,i,j,k,size=0,need_blocks,length;
    flag=find_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        if(search_file(dir[k].inode))
        {
            reload_inode_entry(dir[k].inode);//判断是否有写的权限 
            if(!(inode_area[0].i_mode&2)) // i_mode:111b:读,写,执行
            {
                printf("The file %s can not be writed!\n",tmp);
                return;
            }
            printf("(以'#'表示结束)\n"); 
            getchar();
            //fflush(stdin);会导致第一行写不进去，为甚 
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
            //计算需要的数据块数目
            need_blocks=length/512;
            if(length%512)
            {
            	need_blocks++;
            }
            if(need_blocks<9) // 文件最大 8 个 blocks(512 bytes)
            {
                // 分配文件所需块数目
                //因为以覆盖写的方式写，要先判断原有的数据块数目
                if(inode_area[0].i_blocks<=need_blocks)//如果原数据块数比新写入的少，就重新申请 
                {
                    while(inode_area[0].i_blocks<need_blocks)
                    {
                        inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
                        inode_area[0].i_blocks++;
                    }
                }
                else
                {
                    while(inode_area[0].i_blocks>need_blocks)//原数据块数比新写入的多，就释放多余的 
                    {
                        remove_block(inode_area[0].i_block[inode_area[0].i_blocks - 1]);
                        inode_area[0].i_blocks--;
                    }
                }
                j=0;
                while(j<need_blocks)//把数据依次写入每个数据块 
                {
                    if(j!=need_blocks-1)
                    {
                        reload_block(inode_area[0].i_block[j]);
                        //printf("数据块写入：%d",inode_area[0].i_block[j]);
                        memcpy(Buffer,tempbuf+j*BLOCK_SIZE,BLOCK_SIZE);
                        update_block(inode_area[0].i_block[j]);
                    }
                    else
                    {
                        reload_block(inode_area[0].i_block[j]);
                        //printf("数据块写入：%d",inode_area[0].i_block[j]);
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

//查看目录下的内容
void my_dir(void)
{
    printf("items          type           mode           size\n"); /* 15*4 */
    unsigned short i,j,k,flag;
    i=0;
    reload_inode_entry(current_dir);	//获取当前目录路径 
    while(i<inode_area[0].i_blocks)     //当前目录下存的文件/目录项数 
    {
        k=0;
        reload_dir(inode_area[0].i_block[i]);//获取这一块inode节点信息 
        while(k<32)
        {
            if(dir[k].inode)
            {
                printf("%s",dir[k].name);
                if(dir[k].file_type==2)		//如果是 
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
    reload_inode_entry(current_dir);	//获取当前目录路径 
    while(i<inode_area[0].i_blocks)     //当前目录下存的文件/目录项数 
    {
        k=0;
        reload_dir(inode_area[0].i_block[i]);//获取这一块inode节点信息 
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
    while(i<inode_area[0].i_blocks)     //当前目录下存的文件/目录项数 
    {
    	k=0;
        reload_dir(inode_area[0].i_block[i]);//获取这一块inode节点信息 
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
            	if(current_d[k].file_type==2)		//如果是目录 
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
                    //输出完目录之后输出目录中的内容	
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
		reload_inode_entry(i); // 找到文件 inode
    	//printf("找到了！%d\n",i);	
	}
	else if(find_file(tmp,2,&i,&j,&k))
	{
		reload_inode_entry(i);	//找到目录inode 
	}
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    	return;
	}
    printf("%s\n",inode_area[0].i_mtime);//更新时间 
}
void my_rename(char tmp[9],char new_name[9])
{
	unsigned short tmpno,i,j,k,type;
    reload_inode_entry(current_dir);//当前目录的inode号 ，当前目录的信息存于缓冲区 
    if((find_file(tmp,1,&i,&j,&k)&&find_file(new_name,1,&i,&j,&k))||(find_file(tmp,2,&i,&j,&k)&&find_file(new_name,2,&i,&j,&k)))
    {
    	printf("Directory/File has already existed!\n");
	}
    if(find_file(tmp,1,&i,&j,&k)||find_file(tmp,2,&i,&j,&k)) // 找到了同名文件 
    {
    	//printf("name:%s",dir[k].name);
        dir[k].name_len=strlen(new_name);//名字长 
        strcpy(dir[k].name,new_name);	// 
        update_dir(inode_area[0].i_block[j]);//更改的dir信息写进磁盘 
    }
    else  // 已经存在同名文件或目录
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
//void copy_f(char tmp[9],char path[9])//复制文件 
void copy_f(char tmp[9],unsigned short file_i,unsigned short dir_i)
{
//	1.找到当前文件的inode号
//	2.找到文件夹最后一个文件的inode号
//	3.判断size，大于0复制内容，以新建文件，写文件的方式写进去。
	unsigned short new_inode,flag;
    reload_inode_entry(file_i);
	struct inode file_inode=inode_area[0];//把file 的inode信息暂存 
	//在要复制到的文件夹的inode中新建文件
	new_inode=cat(tmp,dir_i,1);//返回新申请的inode号 
//	printf("file:%s\n",file_inode.i_ctime);
//	printf("file:%d\n",file_inode.i_size); 
	// 复制信息 
	int t;
	if(file_inode.i_size)
	{
		reload_inode_entry(new_inode);//新文件inode加载缓冲区 
		while(inode_area[0].i_blocks<file_inode.i_blocks)//申请数据块 
        {
            inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
            inode_area[0].i_blocks++;
        }
		for(flag=0;flag < file_inode.i_blocks;flag++)
        {
            reload_block(file_inode.i_block[flag]);//将数据块数据写进buffer 
            //这时每个数据块的数据都在buffer 
           //复制的目录下，申请数据块并写入，更改新申请的inode信息 
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
void copy_d(unsigned short r_inode,unsigned short d_inode,char p_name[9])//复制目录 
{
//当两个文件夹在同一根目录 
//	1.获得tmp的inode号
//	2.获得path的inode号
//	3.在path中新建一个tmp,进入tmp中依次复制，
//	如果是文件直接复制，如果是文件夹就递归调用函数，参数传两个inode号。 
	char name[9];
	unsigned short i,new_inode;
    reload_inode_entry(r_inode);	 //获取源文件夹inode信息 
    i=0;
    while(i<inode_area[0].i_blocks)     //源目录下存的数据块数 
    {
        unsigned short k=0;
        reload_dir(inode_area[0].i_block[i]);//获取这一块inode节点信息 
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
                if(dir[k].file_type==2)	 //如果是文件夹，新建目录 
                {
					new_inode=mkdir(name,p_name,d_inode,1);//指定inode下建目录
					copy_d(dir[k].inode,new_inode,name);
				}
			//如果不是文件夹，直接复制文件 --要知道文件名原文件的inode和目的目录的inode 
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

//名字中最后带'\'代表是目录，不带就是普通文件 
void my_copy(char tmp[9],char path[9])//文件复制到目录下---目录复制到目录下 
{
	unsigned short i,j,k,flag,d_inode,r_inode,new_inode=100;
	//先判断是否是目录	
	if(judge(tmp)==1)
	//printf("是目录"); 
	{
		//修改文件名 
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
	    flag=find_file(tmp,1,&i,&j,&k);//① 
	    if(flag)
	    {
	        file_i=i;
	        //printf("file:%d\n",file_i);
		}
		path[strlen(path)-1]='\0';
	    flag=find_file(path,2,&i,&j,&k);//② 
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
	my_copy(tmp,path);//细节：字符串已经改变，后边的\没有了 
	if(flag) 
	rd(tmp);		//删除目录 
	else
	del(tmp,1,current_dir); //删除文件 
}
void read_from_file(char tmp[9])
{
	unsigned short flag,i,j,k,t;
	//写数据的过程 
//	void *memcpy(void *destin, void *source, unsigned n);
//	以source指向的地址为起点，将连续的n个字节数据，
//	复制到以destin指向的地址为起点的内存中。
    flag=find_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        reload_inode_entry(dir[k].inode);
        for(flag=0;flag<inode_area[0].i_blocks;flag++)
        {
        	if(flag!=inode_area[0].i_blocks-1)
        	{
        		reload_block(inode_area[0].i_block[flag]);//将数据块数据写进buffer 
            	memcpy(tempbuf+flag*BLOCK_SIZE,Buffer,BLOCK_SIZE);
			}
            else
            {
            	reload_block(inode_area[0].i_block[flag]);//将数据块数据写进buffer 
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
	if (fp==NULL) // 写入数据的文件
	{
		printf("canot find the file!\n");
		//exit(0);
	}
	fprintf(fp,"%s",tempbuf);
	fclose(fp);
}
//把虚拟磁盘的一个文件导出到本地磁盘 
void  my_export(char tmp[9],char path[9])
{
//	1.把虚拟磁盘的文件名和文件内容读到缓冲区
//	2.把缓冲区的内容存到本地磁盘 
	read_from_file(tmp);
	strcat(path,"//");
	strcat(path,tmp);
	//strcpy(path,"..//in.txt");
	write_to_local(path);
} 
void read_from_local(char path[9])
{
	//数据读到缓冲区
	FILE *fp;
	if ((fp = fopen(path,"r"))==NULL) // 写入数据的文件
	{
		printf("canot find the file!\n");
	}  
	int flag=0;
	while(fgets(Buffer,1024,fp)!=NULL)//逐行读取fp所指向文件中的内容到text中
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
	reversestring(path);//先把字符串反转 
	int i;
	for(i=0;i<strlen(path);i++)//找到最后一个"/"的位置 
	{
		if(path[i]=='/')
		break;
	}
 	//把前面几位复制到tmp
 	int j;
	for(j=0;j<i;j++)
	{
		tmp[j]=path[j];
	 } 
	 tmp[j]='\0';
	reversestring(tmp);				//再翻转回来 
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
            //计算需要的数据块数目
            need_blocks=length/512;
            if(length%512)
            {
            	need_blocks++;
            }
            if(need_blocks<9) // 文件最大 8 个 blocks(512 bytes)
            {
                // 分配文件所需块数目
                //因为以覆盖写的方式写，要先判断原有的数据块数目
                if(inode_area[0].i_blocks<=need_blocks)//如果原数据块数比新写入的少，就重新申请 
                {
                    while(inode_area[0].i_blocks<need_blocks)
                    {
                        inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
                        inode_area[0].i_blocks++;
                    }
                }
                else
                {
                    while(inode_area[0].i_blocks>need_blocks)//原数据块数比新写入的多，就释放多余的 
                    {
                        remove_block(inode_area[0].i_block[inode_area[0].i_blocks - 1]);
                        inode_area[0].i_blocks--;
                    }
                }
                j=0;
                while(j<need_blocks)//把数据依次写入每个数据块 
                {
                    if(j!=need_blocks-1)
                    {
                        reload_block(inode_area[0].i_block[j]);
                        //printf("数据块写入：%d",inode_area[0].i_block[j]);
                        memcpy(Buffer,tempbuf+j*BLOCK_SIZE,BLOCK_SIZE);
                        update_block(inode_area[0].i_block[j]);
                    }
                    else
                    {
                        reload_block(inode_area[0].i_block[j]);
                        //printf("数据块写入：%d",inode_area[0].i_block[j]);
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
	//新建文件
	cat(tmp,current_dir,0);
	//写内容
	write_buffer(tmp);
}
//把本地磁盘的文件导入虚拟磁盘 ,路径中包含文件名 
void my_import(char path[9])
{
	//strcpy(path,"..//in.txt");
	//从当地读文件 
	read_from_local(path);
	char tmp[9];
	//从输入的路径提取文件名 
	get_name(path,tmp);
	//printf("tmp:%s\n",tmp);
	//把当地的文件写进myfile 
	write_to_file(tmp);
}
void ver()
{
	printf("Microsoft Windows [版本 10.0.18363.1556]\n");  
 } 
 void my_ver(char tmp[9])
 {
 	//显示创建时间
	unsigned short flag,i,j,k,t;
    if(find_file(tmp,1,&i,&j,&k)) 
	{
		reload_inode_entry(i); // 找到文件 inode
	}
	else if(find_file(tmp,2,&i,&j,&k))
	{
		reload_inode_entry(i);	//找到目录inode 
	}
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    	return;
	}
    printf("创建时间：%s\n",inode_area[0].i_ctime);
    printf("创建者：root\n");
	//printf("文件夹：%s\n",); 
  } 
void help(void)
{
	printf("dir            显示一个目录中的文件和子目录。\n");
	printf("               /s---显示所有文件\n");
	printf("               *.*---显示指定后缀的文件\n");
	printf("md             创建一个目录。\n");
	printf("touch          创建文件。\n");
	printf("del            删除文件。\n");
	printf("rd             删除目录。\n");
	printf("move           将文件或文件夹移动到另一个目录。\n");
	printf("               (文件夹在名字后+'\\')\n");
	printf("copy           将文件或文件夹复制到另一个目录。\n");
	printf("               (文件夹在名字后+'\\')\n");
	printf("rename         重命名文件。\n");
	printf("help           提供 Windows 命令的帮助信息。\n");
	printf("time           显示时间。\n");
	printf("ver            显示文件版本。\n");
	printf("more           文件显示输出。\n");
	printf("export         从本地磁盘复制内容到虚拟的磁盘驱动器\n");
	printf("import         从虚拟的磁盘驱动器复制内容到本地磁盘\n");
	printf("exit           退出 CMD.EXE 程序(命令解释程序)。\n");
	printf("");
}

