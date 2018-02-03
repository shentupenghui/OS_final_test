//
//  main.cpp
//  OS_final_test
//
//  Created by 申屠鹏会 on 2018/1/11.
//  Copyright © 2018年 申屠鹏会. All rights reserved.
//

#include <stdio.h>//c语言的的unix文件输入输出库
#include <fcntl.h>//根据文件描述词操作文件的特性
#include <string>
#include <iostream>
#include<termios.h>
#include<unistd.h>
#include<assert.h>
#include<time.h>
using namespace std;

//自定义不回显字符
int getch()
{
    int c = 0;
    struct termios org_opts, new_opts;
    int res = 0;
    res = tcgetattr(STDIN_FILENO, &org_opts);
    assert(res == 0);
    memcpy(&new_opts, &org_opts, sizeof(new_opts));
    new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
    c = getchar();
    res = tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);
    assert(res == 0);
    if(c == '\n') c = '\r';
    else if(c == 127) c = '\b';
    return c;
}


//以下定义常量

//文件可使用的最大数量节点
const unsigned int NADDR = 6;

//一个block的大小
const unsigned short BLOCK_SIZE = 512;

//文件最大尺寸
const unsigned int FILE_SIZE_MAX = (NADDR - 2) * BLOCK_SIZE + BLOCK_SIZE / sizeof(int) * BLOCK_SIZE;

//block的数量
const unsigned short BLOCK_NUM = 512;

//inode大小
const unsigned short INODE_SIZE = 128;

//inode数量
const unsigned short INODE_NUM = 256;

//inode的起始位置
const unsigned int INODE_START = 3 * BLOCK_SIZE;

//数据起始位置
const unsigned int DATA_START = INODE_START + INODE_NUM * INODE_SIZE;

//文件系统支持的用户数量
const unsigned int ACCOUNT_NUM = 10;

//子目录的最大数量
const unsigned int DIRECTORY_NUM = 16;

//文件名的最大长度
const unsigned short FILE_NAME_LENGTH = 14;

//用户名的最大长度
const unsigned short USER_NAME_LENGTH = 14;

//用户密码的最大长度
const unsigned short USER_PASSWORD_LENGTH = 14;

//文件最大权限
const unsigned short MAX_PERMISSION = 511;

//用户最大权限
const unsigned short MAX_OWNER_PERMISSION = 448;

//权限
const unsigned short ELSE_E = 1;
const unsigned short ELSE_W = 1 << 1;
const unsigned short ELSE_R = 1 << 2;
const unsigned short GRP_E = 1 << 3;
const unsigned short GRP_W = 1 << 4;
const unsigned short GRP_R = 1 << 5;
const unsigned short OWN_E = 1 << 6;
const unsigned short OWN_W = 1 << 7;
const unsigned short OWN_R = 1 << 8;

//inode设计
struct inode
{
    unsigned int i_ino;            //inode号.
    unsigned int di_addr[NADDR];   //Number of data blocks where the file stored.
    unsigned short di_number;    //Number of associated files.
    unsigned short di_mode;        //文件类型.
    unsigned short icount;        //连接数
    unsigned short permission;    //文件权限
    unsigned short di_uid;        //文件所属用户id.
    unsigned short di_grp;        //文件所属组
    unsigned short di_size;        //文件大小.
    char time[83];
};

//超级块设置
struct filsys
{
    unsigned short s_num_inode;            //inode总数
    unsigned short s_num_finode;           //空闲inode数.
    unsigned short s_size_inode;           //inode大小.
    unsigned short s_num_block;            //block的数量.
    unsigned short s_num_fblock;           //空闲块的数量.
    unsigned short s_size_block;           //block的大小.
    unsigned int special_stack[50];
    int special_free;
};

//目录设计
struct directory
{
    char fileName[20][FILE_NAME_LENGTH];    //目录名称
    unsigned int inodeID[DIRECTORY_NUM];    //inode号
};

//账户设计
struct userPsw
{
    unsigned short userID[ACCOUNT_NUM];                 //用户id
    char userName[ACCOUNT_NUM][USER_NAME_LENGTH];       //用户名
    char password[ACCOUNT_NUM][USER_PASSWORD_LENGTH];   //用户密码
    unsigned short groupID[ACCOUNT_NUM];                //用户所在组id
};

//功能函数声明
void CommParser(inode*&);

void Help();        //帮助信息

void Sys_start();   //启动文件系统

//全局变量
FILE* fd = NULL; //文件系统位置

//超级块
filsys superBlock;

//1代表已经使用，0表示空闲
unsigned short inode_bitmap[INODE_NUM];

//用户
userPsw users;

//用户id
unsigned short userID = ACCOUNT_NUM;

//用户名
char userName[USER_NAME_LENGTH + 6];

//当前目录
directory currentDirectory;

char ab_dir[100][14];

unsigned short dir_pointer;

//寻找空闲块
void find_free_block(unsigned int &inode_number)
{
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fread(&superBlock, sizeof(filsys), 1, fd);
    if (superBlock.special_free == 0)
    {
        if (superBlock.special_stack[0] == 0)
        {
            printf("No value block!\n");
            return;
        }
        unsigned int stack[51];
        
        for (int i = 0; i < 50; i++)
        {
            stack[i] = superBlock.special_stack[i];
        }
        stack[50] = superBlock.special_free;
        fseek(fd, DATA_START + (superBlock.special_stack[0] - 50) * BLOCK_SIZE, SEEK_SET);
        fwrite(stack, sizeof(stack), 1, fd);
        
        fseek(fd, DATA_START + superBlock.special_stack[0] * BLOCK_SIZE, SEEK_SET);
        fread(stack, sizeof(stack), 1, fd);
        for (int i = 0; i < 50; i++)
        {
            superBlock.special_stack[i] = stack[i];
        }
        superBlock.special_free = stack[50];
    }
    inode_number = superBlock.special_stack[superBlock.special_free];
    superBlock.special_free--;
    superBlock.s_num_fblock--;
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(filsys), 1, fd);
}

//重置block
void recycle_block(unsigned int &inode_number)
{
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fread(&superBlock, sizeof(filsys), 1, fd);
    if (superBlock.special_free == 49)
    {
        unsigned int block_num;
        unsigned int stack[51];
        if (superBlock.special_stack[0] == 0)
            block_num = 499;
        else
            block_num = superBlock.special_stack[0] - 50;
        for (int i = 0; i < 50; i++)
        {
            stack[i] = superBlock.special_stack[i];
        }
        stack[50] = superBlock.special_free;
        fseek(fd, DATA_START + block_num*BLOCK_SIZE, SEEK_SET);
        fwrite(stack, sizeof(stack), 1, fd);
        block_num -= 50;
        fseek(fd, DATA_START + block_num*BLOCK_SIZE, SEEK_SET);
        fread(stack, sizeof(stack), 1, fd);
        for (int i = 0; i < 50; i++)
        {
            superBlock.special_stack[i] = stack[i];
        }
        superBlock.special_free = stack[50];
    }
    superBlock.special_free++;
    superBlock.s_num_fblock++;
    superBlock.special_stack[superBlock.special_free] = inode_number;
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(filsys), 1, fd);
}

//初始化文件系统
bool Format()
{
    //在当前目录新建一个文件作为文件卷
    FILE* fd = fopen("shentu.penghui", "wb+");
    if (fd == NULL)
    {
        printf("初始化文件系统失败!\n");
        return false;
    }
    
    //初始化超级块
    filsys superBlock;
    superBlock.s_num_inode = INODE_NUM;
    superBlock.s_num_block = BLOCK_NUM + 3 + 64; //3代表，0空闲块、1超级块、2Inode位示图表,64块存inode
    superBlock.s_size_inode = INODE_SIZE;
    superBlock.s_size_block = BLOCK_SIZE;
    superBlock.s_num_fblock = BLOCK_NUM - 2;
    superBlock.s_num_finode = INODE_NUM - 2;
    superBlock.special_stack[0] = 99;
    for (int i = 1; i < 50; i++)
    {
        superBlock.special_stack[i] = 49 - i;
    }
    superBlock.special_free = 47;
    //Write super block into file.
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(filsys), 1, fd);
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fread(&superBlock, sizeof(filsys), 1, fd);
    
    //初始化位示图
    unsigned short inode_bitmap[INODE_NUM];
    
    memset(inode_bitmap, 0, INODE_NUM);
    inode_bitmap[0] = 1;
    inode_bitmap[1] = 1;
    //Write bitmaps into file.
    fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
    fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
    
    //成组连接
    unsigned int stack[51];
    for (int i = 0; i < BLOCK_NUM / 50; i++)
    {
        memset(stack, 0, sizeof(stack));
        for (unsigned int j = 0; j < 50; j++)
        {
            stack[j] = (49 + i * 50) - j;
        }
        stack[0] = 49 + (i + 1) * 50;
        stack[50] = 49;
        fseek(fd, DATA_START + (49 + i * 50)*BLOCK_SIZE, SEEK_SET);
        fwrite(stack, sizeof(unsigned int) * 51, 1, fd);
    }
    memset(stack, 0, sizeof(stack));
    for (int i = 0; i < 12; i++)
    {
        stack[i] = 511 - i;
    }
    stack[0] = 0;
    stack[50] = 11;
    fseek(fd, DATA_START + 511 * BLOCK_SIZE, SEEK_SET);
    fwrite(stack, sizeof(unsigned int) * 51, 1, fd);
    
    fseek(fd, DATA_START + 49 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 99 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 149 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 199 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 249 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 299 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 349 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 399 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 449 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 499 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    fseek(fd, DATA_START + 511 * BLOCK_SIZE, SEEK_SET);
    fread(stack, sizeof(unsigned int) * 51, 1, fd);
    
    
    //创建根目录
    inode iroot_tmp;
    iroot_tmp.i_ino = 0;
    iroot_tmp.di_number = 2;
    iroot_tmp.di_mode = 0;                    //0 =目录
    iroot_tmp.di_size = 0;                    //目录大小为0
    memset(iroot_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
    iroot_tmp.di_addr[0] = 0;
    iroot_tmp.permission = MAX_OWNER_PERMISSION;
    iroot_tmp.di_grp = 1;
    iroot_tmp.di_uid = 0;                    //Root user id.
    iroot_tmp.icount = 0;
    time_t t = time(0);
    strftime(iroot_tmp.time, sizeof(iroot_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
    iroot_tmp.time[64] = 0;
    fseek(fd, INODE_START, SEEK_SET);
    fwrite(&iroot_tmp, sizeof(inode), 1, fd);
    
    //直接创建文件
    directory droot_tmp;
    memset(droot_tmp.fileName, 0, sizeof(char) * DIRECTORY_NUM * FILE_NAME_LENGTH);
    memset(droot_tmp.inodeID, -1, sizeof(unsigned int) * DIRECTORY_NUM);
    strcpy(droot_tmp.fileName[0], ".");
    droot_tmp.inodeID[0] = 0;
    strcpy(droot_tmp.fileName[1], "..");
    droot_tmp.inodeID[1] = 0;
    //A sub directory for accounting files
    strcpy(droot_tmp.fileName[2], "pw");
    droot_tmp.inodeID[2] = 1;
    
    //写入
    fseek(fd, DATA_START, SEEK_SET);
    fwrite(&droot_tmp, sizeof(directory), 1, fd);
    
    //创建用户文件
    //先创建 inode
    inode iaccouting_tmp;
    iaccouting_tmp.i_ino = 1;
    iaccouting_tmp.di_number = 1;
    iaccouting_tmp.di_mode = 1;                    //1 代表文件
    iaccouting_tmp.di_size = sizeof(userPsw);    //文件大小
    memset(iaccouting_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
    iaccouting_tmp.di_addr[0] = 1;                //根目录存在第一块
    iaccouting_tmp.di_uid = 0;                    //Root user id.
    iaccouting_tmp.di_grp = 1;
    iaccouting_tmp.permission = 320;
    iaccouting_tmp.icount = 0;
    t = time(0);
    strftime(iaccouting_tmp.time, sizeof(iaccouting_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
    iaccouting_tmp.time[64] = 0;
    fseek(fd, INODE_START + INODE_SIZE, SEEK_SET);
    fwrite(&iaccouting_tmp, sizeof(inode), 1, fd);
    
    //创建账户.
    userPsw paccouting_tmp;
    memset(paccouting_tmp.userName, 0, sizeof(char) * USER_NAME_LENGTH * ACCOUNT_NUM);
    memset(paccouting_tmp.password, 0, sizeof(char) * USER_PASSWORD_LENGTH * ACCOUNT_NUM);
    
    strcpy(paccouting_tmp.userName[0], "shentupenghui");
    strcpy(paccouting_tmp.userName[1], "guest");
    strcpy(paccouting_tmp.password[0], "shentupenghui");
    strcpy(paccouting_tmp.password[1], "guest");
    //0 代表管理员，其他数字就是userid
    for (unsigned short i = 0; i < ACCOUNT_NUM; i++)
    {
        paccouting_tmp.userID[i] = i;
    }
    paccouting_tmp.groupID[0] = 1;
    paccouting_tmp.groupID[1] = 2;
    //写入文件
    fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
    fwrite(&paccouting_tmp, sizeof(userPsw), 1, fd);
    
    //关闭文件.
    fclose(fd);
    
    return true;
};

//初始化文件系统的各项功能
bool Mount()
{
    //打开文件卷
    fd = fopen("shentu.penghui", "rb+");
    if (fd == NULL)
    {
        printf("文件系统没有找到!\n");
        return false;
    }
    
    //读取超级块信息
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fread(&superBlock, sizeof(superBlock), 1, fd);
    
    //读取节点映射表
    fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
    fread(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
    
    //读取当前目录
    fseek(fd, DATA_START, SEEK_SET);
    fread(&currentDirectory, sizeof(directory), 1, fd);
    
    //读取账户资料
    fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
    fread(&users, sizeof(userPsw), 1, fd);
    
    return true;
};

//登录模块
bool Login(const char* user, const char* password)
{
    //检测参数
    if (user == NULL || password == NULL)
    {
        printf("用户名或密码不合法!\n\n");
        return false;
    }
    if (strlen(user) > USER_NAME_LENGTH || strlen(password) > USER_PASSWORD_LENGTH)
    {
        printf("用户名或密码不合法!\n");
        return false;
    }
    
    //检测是否登录
    if (userID != ACCOUNT_NUM)
    {
        printf("用户已经登录，请先退出.\n");
        return false;
    }
    
    //在账户文件中搜相应的账户名
    for (int i = 0; i < ACCOUNT_NUM; i++)
    {
        if (strcmp(users.userName[i], user) == 0)
        {
            //验证相应的密码
            if (strcmp(users.password[i], password) == 0)
            {
                //登录成功提示
                printf("登录成功！.\n");
                userID = users.userID[i];
                //个性化设置
                memset(userName, 0, USER_NAME_LENGTH + 6);
                if (userID == 0)
                {
                    strcat(userName, "superman ");
                    strcat(userName, users.userName[i]);
                    strcat(userName, "$");
                }
                else
                {
                    strcat(userName, users.userName[i]);
                    strcat(userName, "#");
                }
                
                return true;
            }
            else
            {
                //密码错误的提示
                printf("密码错误.\n");
                return false;
            }
        }
    }
    
    //用户名未找到
    printf("登录失败，没有相应的用户.\n");
    return false;
    
};

//登出功能
void Logout()
{
    userID = ACCOUNT_NUM;
    memset(&users, 0, sizeof(users));
    memset(userName, 0, 6 + USER_NAME_LENGTH);
    Mount();
};

//根据文件名创建文件
bool CreateFile(const char* filename)
{
    //文件名合法性检测
    if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
    {
        printf("不合法的文件名.\n");
        return false;
    }
    
    //检测是否还有空闲的block
    if (superBlock.s_num_fblock <= 0 || superBlock.s_num_finode <= 0)
    {
        printf("没有空间创建新文件了.\n");
        return false;
    }
    //若有空闲block，则找新的inode
    int new_ino = 0;
    unsigned int new_block_addr = -1;
    for (; new_ino < INODE_NUM; new_ino++)
    {
        if (inode_bitmap[new_ino] == 0)
        {
            break;
        }
    }
    
    //检测当前目录是否有同名文件
    for (int i = 0; i < DIRECTORY_NUM; i++)
    {
        if (strcmp(currentDirectory.fileName[i], filename) == 0)
        {
            inode* tmp_file_inode = new inode;
            int tmp_file_ino = currentDirectory.inodeID[i];
            fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
            fread(tmp_file_inode, sizeof(inode), 1, fd);
            if (tmp_file_inode->di_mode == 0) continue;
            else {
                printf("文件名'%s' 有重复.\n", currentDirectory.fileName[i]);
                return false;
            }
        }
    }
    
    //检测当前目录的文件数量是否达到限制
    int itemCounter = 0;
    for (int i = 0; i < DIRECTORY_NUM; i++)
    {
        if (strlen(currentDirectory.fileName[i]) > 0)
        {
            itemCounter++;
        }
    }
    if (itemCounter >= DIRECTORY_NUM)
    {
        printf("File creation error: Too many files or directories in current path.\n");
        return false;
    }
    
    //创建新的inode
    inode ifile_tmp;
    ifile_tmp.i_ino = new_ino;
    ifile_tmp.di_number = 1;
    ifile_tmp.di_mode = 1;                    //1 表示文件
    ifile_tmp.di_size = 0;                    //新文件大小为0
    memset(ifile_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
    ifile_tmp.di_uid = userID;                //当前用户id.
    ifile_tmp.di_grp = users.groupID[userID];//当前用户的组
    ifile_tmp.permission = MAX_PERMISSION;
    ifile_tmp.icount = 0;
    time_t t = time(0);
    strftime(ifile_tmp.time, sizeof(ifile_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
    ifile_tmp.time[64];
    fseek(fd, INODE_START + new_ino * INODE_SIZE, SEEK_SET);
    fwrite(&ifile_tmp, sizeof(inode), 1, fd);
    
    //更新映射表
    inode_bitmap[new_ino] = 1;
    fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
    fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
    
    //更新目录
    //查找当前目录的inode
    int pos_directory_inode = 0;
    pos_directory_inode = currentDirectory.inodeID[0]; //"."
    inode tmp_directory_inode;
    fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
    fread(&tmp_directory_inode, sizeof(inode), 1, fd);
    
    //加入当前目录
    for (int i = 2; i < DIRECTORY_NUM; i++)
    {
        if (strlen(currentDirectory.fileName[i]) == 0)
        {
            strcat(currentDirectory.fileName[i], filename);
            currentDirectory.inodeID[i] = new_ino;
            break;
        }
    }
    //写入文件
    fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(&currentDirectory, sizeof(directory), 1, fd);
    
    //更新相关信息
    directory tmp_directory = currentDirectory;
    int tmp_pos_directory_inode = pos_directory_inode;
    while (true)
    {
        tmp_directory_inode.di_number++;
        fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
        fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
        //如果到了根目录就停止更新
        if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
        {
            break;
        }
        tmp_pos_directory_inode = tmp_directory.inodeID[1];        //".."
        fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
        fread(&tmp_directory_inode, sizeof(inode), 1, fd);
        fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
        fread(&tmp_directory, sizeof(directory), 1, fd);
    }
    
    //更新超级块
    superBlock.s_num_finode--;
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(filsys), 1, fd);
    
    return true;
};

//删除文件
bool DeleteFile(const char* filename)
{
    //文件名合法性检测
    if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
    {
        printf("Error: Illegal file name.\n");
        return false;
    }
    
    //1.检测文件名是否存在当前目录
    int pos_in_directory = -1, tmp_file_ino;
    inode tmp_file_inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("Delete error: File not found.\n");
            return false;
        }
        
        //2.看inode是否为文件还是目录
        tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
        fread(&tmp_file_inode, sizeof(inode), 1, fd);
        //检测目录
    } while (tmp_file_inode.di_mode == 0);
    
    //权限检测
    
    if (userID == tmp_file_inode.di_uid)
    {
        if (!(tmp_file_inode.permission & OWN_E)) {
            printf("不好意思，你没有权限删除.\n");
            return -1;
        }
    }
    else if (users.groupID[userID] == tmp_file_inode.di_grp) {
        if (!(tmp_file_inode.permission & GRP_E)) {
            printf("不好意思，没有权限删除.\n");
            return -1;
        }
    }
    else {
        if (!(tmp_file_inode.permission & ELSE_E)) {
            printf("不好意思，没有权限删除.\n");
            return -1;
        }
    }
    
    //3.开始删除，inode赋值为0
    if (tmp_file_inode.icount > 0) {
        tmp_file_inode.icount--;
        fseek(fd, INODE_START + tmp_file_inode.i_ino * INODE_SIZE, SEEK_SET);
        fwrite(&tmp_file_inode, sizeof(inode), 1, fd);
        //更新目录
        int pos_directory_inode = currentDirectory.inodeID[0];    //"."
        inode tmp_directory_inode;
        fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
        fread(&tmp_directory_inode, sizeof(inode), 1, fd);
        memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
        currentDirectory.inodeID[pos_in_directory] = -1;
        fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
        fwrite(&currentDirectory, sizeof(directory), 1, fd);
        //更新相关信息
        directory tmp_directory = currentDirectory;
        int tmp_pos_directory_inode = pos_directory_inode;
        while (true)
        {
            tmp_directory_inode.di_number--;
            fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
            fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
            if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
            {
                break;
            }
            tmp_pos_directory_inode = tmp_directory.inodeID[1];        //".."
            fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
            fread(&tmp_directory_inode, sizeof(inode), 1, fd);
            fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
            fread(&tmp_directory, sizeof(directory), 1, fd);
        }
        return true;
    }
    int tmp_fill[sizeof(inode)];
    memset(tmp_fill, 0, sizeof(inode));
    fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
    fwrite(&tmp_fill, sizeof(inode), 1, fd);
    
    //4.更新映射
    inode_bitmap[tmp_file_ino] = 0;
    fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
    fwrite(&inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
    for (int i = 0; i < NADDR - 2; i++)
    {
        if(tmp_file_inode.di_addr[i] != -1)
            recycle_block(tmp_file_inode.di_addr[i]);
        else break;
    }
    if (tmp_file_inode.di_addr[NADDR - 2] != -1) {
        unsigned int f1[128];
        fseek(fd, DATA_START + tmp_file_inode.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
        fread(f1, sizeof(f1), 1, fd);
        for (int k = 0; k < 128; k++) {
            recycle_block(f1[k]);
        }
        recycle_block(tmp_file_inode.di_addr[NADDR - 2]);
    }
    
    // 5. 更新目录
    int pos_directory_inode = currentDirectory.inodeID[0];    //"."
    inode tmp_directory_inode;
    fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
    fread(&tmp_directory_inode, sizeof(inode), 1, fd);
    
    //更新目录项
    memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
    currentDirectory.inodeID[pos_in_directory] = -1;
    fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(&currentDirectory, sizeof(directory), 1, fd);
    directory tmp_directory = currentDirectory;
    int tmp_pos_directory_inode = pos_directory_inode;
    while (true)
    {
        tmp_directory_inode.di_number--;
        fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
        fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
        if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
        {
            break;
        }
        tmp_pos_directory_inode = tmp_directory.inodeID[1];        //".."
        fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
        fread(&tmp_directory_inode, sizeof(inode), 1, fd);
        fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
        fread(&tmp_directory, sizeof(directory), 1, fd);
    }
    
    //6.更新超级块
    superBlock.s_num_finode++;
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(filsys), 1, fd);
    
    return true;
}

//根据文件名打开文件
inode* OpenFile(const char* filename)
{
    //文件名检测
    if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
    {
        printf("不合法文件名.\n");
        return NULL;
    }
    
    //1. 查找是否存在该文件.
    int pos_in_directory = -1;
    inode* tmp_file_inode = new inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("没有找到该文件.\n");
            return NULL;
        }
        
        // 2. 判断inode是否是目录
        int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
        fread(tmp_file_inode, sizeof(inode), 1, fd);
    } while (tmp_file_inode->di_mode == 0);
    
    return tmp_file_inode;
};

//在文件尾添加数据
int Write(inode& ifile, const char* content)
{
    if (content == NULL)
    {
        printf("不合法的文件名.\n");
        return -1;
    }
    //权限检测
    if (userID == ifile.di_uid)
    {
        if (!(ifile.permission & OWN_W)) {
            printf("权限不够.\n");
            return -1;
        }
    }
    else if (users.groupID[userID] == ifile.di_grp) {
        if (!(ifile.permission & GRP_W)) {
            printf("权限不够.\n");
            return -1;
        }
    }
    else {
        if (!(ifile.permission & ELSE_W)) {
            printf("权限不够.\n");
            return -1;
        }
    }
    
    // 1.检测文件是否达到了最大大小
    int len_content = strlen(content);
    unsigned int new_file_length = len_content + ifile.di_size;
    if (new_file_length >= FILE_SIZE_MAX)
    {
        printf("文件不能再大了.\n");
        return -1;
    }
    
    //2. 取得所需的inode，并查看是否有空闲.
    unsigned int block_num;
    if (ifile.di_addr[0] == -1)block_num = -1;
    else
    {
        for (int i = 0; i < NADDR - 2; i++)
        {
            if (ifile.di_addr[i] != -1)
                block_num = ifile.di_addr[i];
            else break;
        }
        int f1[128];
        fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
        int num;
        if (ifile.di_size%BLOCK_SIZE == 0)
            num = ifile.di_size / BLOCK_SIZE;
        else num = ifile.di_size / BLOCK_SIZE + 1;
        if (num > 4 && num <=132)
        {
            fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
            fread(f1, sizeof(f1), 1, fd);
            block_num = f1[num - 4];
        }
        
    }
    int free_space_firstBlock = BLOCK_SIZE - ifile.di_size % BLOCK_SIZE;
    unsigned int num_block_needed;
    if (len_content - free_space_firstBlock > 0)
    {
        num_block_needed = (len_content - free_space_firstBlock) / BLOCK_SIZE + 1;
    }
    else
    {
        num_block_needed = 0;
    }
    //检查是否有空闲
    if (num_block_needed > superBlock.s_num_fblock)
    {
        printf("内存不够.\n");
        return -1;
    }
    
    //3. 写入第一块.
    if (ifile.di_addr[0] == -1)
    {
        find_free_block(block_num);
        ifile.di_addr[0] = block_num;
        fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
    }
    else
        fseek(fd, DATA_START + block_num * BLOCK_SIZE + ifile.di_size % BLOCK_SIZE, SEEK_SET);
    char data[BLOCK_SIZE];
    if (num_block_needed == 0)
    {
        fwrite(content, len_content, 1, fd);
        fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
        fread(data, sizeof(data), 1, fd);
        ifile.di_size += len_content;
    }
    else
    {
        fwrite(content, free_space_firstBlock, 1, fd);
        fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
        fread(data, sizeof(data), 1, fd);
        ifile.di_size += free_space_firstBlock;
    }
    
    //4. 写入其他块，更新文件信息。
    char write_buf[BLOCK_SIZE];
    unsigned int new_block_addr = -1;
    unsigned int content_write_pos = free_space_firstBlock;
    //循环写入
    if ((len_content + ifile.di_size) / BLOCK_SIZE + ((len_content + ifile.di_size) % BLOCK_SIZE == 0 ? 0 : 1) <= NADDR - 2) {
        for (int i = 0; i < num_block_needed; i++)
        {
            find_free_block(new_block_addr);
            if (new_block_addr == -1)return -1;
            for (int j = 0; j < NADDR - 2; j++)
            {
                if (ifile.di_addr[j] == -1)
                {
                    ifile.di_addr[j] = new_block_addr;
                    break;
                }
            }
            memset(write_buf, 0, BLOCK_SIZE);
            unsigned int tmp_counter = 0;
            for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
            {
                if (content[content_write_pos + tmp_counter] == '\0')
                    break;
                write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
            }
            content_write_pos += tmp_counter;
            fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
            fwrite(write_buf, tmp_counter, 1, fd);
            fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
            fread(data, sizeof(data), 1, fd);
            ifile.di_size += tmp_counter;
        }
    }
    else if ((len_content+ifile.di_size)/BLOCK_SIZE+((len_content + ifile.di_size) % BLOCK_SIZE == 0 ? 0 : 1)> NADDR - 2) {
        for (int i = 0; i < NADDR - 2; i++)
        {
            if (ifile.di_addr[i] != -1)continue;
            
            memset(write_buf, 0, BLOCK_SIZE);
            new_block_addr = -1;
            
            find_free_block(new_block_addr);
            if (new_block_addr == -1)return -1;
            ifile.di_addr[i] = new_block_addr;
            unsigned int tmp_counter = 0;
            for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
            {
                if (content[content_write_pos + tmp_counter] == '\0') {
                    break;
                }
                write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
            }
            content_write_pos += tmp_counter;
            fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
            fwrite(write_buf, tmp_counter, 1, fd);
           ifile.di_size += tmp_counter;
        }
        unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
        
        new_block_addr = -1;
        find_free_block(new_block_addr);
        if (new_block_addr == -1)return -1;
        ifile.di_addr[NADDR - 2] = new_block_addr;
        for (int i = 0;i < BLOCK_SIZE / sizeof(unsigned int);i++)
        {
            new_block_addr = -1;
            find_free_block(new_block_addr);
            if (new_block_addr == -1)return -1;
            else
                f1[i] = new_block_addr;
        }
        fseek(fd, DATA_START + ifile.di_addr[4] * BLOCK_SIZE, SEEK_SET);
        fwrite(f1, sizeof(f1), 1, fd);
        bool flag = 0;
        for (int j = 0; j < BLOCK_SIZE / sizeof(int); j++) {
            fseek(fd, DATA_START + f1[j] * BLOCK_SIZE, SEEK_SET);
            unsigned int tmp_counter = 0;
            for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
            {
                if (content[content_write_pos + tmp_counter] == '\0') {
                    //tmp_counter--;
                    flag = 1;
                    break;
                }
                write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
            }
            content_write_pos += tmp_counter;
            fwrite(write_buf, tmp_counter, 1, fd);
            ifile.di_size += tmp_counter;
            if (flag == 1) break;
        }
    }
    time_t t = time(0);
    strftime(ifile.time, sizeof(ifile.time), "%Y/%m/%d %X %A %jday", localtime(&t));
    ifile.time[64] = 0;
    fseek(fd, INODE_START + ifile.i_ino * INODE_SIZE, SEEK_SET);
    fwrite(&ifile, sizeof(inode), 1, fd);
    
    //5.更新超级块
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(superBlock), 1, fd);
    
    return len_content;
};

//输出文件信息
void PrintFile(inode& ifile)
{
    //权限检测
    if (userID == ifile.di_uid)
    {
        if (!(ifile.permission & OWN_R)) {
            printf("没有读取权限.\n");
            return;
        }
    }
    else if (users.groupID[userID] == ifile.di_grp) {
        if (!(ifile.permission & GRP_R)) {
            printf("没有读取权限.\n");
            return;
        }
    }
    else {
        if (!(ifile.permission & ELSE_R)) {
            printf("没有读取权限.\n");
            return;
        }
    }
    int block_num = ifile.di_size / BLOCK_SIZE + 1;
    int print_line_num = 0;        //16 bytes 每一行.
    //从块中读取文件
    char stack[BLOCK_SIZE];
    if (block_num <= NADDR - 2)
    {
        for (int i = 0; i < block_num; i++)
        {
            fseek(fd, DATA_START + ifile.di_addr[i] * BLOCK_SIZE, SEEK_SET);
            fread(stack, sizeof(stack), 1, fd);
            for (int j = 0; j < BLOCK_SIZE; j++)
            {
                if (stack[j] == '\0')break;
                if (j % 16 == 0)
                {
                    printf("\n");
                    printf("%d\t", ++print_line_num);
                }
                printf("%c", stack[j]);
            }
        }
    }
    else if (block_num > NADDR - 2) {
        for (int i = 0; i < NADDR - 2; i++)
        {
            fseek(fd, DATA_START + ifile.di_addr[i] * BLOCK_SIZE, SEEK_SET);
            fread(stack, sizeof(stack), 1, fd);
            for (int j = 0; j < BLOCK_SIZE; j++)
            {
                if (stack[j] == '\0')break;
                if (j % 16 == 0)
                {
                    printf("\n");
                    printf("%d\t", ++print_line_num);
                }
                printf("%c", stack[j]);
            }
        }
        unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
        fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
        fread(f1, sizeof(f1), 1, fd);
        for (int i = 0; i < block_num - (NADDR - 2); i++) {
            fseek(fd, DATA_START + f1[i] * BLOCK_SIZE, SEEK_SET);
            fread(stack, sizeof(stack), 1, fd);
            for (int j = 0; j < BLOCK_SIZE; j++)
            {
                if (stack[j] == '\0')break;
                if (j % 16 == 0)
                {
                    printf("\n");
                    printf("%d\t", ++print_line_num);
                }
                printf("%c", stack[j]);
            }
        }
    }
    printf("\n\n\n");
};

//创建新目录，新目录包含. ..
bool MakeDir(const char* dirname)
{
    //参数检测
    if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
    {
        printf("不合法的目录名.\n");
        return false;
    }
    
    //  1. 检查inode是否用光了。
    if (superBlock.s_num_fblock <= 0 || superBlock.s_num_finode <= 0)
    {
        printf("没有足够空间创建目录了.\n");
        return false;
    }
    int new_ino = 0;
    unsigned int new_block_addr = 0;
    for (; new_ino < INODE_NUM; new_ino++)
    {
        if (inode_bitmap[new_ino] == 0)
        {
            break;
        }
    }
    find_free_block(new_block_addr);
    if (new_block_addr == -1) return false;
    if (new_ino == INODE_NUM || new_block_addr == BLOCK_NUM)
    {
        printf("File creation error: No valid spaces.\n");
        return false;
    }
    
    //2. 检查目录名在当前目录是否有重名.
    for (int i = 0; i < DIRECTORY_NUM; i++)
    {
        if (strcmp(currentDirectory.fileName[i], dirname) == 0)
        {
            inode* tmp_file_inode = new inode;
            int tmp_file_ino = currentDirectory.inodeID[i];
            fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
            fread(tmp_file_inode, sizeof(inode), 1, fd);
            if (tmp_file_inode->di_mode == 1) continue;
            else {
                printf("File creation error: Directory name '%s' has been used.\n", currentDirectory.fileName[i]);
                return false;
            }
        }
    }
    
    //3. 检查当前目录项是否太多了.
    int itemCounter = 0;
    for (int i = 0; i < DIRECTORY_NUM; i++)
    {
        if (strlen(currentDirectory.fileName[i]) > 0)
        {
            itemCounter++;
        }
    }
    if (itemCounter >= DIRECTORY_NUM)
    {
        printf("File creation error: Too many files or directories in current path.\n");
        return false;
    }
    
    //4. 创建新inode.
    inode idir_tmp;
    idir_tmp.i_ino = new_ino;
    idir_tmp.di_number = 1;
    idir_tmp.di_mode = 0;                    //0 代表目录
    idir_tmp.di_size = sizeof(directory);
    memset(idir_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
    idir_tmp.di_addr[0] = new_block_addr;
    idir_tmp.di_uid = userID;
    idir_tmp.di_grp = users.groupID[userID];
    time_t t = time(0);
    strftime(idir_tmp.time, sizeof(idir_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
    idir_tmp.time[64] = 0;
    idir_tmp.icount = 0;
    idir_tmp.permission = MAX_PERMISSION;
    fseek(fd, INODE_START + new_ino * INODE_SIZE, SEEK_SET);
    fwrite(&idir_tmp, sizeof(inode), 1, fd);
    
    //5. 创建目录文件.
    directory tmp_dir;
    memset(tmp_dir.fileName, 0, sizeof(char) * DIRECTORY_NUM * FILE_NAME_LENGTH);
    memset(tmp_dir.inodeID, -1, sizeof(unsigned int) * DIRECTORY_NUM);
    strcpy(tmp_dir.fileName[0], ".");
    tmp_dir.inodeID[0] = new_ino;
    strcpy(tmp_dir.fileName[1], "..");
    tmp_dir.inodeID[1] = currentDirectory.inodeID[0];
    fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
    fwrite(&tmp_dir, sizeof(directory), 1, fd);
    
    //6.  更新映射表.
    inode_bitmap[new_ino] = 1;
    fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
    fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
    
    //7. 更新目录.
    int pos_directory_inode = 0;
    pos_directory_inode = currentDirectory.inodeID[0]; //"."
    inode tmp_directory_inode;
    fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
    fread(&tmp_directory_inode, sizeof(inode), 1, fd);
    for (int i = 2; i < DIRECTORY_NUM; i++)
    {
        if (strlen(currentDirectory.fileName[i]) == 0)
        {
            strcat(currentDirectory.fileName[i], dirname);
            currentDirectory.inodeID[i] = new_ino;
            break;
        }
    }
    fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(&currentDirectory, sizeof(directory), 1, fd);
    directory tmp_directory = currentDirectory;
    int tmp_pos_directory_inode = pos_directory_inode;
    while (true)
    {
        tmp_directory_inode.di_number++;
        fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
        fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
        if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
        {
            break;
        }
        tmp_pos_directory_inode = tmp_directory.inodeID[1];        //".."
        fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
        fread(&tmp_directory_inode, sizeof(inode), 1, fd);
        fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
        fread(&tmp_directory, sizeof(directory), 1, fd);
    }
    
    // 8. 更新超级块.
    superBlock.s_num_finode--;
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(filsys), 1, fd);
    
    return true;
};

//删除一个目录和目录下的所有文件
bool RemoveDir(const char* dirname)
{
    if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
    {
        printf("目录不合法.\n");
        return false;
    }
    
    //1. 检查目录是否存在
    int pos_in_directory = 0;
    int tmp_dir_ino;
    inode tmp_dir_inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], dirname) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("没有找到该目录.\n");
            return false;
        }
        
        // 2. 查看inode是否是文件.
        tmp_dir_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
        fread(&tmp_dir_inode, sizeof(inode), 1, fd);
        //Directory check
    } while (tmp_dir_inode.di_mode == 1);
    
    //3. 权限检
    if (userID == tmp_dir_inode.di_uid)
    {
        if (!(tmp_dir_inode.permission & OWN_E)) {
            printf("权限不够.\n");
            return false;
        }
    }
    else if (users.groupID[userID] == tmp_dir_inode.di_grp) {
        if (!(tmp_dir_inode.permission & GRP_E)) {
            printf("权限不够.\n");
            return false;
        }
    }
    else {
        if (!(tmp_dir_inode.permission & ELSE_E)) {
            printf("权限不够.\n");
            return false;
        }
    }
    //4. 开始删除
    if (tmp_dir_inode.icount > 0) {
        tmp_dir_inode.icount--;
        fseek(fd, INODE_START + tmp_dir_inode.i_ino * INODE_SIZE, SEEK_SET);
        fwrite(&tmp_dir_inode, sizeof(inode), 1, fd);
        //更新目录
        int pos_directory_inode = currentDirectory.inodeID[0];    //"."
        inode tmp_directory_inode;
        fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
        fread(&tmp_directory_inode, sizeof(inode), 1, fd);
        memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
        currentDirectory.inodeID[pos_in_directory] = -1;
        fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
        fwrite(&currentDirectory, sizeof(directory), 1, fd);
        directory tmp_directory = currentDirectory;
        int tmp_pos_directory_inode = pos_directory_inode;
        while (true)
        {
            tmp_directory_inode.di_number--;
            fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
            fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
            if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
            {
                break;
            }
            tmp_pos_directory_inode = tmp_directory.inodeID[1];        //".."
            fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
            fread(&tmp_directory_inode, sizeof(inode), 1, fd);
            fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
            fread(&tmp_directory, sizeof(directory), 1, fd);
        }
        return true;
    }
    directory tmp_dir;
    fseek(fd, DATA_START + tmp_dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fread(&tmp_dir, sizeof(directory), 1, fd);
    
    //查找所有的子文件目录，并删除.
    inode tmp_sub_inode;
    char tmp_sub_filename[FILE_NAME_LENGTH];
    memset(tmp_sub_filename, 0, FILE_NAME_LENGTH);
    for (int i = 2; i < DIRECTORY_NUM; i++)
    {
        if (strlen(tmp_dir.fileName[i]) > 0)
        {
            strcpy(tmp_sub_filename, tmp_dir.fileName[i]);
            fseek(fd, INODE_START + tmp_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
            fread(&tmp_sub_inode, sizeof(inode), 1, fd);
            directory tmp_swp;
            tmp_swp = currentDirectory;
            currentDirectory = tmp_dir;
            tmp_dir = tmp_swp;
            if (tmp_sub_inode.di_mode == 1)
            {
                DeleteFile(tmp_sub_filename);
            }
            else if (tmp_sub_inode.di_mode == 0)
            {
                RemoveDir(tmp_sub_filename);
            }
            tmp_swp = currentDirectory;
            currentDirectory = tmp_dir;
            tmp_dir = tmp_swp;
        }
    }
    
    //5.将inode赋为0.
    int tmp_fill[sizeof(inode)];
    memset(tmp_fill, 0, sizeof(inode));
    fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
    fwrite(&tmp_fill, sizeof(inode), 1, fd);
    //6. 更新映射
    inode_bitmap[tmp_dir_ino] = 0;
    fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
    fwrite(&inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
    for (int i = 0; i < (tmp_dir_inode.di_size / BLOCK_SIZE + 1); i++)
    {
        recycle_block(tmp_dir_inode.di_addr[i]);
    }
    
    //7. 更新目录
    int pos_directory_inode = currentDirectory.inodeID[0];    //"."
    inode tmp_directory_inode;
    fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
    fread(&tmp_directory_inode, sizeof(inode), 1, fd);
    memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
    currentDirectory.inodeID[pos_in_directory] = -1;
    fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * INODE_SIZE, SEEK_SET);
    fwrite(&currentDirectory, sizeof(directory), 1, fd);
    directory tmp_directory = currentDirectory;
    int tmp_pos_directory_inode = pos_directory_inode;
    while (true)
    {
        tmp_directory_inode.di_number--;
        fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
        fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
        if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
        {
            break;
        }
        tmp_pos_directory_inode = tmp_directory.inodeID[1];        //".."
        fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
        fread(&tmp_directory_inode, sizeof(inode), 1, fd);
        fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
        fread(&tmp_directory, sizeof(directory), 1, fd);
    }
    
    //8 更新超级块
    superBlock.s_num_finode++;
    fseek(fd, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(filsys), 1, fd);
    
    return true;
};

//打开一个目录
bool OpenDir(const char* dirname)
{
    //参数检测
    if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
    {
        printf("不合法名称.\n");
        return false;
    }
    //1. 检查是否存在该目录.
    int pos_in_directory = 0;
    inode tmp_dir_inode;
    int tmp_dir_ino;
    do {
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], dirname) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("Delete error: Directory not found.\n");
            return false;
        }
        
        //2. 查找inode，查看是否为目录.
        tmp_dir_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
        fread(&tmp_dir_inode, sizeof(inode), 1, fd);
    } while (tmp_dir_inode.di_mode == 1);
    if (userID == tmp_dir_inode.di_uid)
    {
        if (tmp_dir_inode.permission & OWN_E != OWN_E) {
            printf("权限不够.\n");
            return NULL;
        }
    }
    else if (users.groupID[userID] == tmp_dir_inode.di_grp) {
        if (tmp_dir_inode.permission & GRP_E != GRP_E) {
            printf("权限不够.\n");
            return NULL;
        }
    }
    else {
        if (tmp_dir_inode.permission & ELSE_E != ELSE_E) {
            printf("权限不够.\n");
            return NULL;
        }
    }
    //3. 更新当前目录.
    directory new_current_dir;
    fseek(fd, DATA_START + tmp_dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fread(&new_current_dir, sizeof(directory), 1, fd);
    currentDirectory = new_current_dir;
    if (dirname[0] == '.' && dirname[1] == 0) {
        dir_pointer;
    }
    else if (dirname[0] == '.' && dirname[1] == '.' && dirname[2] == 0) {
        if (dir_pointer != 0) dir_pointer--;
    }
    else {
        for (int i = 0; i < 14; i++) {
            ab_dir[dir_pointer][i] = dirname[i];
        }
        dir_pointer++;
    }
    return true;
};

//显示当前目录下的文件信息
void List()
{
    printf("\n     name\tuser\tgroup\tinodeID\tIcount\tsize\tpermission\ttime\n");
    for (int i = 0; i < DIRECTORY_NUM; i++)
    {
        if (strlen(currentDirectory.fileName[i]) > 0)
        {
            inode tmp_inode;
            fseek(fd, INODE_START + currentDirectory.inodeID[i] * INODE_SIZE, SEEK_SET);
            fread(&tmp_inode, sizeof(inode), 1, fd);
            
            const char* tmp_type = tmp_inode.di_mode == 0 ? "d" : "-";
            const char* tmp_user = users.userName[tmp_inode.di_uid];
            const int tmp_grpID = tmp_inode.di_grp;
            
            printf("%10s\t%s\t%d\t%d\t%d\t%u\t%s", currentDirectory.fileName[i], tmp_user, tmp_grpID, tmp_inode.i_ino, tmp_inode.icount, tmp_inode.di_size, tmp_type);
            for (int x = 8; x > 0; x--) {
                if (tmp_inode.permission & (1 << x)) {
                    if ((x + 1) % 3 == 0) printf("r");
                    else if ((x + 1) % 3 == 2) printf("w");
                    else printf("x");
                }
                else printf("-");
            }
            if(tmp_inode.permission & 1) printf("x\t");
            else printf("-\t");
            printf("%s\n", tmp_inode.time);
        }
    }
    
    printf("\n\n");
}

//显示绝对目录.
void Ab_dir() {
    for (int i = 0; i < dir_pointer; i++)
        printf("%s/", ab_dir[i]);
    printf("\n");
}

//修改文件权限
void Chmod(char* filename) {
    printf("0=文件，1=目录，请输入:");
    int tt;
    scanf("%d", &tt);
    if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
    {
        printf("不合法.\n");
        return;
    }
    
    //1. 检查是否存在.
    int pos_in_directory = -1;
    inode* tmp_file_inode = new inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("没有找到.\n");
            return;
        }
        
        //2. 检查是否存在目录.
        int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
        fread(tmp_file_inode, sizeof(inode), 1, fd);
    } while (tmp_file_inode->di_mode == tt);
    
    printf("请输入 0&1 串给予权限\n");
    printf("格式: rwerwerwe\n");
    char str[10];
    scanf("%s", &str);
    if (userID == tmp_file_inode->di_uid)
    {
        if (!(tmp_file_inode->permission & OWN_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    else if (users.groupID[userID] == tmp_file_inode->di_grp) {
        if (!(tmp_file_inode->permission & GRP_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    else {
        if (!(tmp_file_inode->permission & ELSE_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    int temp = 0;
    for (int i = 0; i < 8; i++) {
        if (str[i] == '1')
            temp += 1 << (8 - i);
    }
    if (str[8] == '1') temp += 1;
    tmp_file_inode->permission = temp;
    fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
    fwrite(tmp_file_inode, sizeof(inode), 1, fd);
    return;
}

//改变文件所属
void Chown(char* filename) {
    printf("0=文件，1=目录，请选择:");
    int tt;
    scanf("%d", &tt);
    //参数检测
    if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
    {
        printf("参数不合法.\n");
        return;
    }
    // 1. 检查文件是否存在.
    int pos_in_directory = -1;
    inode* tmp_file_inode = new inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("Not found.\n");
            return;
        }

        int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
        fread(tmp_file_inode, sizeof(inode), 1, fd);
    } while (tmp_file_inode->di_mode == tt);
    
    if (userID == tmp_file_inode->di_uid)
    {
        if (!(tmp_file_inode->permission & OWN_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    else if (users.groupID[userID] == tmp_file_inode->di_grp) {
        if (!(tmp_file_inode->permission & GRP_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    else {
        if (!(tmp_file_inode->permission & ELSE_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    
    printf("请输入用户名:");
    char str[USER_NAME_LENGTH];
    int i;
    scanf("%s", str);
    for (i = 0; i < ACCOUNT_NUM; i++) {
        if (strcmp(users.userName[i], str) == 0) break;
    }
    if (i == ACCOUNT_NUM) {
        printf("不合法用户!\n");
        return;
    }
    tmp_file_inode->di_uid = i;
    fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
    fwrite(tmp_file_inode, sizeof(inode), 1, fd);
    return;
}

//改变文件所属组.
void Chgrp(char* filename) {
    printf("0=文件，1=目录，请选择:");
    int tt;
    scanf("%d", &tt);
    if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
    {
        printf("不合法.\n");
        return;
    }
    int pos_in_directory = -1;
    inode* tmp_file_inode = new inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("Not found.\n");
            return;
        }
        int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
        fread(tmp_file_inode, sizeof(inode), 1, fd);
    } while (tmp_file_inode->di_mode == tt);
    
    if (userID == tmp_file_inode->di_uid)
    {
        if (!(tmp_file_inode->permission & OWN_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    else if (users.groupID[userID] == tmp_file_inode->di_grp) {
        if (!(tmp_file_inode->permission & GRP_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    else {
        if (!(tmp_file_inode->permission & ELSE_E)) {
            printf("权限不够.\n");
            return;
        }
    }
    
    printf("请输入组号:");
    int ttt, i;
    scanf("%d", &ttt);
    for (i = 0; i < ACCOUNT_NUM; i++) {
        if (users.groupID[i] == ttt) break;
    }
    if (i == ACCOUNT_NUM) {
        printf("用户错误!\n");
        return;
    }
    tmp_file_inode->di_grp = ttt;
    fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
    fwrite(tmp_file_inode, sizeof(inode), 1, fd);
    return;
}

//修改密码
void Passwd() {
    printf("请输入旧密码:");
    char str[USER_PASSWORD_LENGTH];
    scanf("%s", str);
    str[USER_PASSWORD_LENGTH] = 0;
    if (strcmp(users.password[userID], str) == 0) {
        printf("请输入新密码:");
        scanf("%s", str);
        if (strcmp(users.password[userID], str) == 0) {
            printf("两次密码相同!\n");
        }
        else {
            strcpy(users.password[userID], str);
            fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
            fwrite(&users, sizeof(users), 1, fd);
            printf("修改成功\n");
        }
    }
    else {
        printf("密码错误!!!\n");
    }
}

//用户管理
void User_management() {
    if (userID != 0) {
        printf("只有申屠鹏会才可以管理用户!\n");
        return;
    }
    printf("欢迎来到用户管理!\n");
    char c, d;
    scanf("%c", &c);
    while (c != '0') {
        printf("\n1.查看 2.插入 3.删除 0.保存 & 退出\n");
        scanf("%c", &c);
        switch (c) {
            case '1':
                for (int i = 0; i < ACCOUNT_NUM; i++) {
                    if (users.userName[i][0] != 0)
                        printf("%d\t%s\t%d\n", users.userID[i], users.userName[i], users.groupID[i]);
                    else break;
                }
                scanf("%c", &c);
                break;
            case '2':
                int i;
                for (i = 0; i < ACCOUNT_NUM; i++) {
                    if (users.userName[i][0] == 0) break;
                }
                if (i == ACCOUNT_NUM) {
                    printf("用户太多了!\n");
                    break;
                }
                printf("请输入用户名:");
                char str[USER_NAME_LENGTH];
                scanf("%s", str);
                for (i = 0; i < ACCOUNT_NUM; i++) {
                    if (strcmp(users.userName[i], str) == 0) {
                        printf("已经有同名的用户名了!\n");
                    }
                    if (users.userName[i][0] == 0) {
                        strcpy(users.userName[i], str);
                        printf("请输入密码:");
                        scanf("%s", str);
                        strcpy(users.password[i], str);
                        printf("请输入 group ID:");
                        int t;
                        scanf("%d", &t);
                        scanf("%c", &c);
                        if (t > 0) {
                            users.groupID[i] = t;
                            printf("成功!\n");
                        }
                        else {
                            printf("插入失败!\n");
                            strcpy(users.userName[i], 0);
                            strcpy(users.password[i], 0);
                        }
                        break;
                    }
                }
                break;
            case '3':
                printf("请输入userID:");
                int tmp;
                scanf("%d", &tmp);scanf("%c", &c);
                for (int j = tmp; j < ACCOUNT_NUM - 1; j++) {
                    strcpy(users.userName[j], users.userName[j + 1]);
                    strcpy(users.password[j], users.password[j + 1]);
                    users.groupID[j] = users.groupID[j+1];
                }
                printf("成功!\n");
        }
    }
    fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
    fwrite(&users, sizeof(users), 1, fd);
}

//对文件或目录重命名
void Rename(char* filename) {
    printf("0=文件，1=目录，请选择:");
    int tt;
    scanf("%d", &tt);
    if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
    {
        printf("参数不合法.\n");
        return;
    }
    int pos_in_directory = -1;
    inode* tmp_file_inode = new inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("没有找到.\n");
            return;
        }
        int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
        fread(tmp_file_inode, sizeof(inode), 1, fd);
    } while (tmp_file_inode->di_mode == tt);
    printf("请输入新的用户名:");
    char str[14];
    scanf("%s", str);
    str[14] = 0;
    for (int i = 0; i < DIRECTORY_NUM; i++)
    {
        if (currentDirectory.inodeID[i] == tmp_file_inode->i_ino)
        {
            strcpy(currentDirectory.fileName[i], str);
            break;
        }
    }
    fseek(fd, DATA_START + tmp_file_inode->di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(&currentDirectory, sizeof(directory), 1, fd);
    return;
}

//链接
bool ln(char* filename)
{
    printf("0=文件，1=目录，请选择:");
    int tt;
    scanf("%d", &tt);
    if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
    {
        printf("文件名错误.\n");
        return false;
    }
    int pos_in_directory = -1;
    inode* tmp_file_inode = new inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("没有找到.\n");
            return false;
        }
        int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
        fread(tmp_file_inode, sizeof(inode), 1, fd);
    } while (tmp_file_inode->di_mode == tt);
//权限检测
    if (userID == tmp_file_inode->di_uid)
    {
        if (!(tmp_file_inode->permission & OWN_E)) {
            printf("没有权限.\n");
            return false;
        }
    }
    else if (users.groupID[userID] == tmp_file_inode->di_grp) {
        if (!(tmp_file_inode->permission & GRP_E)) {
            printf("没有权限.\n");
            return false;
        }
    }
    else {
        if (!(tmp_file_inode->permission & ELSE_E)) {
            printf("没有权限.\n");
            return false;
        }
    }
    //取得绝对地址
    char absolute[1024];
    int path_pos = 0;
    printf("请输入绝对地址以'#'结束:");
    scanf("%s", absolute);
    char dirname[14];
    int pos_dir = 0;
    bool root = false;
    inode dir_inode;
    directory cur_dir;
    int i;
    for (i = 0; i < 5; i++)
    {
        dirname[i] = absolute[i];
    }
    dirname[i] = 0;
    if (strcmp("root/", dirname) != 0)
    {
        printf("路径错误!\n");
        return false;
    }
    fseek(fd, INODE_START, SEEK_SET);
    fread(&dir_inode, sizeof(inode), 1, fd);
    for (int i = 5; absolute[i] != '#'; i++)
    {
        if (absolute[i] == '/')
        {
            dirname[pos_dir++] = 0;
            pos_dir = 0;
            fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
            fread(&cur_dir, sizeof(directory), 1, fd);
            int i;
            for (i = 0; i < DIRECTORY_NUM; i++)
            {
                if (strcmp(cur_dir.fileName[i], dirname) == 0)
                {
                    fseek(fd, INODE_START + cur_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
                    fread(&dir_inode, sizeof(inode), 1, fd);
                    if (dir_inode.di_mode == 0)break;
                }
            }
            if (i == DIRECTORY_NUM)
            {
                printf("路径错误!\n");
                return false;
            }
        }
        else
            dirname[pos_dir++] = absolute[i];
    }
    //更新当前目录
    fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fread(&cur_dir, sizeof(directory), 1, fd);
    for (i = 0; i < DIRECTORY_NUM; i++)
    {
        if (strlen(cur_dir.fileName[i]) == 0)
        {
            strcat(cur_dir.fileName[i], filename);
            cur_dir.inodeID[i] = tmp_file_inode->i_ino;
            break;
        }
    }
    if (i == DIRECTORY_NUM)
    {
        printf("没有目录项!\n");
        return false;
    }
    fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(&cur_dir, sizeof(directory), 1, fd);
    dir_inode.di_number++;
    tmp_file_inode->icount++;
    fseek(fd, INODE_START + tmp_file_inode->i_ino*INODE_SIZE, SEEK_SET);
    fwrite(tmp_file_inode, sizeof(inode), 1, fd);
    return true;
}

//文件复制
bool Copy(char* filename, inode*& currentInode) {
    currentInode = OpenFile(filename);
    int block_num = currentInode->di_size / BLOCK_SIZE + 1;
    //读取文件
    char stack[BLOCK_SIZE];
    char str[100000];
    int cnt = 0;
    if (block_num <= NADDR - 2)
    {
        for (int i = 0; i < block_num; i++)
        {
            if (currentInode->di_addr[i] == -1) break;
            fseek(fd, DATA_START + currentInode->di_addr[i] * BLOCK_SIZE, SEEK_SET);
            fread(stack, sizeof(stack), 1, fd);
            for (int j = 0; j < BLOCK_SIZE; j++)
            {
                if (stack[j] == '\0') {
                    str[cnt] = 0;
                    break;
                }
                str[cnt++] = stack[j];
            }
        }
    }
    else if (block_num > NADDR - 2) {
        for (int i = 0; i < NADDR - 2; i++)
        {
            fseek(fd, DATA_START + currentInode->di_addr[i] * BLOCK_SIZE, SEEK_SET);
            fread(stack, sizeof(stack), 1, fd);
            for (int j = 0; j < BLOCK_SIZE; j++)
            {
                if (stack[j] == '\0') {
                    str[cnt] = 0;
                    break;
                }
                str[cnt++] = stack[j];
            }
        }
        unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
        fseek(fd, DATA_START + currentInode->di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
        fread(f1, sizeof(f1), 1, fd);
        for (int i = 0; i < block_num - (NADDR - 2); i++) {
            fseek(fd, DATA_START + f1[i] * BLOCK_SIZE, SEEK_SET);
            fread(stack, sizeof(stack), 1, fd);
            for (int j = 0; j < BLOCK_SIZE; j++)
            {
                if (stack[j] == '\0') {
                    str[cnt] = 0;
                    break;
                }
                str[cnt++] = stack[j];
            }
        }
    }
    
    int pos_in_directory = -1;
    inode* tmp_file_inode = new inode;
    do {
        pos_in_directory++;
        for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
        {
            if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
            {
                break;
            }
        }
        if (pos_in_directory == DIRECTORY_NUM)
        {
            printf("没有找到.\n");
            return false;
        }
        int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
        fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
        fread(tmp_file_inode, sizeof(inode), 1, fd);
    } while (tmp_file_inode->di_mode == 0);
    
    //权限检测
    if (userID == tmp_file_inode->di_uid)
    {
        if (!(tmp_file_inode->permission & OWN_E)) {
            printf("权限不够.\n");
            return false;
        }
    }
    else if (users.groupID[userID] == tmp_file_inode->di_grp) {
        if (!(tmp_file_inode->permission & GRP_E)) {
            printf("权限不够.\n");
            return false;
        }
    }
    else {
        if (!(tmp_file_inode->permission & ELSE_E)) {
            printf("权限不够.\n");
            return false;
        }
    }
    //取绝对地址
    char absolute[1024];
    int path_pos = 0;
    printf("请输入地址以'#'结尾:");
    scanf("%s", absolute);
    char dirname[14];
    int pos_dir = 0;
    bool root = false;
    inode dir_inode;
    directory cur_dir;
    int i;
    for (i = 0; i < 5; i++)
    {
        dirname[i] = absolute[i];
    }
    dirname[i] = 0;
    if (strcmp("root/", dirname) != 0)
    {
        printf("路径错误!\n");
        return false;
    }
    fseek(fd, INODE_START, SEEK_SET);
    fread(&dir_inode, sizeof(inode), 1, fd);
    for (int i = 5; absolute[i] != '#'; i++)
    {
        if (absolute[i] == '/')
        {
            dirname[pos_dir++] = 0;
            pos_dir = 0;
            fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
            fread(&cur_dir, sizeof(directory), 1, fd);
            int i;
            for (i = 0; i < DIRECTORY_NUM; i++)
            {
                if (strcmp(cur_dir.fileName[i], dirname) == 0)
                {
                    fseek(fd, INODE_START + cur_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
                    fread(&dir_inode, sizeof(inode), 1, fd);
                    if (dir_inode.di_mode == 0)break;
                }
            }
            if (i == DIRECTORY_NUM)
            {
                printf("路径错误!\n");
                return false;
            }
        }
        else
            dirname[pos_dir++] = absolute[i];
    }
    //更新当前目录
    fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fread(&cur_dir, sizeof(directory), 1, fd);
    for (i = 0; i < DIRECTORY_NUM; i++)
    {
        if (strlen(cur_dir.fileName[i]) == 0)
        {
            strcat(cur_dir.fileName[i], filename);
            int new_ino = 0;
            for (; new_ino < INODE_NUM; new_ino++)
            {
                if (inode_bitmap[new_ino] == 0)
                {
                    break;
                }
            }
            inode ifile_tmp;
            ifile_tmp.i_ino = new_ino;
            ifile_tmp.icount = 0;
            ifile_tmp.di_uid = tmp_file_inode->di_uid;
            ifile_tmp.di_grp = tmp_file_inode->di_grp;
            ifile_tmp.di_mode = tmp_file_inode->di_mode;
            memset(ifile_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
            ifile_tmp.di_size = 0;
            ifile_tmp.permission = tmp_file_inode->permission;
            time_t t = time(0);
            strftime(ifile_tmp.time, sizeof(ifile_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
            cur_dir.inodeID[i] = new_ino;
            Write(ifile_tmp, str);
            //Update bitmaps
            inode_bitmap[new_ino] = 1;
            fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
            fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
            superBlock.s_num_finode--;
            fseek(fd, BLOCK_SIZE, SEEK_SET);
            fwrite(&superBlock, sizeof(filsys), 1, fd);
            break;
        }
    }
    if (i == DIRECTORY_NUM)
    {
        printf("No value iterms!\n");
        return false;
    }
    fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(&cur_dir, sizeof(directory), 1, fd);
    dir_inode.di_number++;
    fseek(fd, INODE_START + tmp_file_inode->i_ino*INODE_SIZE, SEEK_SET);
    fwrite(tmp_file_inode, sizeof(inode), 1, fd);
    return true;
}

int main()
{
    memset(ab_dir, 0, sizeof(ab_dir));
    dir_pointer = 0;
    //先查找文件卷.
    FILE* fs_test = fopen("shentu.penghui", "r");
    if (fs_test == NULL)
    {
        printf("文件卷没找到... 请稍后，正在新建文件卷...\n\n");
        Format();
    }
    Sys_start();
    ab_dir[dir_pointer][0] = 'r';ab_dir[dir_pointer][1] = 'o';ab_dir[dir_pointer][2] = 'o';ab_dir[dir_pointer][3] = 't';ab_dir[dir_pointer][4] = '\0';
    dir_pointer++;
    //登录
    char tmp_userName[USER_NAME_LENGTH];
    char tmp_userPassword[USER_PASSWORD_LENGTH * 5];
    
    do {
        memset(tmp_userName, 0, USER_NAME_LENGTH);
        memset(tmp_userPassword, 0, USER_PASSWORD_LENGTH * 5);
        
        printf("用户登录\n\n");
        printf("用户名:\t");
        scanf("%s", tmp_userName);
        printf("密码:\t");
        char c;
        scanf("%c", &c);
        int i = 0;
        while (1) {
            char ch;
            ch = getch();
            if (ch == '\b') {
                if (i != 0) {
                    printf("\b \b");
                    i--;
                }
                else {
                    tmp_userPassword[i] = '\0';
                }
            }
            else if (ch == '\r') {
                tmp_userPassword[i] = '\0';
                printf("\n\n");
                break;
            }
            else {
                putchar('*');
                tmp_userPassword[i++] = ch;
            }
        }
    } while (Login(tmp_userName, tmp_userPassword) != true);
    inode* currentInode = new inode;
    CommParser(currentInode);
    return 0;
}
//系统启动
void Sys_start() {
    //载入文件系统
    Mount();
    
    printf("**************************************************************\n");
    printf("*                                                            *\n");
    printf("*               欢迎使用文件管理系统                             *\n");
    printf("*                                                            *\n");
    printf("**************************************************************\n");
}

void CommParser(inode*& currentInode)
{
    char para1[11];
    char para2[1024];
    bool flag = false;
    while (true)
    {
        unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
        fseek(fd, DATA_START + 8 * BLOCK_SIZE, SEEK_SET);
        fread(f1, sizeof(f1), 1, fd);
        memset(para1, 0, 11);
        memset(para2, 0, 1024);
        printf("%s>", userName);
        scanf("%s", para1);
        para1[10] = 0;
        //选择功能
        if (strcmp("ls", para1) == 0)//显示当前文件
        {
            flag = false;
            List();
        }
        else if (strcmp("cp", para1) == 0) {//文件复制
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;    //安全保护
            Copy(para2, currentInode);
        }
        else if (strcmp("mv", para1) == 0) {//重命名
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;    //安全保护
            Rename(para2);
        }
        else if (strcmp("pwd", para1) == 0) {//显示当前目录
            flag = false;
            Ab_dir();
        }
        else if (strcmp("passwd", para1) == 0) {
            flag = false;
            Passwd();
        }
        else if (strcmp("chmod", para1) == 0) {//用户权限
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;
            Chmod(para2);
        }
        else if (strcmp("chown", para1) == 0) {//更改用户权限
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;
            Chown(para2);
        }
        else if (strcmp("chgrp", para1) == 0) {//更改所属组
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;
            Chgrp(para2);
        }
        else if (strcmp("info", para1) == 0) {
            printf("系统信息:\n总共的block:%d\n 空闲block:%d\n总inode:%d\n剩余inode:%d\n\n", superBlock.s_num_block, superBlock.s_num_fblock, superBlock.s_num_inode, superBlock.s_num_finode);
            for (int i = 0; i < 50; i++)
            {
                if (i>superBlock.special_free)printf("-1\t");
                else printf("%d\t", superBlock.special_stack[i]);
                if (i % 10 == 9)printf("\n");
            }
            printf("\n\n");
        }
        //创建文件
        else if (strcmp("create", para1) == 0)
        {
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;
            CreateFile(para2);
        }
        //删除文件
        else if (strcmp("rm", para1) == 0)
        {
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;
            DeleteFile(para2);
        }
        //打开文件
        else if (strcmp("open", para1) == 0){
            flag = true;
            scanf("%s", para2);
            para2[1023] = 0;
            currentInode = OpenFile(para2);
        }
        //写文件
        else if (strcmp("insert", para1) == 0 && flag){
            scanf("%s", para2);
            para2[1023] = 0;
            Write(*currentInode, para2);
        }
        //读文件
        else if (strcmp("cat", para1) == 0 && flag) {
            PrintFile(*currentInode);
        }
        //打开一个目录
        else if (strcmp("cd", para1) == 0){
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;
            OpenDir(para2);
        }
        //创建目录
        else if (strcmp("mkdir", para1) == 0){
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;    //security protection
            
            MakeDir(para2);
        }
        //删除目录
        else if (strcmp("rmdir", para1) == 0){
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;    //security protection
            
            RemoveDir(para2);
        }
        //登出系统
        else if (strcmp("logout", para1) == 0){
            flag = false;
            Logout();
            char tmp_userName[USER_NAME_LENGTH], tmp_userPassword[USER_PASSWORD_LENGTH * 5];
            do {
                memset(tmp_userName, 0, USER_NAME_LENGTH);
                memset(tmp_userPassword, 0, USER_PASSWORD_LENGTH * 5);
                printf("用户登录\n\n");
                printf("用户名:\t");
                scanf("%s", tmp_userName);
                printf("密码:\t");
                char c;
                scanf("%c", &c);
                int i = 0;
                while (1) {
                    char ch;
                    ch = getch();
                    if (ch == '\b') {
                        if (i != 0) {
                            printf("\b \b");
                            i--;
                        }
                        else {
                            tmp_userPassword[i] = '\0';
                        }
                    }
                    else if (ch == '\r') {
                        tmp_userPassword[i] = '\0';
                        printf("\n\n");
                        break;
                    }
                    else {
                        putchar('*');
                        tmp_userPassword[i++] = ch;
                    }
                }
            } while (Login(tmp_userName, tmp_userPassword) != true);
            
            inode* currentInode = new inode;
        }
        else if (strcmp("ln", para1) == 0)
        {
            flag = false;
            scanf("%s", para2);
            para2[1023] = 0;
            ln(para2);
        }
        //登录
        else if (strcmp("su", para1) == 0){
            Logout();
            flag = false;
            char para3[USER_PASSWORD_LENGTH * 5];
            memset(para3, 0, USER_PASSWORD_LENGTH + 1);
            scanf("%s", para2);
            para2[1023] = 0;
            printf("密码: ");
            char c;
            scanf("%c", &c);
            int i = 0;
            while (1) {
                char ch;
                ch = getch();
                if (ch == '\b') {
                    if (i != 0) {
                        printf("\b \b");
                        i--;
                    }
                }
                else if (ch == '\r') {
                    para3[i] = '\0';
                    printf("\n\n");
                    break;
                }
                else {
                    putchar('*');
                    para3[i++] = ch;
                }
            }
            para3[USER_PASSWORD_LENGTH] = 0;    //security protection
            
            Login(para2, para3);
        }
        else if (strcmp("Muser", para1) == 0) {
            flag = false;
            User_management();
        }
        //退出系统
        else if (strcmp("exit", para1) == 0){
            flag = false;
            break;
        }
        //help
        else{
            flag = false;
            Help();
        }
    }
};

void Help(){
    printf("系统当前支持指令:\n");
    printf("\t01.退出系统.........................(exit)\n");
    printf("\t02.显示帮助信息......................(help)\n");
    printf("\t03.显示当前目录......................(pwd)\n");
    printf("\t04.列出文件或目录....................(ls)\n");
    printf("\t05.cd到其他目录......................(cd + dirname)\n");
    printf("\t06.返回上级目录...................... (cd ..)\n");
    printf("\t07.创建新目录........................(mkdir + dirname)\n");
    printf("\t08.删除目录..........................(rmdir + dirname)\n");
    printf("\t09.新建文件..........................(create + fname)\n");
    printf("\t10.打开文件..........................(open + fname)\n");
    printf("\t11.读取文件..........................(cat)\n");
    printf("\t12.写入文件..........................(insert + <content>)\n");
    printf("\t13.复制文件..........................(cp + fname)\n");
    printf("\t14.删除文件..........................(rm + fname)\n");
    printf("\t15.查看系统信息.......................(info)\n");
    printf("\t16.退出当前用户.......................(logout)\n");
    printf("\t17.改变当前用户名.....................(su + username)\n");
    printf("\t18.改变文件权限.......................(chmod + fname)\n");
    printf("\t19.改变文件所有者.....................(chown + fname)\n");
    printf("\t20.改变所属组.........................(chgrp + fname)\n");
    printf("\t21.重命名............................(mv + fname)\n");
    printf("\t22.连接..............................(ln + fname)\n");
    printf("\t23.改密码.............................(passwd) \n");
    printf("\t24.用户管理界面.........................(Muser)\n");
};


