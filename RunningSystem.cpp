//
// Created by 86136 on 2023/5/25.
//

#include <string>
#include <iomanip>
#include "RunningSystem.h"

void initial() {
    // 读硬盘
    disk = fopen(disk_file_name, "wb+");
    char nothing[BLOCKSIZ] = {0};
    fseek(disk, 0, SEEK_SET);
    for(int i = 0; i < FILEBLK + DINODEBLK + 2; i++)
        fwrite(nothing, BLOCKSIZ, 1, disk);

    // 初始化超级块
    // 此时只有root对应的磁盘i节点和数据块
    // 占一个磁盘i节点和一个数据块，编号都为1
    file_system.s_block_size = FILEBLK - 1;
    file_system.s_dinode_size = BLOCKSIZ / DINODESIZ * DINODEBLK;
    file_system.s_free_dinode_num = file_system.s_dinode_size - 1;
    for (int i = 0; i < NICINOD; i++) {
        file_system.s_dinodes[i] = i + 2;
    }
    file_system.s_pdinode = 0;
    file_system.s_rdinode = 2;

    file_system.s_free_block_size = 0;
    file_system.s_pfree_block = 0;
    for (int i = FILEBLK; i > 1; i--) {
        bfree(i);
    }
    file_system.s_fmod = '0';

    fseek(disk, BLOCKSIZ, SEEK_SET);
    fwrite(&file_system, sizeof(struct super_block), 1, disk);

    // 初始化root磁盘i节点
    cur_dir_inode = (hinode) malloc(sizeof(struct inode));
    cur_dir_inode->dinode.di_number = 1;
    cur_dir_inode->dinode.di_mode = DIDIR;
    cur_dir_inode->dinode.di_uid = 0;
    cur_dir_inode->dinode.di_gid = 0;
    cur_dir_inode->dinode.di_size = sizeof(struct dir);
    cur_dir_inode->dinode.di_addr[0] = 0;

    long addr = DINODESTART + 1 * DINODESIZ;
    fseek(disk, addr, SEEK_SET);
    fwrite(&(cur_dir_inode->dinode), DINODESIZ, 1, disk);

    struct dir root;
    // 初始化root目录数据块内容
    root.size = 2;
    // 根目录
    root.files[0].d_index = 1;
    strcpy(root.files[0].d_name, "root");
    // 父目录
    root.files[1].d_index = 1;
    strcpy(root.files[1].d_name, "root");
    for (int i = 2; i < DIRNUM; i++) {
        root.files[i].d_index = 0;
    }

    addr = DATASTART;
    fseek(disk, addr, SEEK_SET);
    fwrite(&(root), sizeof(struct dir), 1, disk);

    // admin账户
    pwds[0].p_uid = 0;
    pwds[0].p_gid = 0;
    strcpy(pwds[0].password, "admin");
    // 一个普通账户
    pwds[1].p_uid = 1;
    pwds[1].p_gid = 1;
    strcpy(pwds[1].password, "1");
    // 清空
    for (int i = 2; i < PWDNUM; i++) {
        pwds[i].p_uid = 0;
        memset(pwds[i].password, 0, 12);
    }
    fseek(disk, 0, SEEK_SET);
    fwrite(pwds, sizeof(PWD), PWDNUM, disk);
    fclose(disk);
}

void install() {
    // 读硬盘
    disk = fopen(disk_file_name, "rb+");

    // 初始化file_system
    fseek(disk, BLOCKSIZ, SEEK_SET);
    fread(&file_system, sizeof(file_system), 1, disk);

    // 初始化hinodes
    for (auto &i: hinodes) {
        i = (hinode) malloc(sizeof(struct inode));
        memset(i, 0, sizeof(inode));
    }

    // 初始化system_openfiles
    for (auto &system_openfile: system_openfiles) {
        system_openfile.i_count = 0;
        system_openfile.fcb.d_index = 0;
    }

    // 初始化pwds
    // 其实是读第一个块
    fseek(disk, 0, SEEK_SET);
    fread(&pwds, sizeof(PWD), PWDNUM, disk);

    // 初始化user_openfiles
    user_openfiles.clear();


    // 读取root目录和cur_dir当前目录
    // 即第一个i节点
    // 初始化cur_dir_inode
    cur_dir_inode = iget(1);
    cur_path = "root";
    int size = cur_dir_inode->dinode.di_size;
    int block_num = size / BLOCKSIZ;
    unsigned int id;
    long addr;
    int i;
    for (i = 0; i < block_num; i++) {
        id = cur_dir_inode->dinode.di_addr[i];
        addr = DATASTART + id * BLOCKSIZ;
        fseek(disk, addr, SEEK_SET);
        fread((char *) (&cur_dir_inode->content) + i * BLOCKSIZ, BLOCKSIZ, 1, disk);
    }
    id = cur_dir_inode->dinode.di_addr[block_num];
    addr = DATASTART + id * BLOCKSIZ;
    fseek(disk, addr, SEEK_SET);
    cur_dir_inode->content = malloc(size - BLOCKSIZ * block_num);
    fread((char *) (cur_dir_inode->content) + block_num * BLOCKSIZ, size - BLOCKSIZ * block_num, 1, disk);

}

// 格式化
void format() {
    // admin账户
    pwds[0].p_uid = 0;
    pwds[0].p_gid = 0;
    strcpy(pwds[0].password, "admin");
    // 一个普通账户
    pwds[1].p_uid = 1;
    pwds[1].p_gid = 1;
    strcpy(pwds[1].password, "1");
    // 清空
    for (int i = 2; i < PWDNUM; i++) {
        strcpy(pwds[0].password, "");
    }
    fseek(disk, 0, SEEK_SET);
    fwrite(pwds, sizeof(PWD), PWDNUM, disk);

    // 重置超级块
    // 此时只有root对应的磁盘i节点和数据块
    // 占一个磁盘i节点和一个数据块，编号都为1
    file_system.s_block_size = FILEBLK - 1;
    file_system.s_dinode_size = BLOCKSIZ / DINODESIZ * DINODEBLK;
    file_system.s_free_dinode_num = file_system.s_dinode_size - 1;
    for (int i = 0; i < NICINOD; i++) {
        file_system.s_dinodes[i] = i + 2;
    }
    file_system.s_pdinode = 0;
    file_system.s_rdinode = 2;

    file_system.s_free_block_size = 0;
    file_system.s_pfree_block = 0;
    for (int i = FILEBLK; i > 1; i--) {
        bfree(i);
    }
    file_system.s_fmod = '0';

    // 清空系统打开表
    for (auto &system_openfile: system_openfiles) {
        system_openfile.i_count = 0;
        system_openfile.fcb.d_index = 0;
    }

    // 清空用户打开表
    user_openfiles.clear();

    // 清空内存结点缓存
    for (int i = 0; i < NHINO; i++) {
        while (hinodes[i]->i_forw) {
            iput(hinodes[i]->i_forw);
        }
    }

    // 当前目录置为root
    cur_dir_inode = iget(1);
    int size = cur_dir_inode->dinode.di_size;
    int block_num = size / BLOCKSIZ;
    unsigned int id;
    long addr;
    int i;
    for (i = 0; i < block_num; i++) {
        id = cur_dir_inode->dinode.di_addr[i];
        addr = DATASTART + id * BLOCKSIZ;
        fseek(disk, addr, SEEK_SET);
        fread((char *) (&cur_dir_inode->content) + i * BLOCKSIZ, BLOCKSIZ, 1, disk);
    }
    id = cur_dir_inode->dinode.di_addr[block_num];
    addr = DATASTART + id * BLOCKSIZ;
    fseek(disk, addr, SEEK_SET);
    fread((char *) (&cur_dir_inode->content) + block_num * BLOCKSIZ, size - BLOCKSIZ * block_num, 1, disk);
    // admin账户
    pwds[0].p_uid = 0;
    pwds[0].p_gid = 0;
    strcpy(pwds[0].password, "admin");
    // 一个普通账户
    pwds[1].p_uid = 1;
    pwds[1].p_gid = 1;
    strcpy(pwds[1].password, "1");
    // 清空
    for (int j = 2; j < PWDNUM; j++) {
        pwds[j].p_uid = 0;
        memset(pwds[j].password, 0, 12);
    }
    fseek(disk, 0, SEEK_SET);
    fwrite(pwds, sizeof(PWD), PWDNUM, disk);
}

// 关机并保存
void halt() {
    // 检查当前用户表，释放
    for (const auto &data: user_openfiles) {
        user_open_table *tmp = data.second;
        for (int i = 0; i < NOFILE; i++) {
            if (tmp->items[i].f_inode == nullptr)
                continue;
            int id = tmp->items[i].index_to_sysopen;
            system_openfiles[id].i_count--;
            if (system_openfiles[id].i_count == 0)
                system_openfiles[id].fcb.d_index = 0;
        }
    }
    user_openfiles.clear();

    // 清空缓存
    for (int i = 0; i < NHINO; i++) {
        while (hinodes[i]->i_forw) {
            iput(hinodes[i]->i_forw);
        }
    }

    // 保存超级块
    fseek(disk, BLOCKSIZ, SEEK_SET);
    fwrite(&file_system, sizeof(struct super_block), 1, disk);

    fseek(disk, 0, SEEK_SET);
    fwrite(pwds, sizeof(PWD), PWDNUM, disk);
    // 关闭磁盘
    fclose(disk);
}

int login(const string &pwd) {
    int i;
    //检查是否有匹配的PWD
    for (i = 0; i < PWDNUM && strcmp(pwd.c_str(), pwds[i].password) != 0; i++);
    if (i == PWDNUM)
        return -1;

    //检查是否已经登录
    if(!strcmp(pwd.c_str(), cur_user.c_str()))
        return -2;

    //
    if (user_openfiles.find(pwd) != user_openfiles.end()) {
        cur_user = pwd;
        return -3;
    }

    //还有空位
    if (user_openfiles.size() < USERNUM) {
        auto *openTable = new user_open_table;
        openTable->p_uid = pwds[i].p_uid;
        openTable->p_gid = pwds[i].p_gid;
        memset(openTable->items, 0, sizeof(user_open_item) * NOFILE);
        user_openfiles.insert(pair<string, user_open_table *>(pwd, openTable));
        //设置当前文件为根目录
        cur_user = pwd;
        cur_dir_inode = iget(1);
        return 0;
    }
    return -3;
}

void logout(const string &pwd) {
    user_open_table *u = user_openfiles.find(pwd)->second;
    //关闭每个文件
    for (auto &item: u->items) {
        short id_to_sysopen = item.index_to_sysopen;
        if (id_to_sysopen != -1) {
            //关闭后打开数为零需要释放内存节点
            system_openfiles[id_to_sysopen].i_count--;
            if (system_openfiles[id_to_sysopen].i_count == 0) {
                iput(iget(system_openfiles[id_to_sysopen].fcb.d_index));
            }
        }
    }
    free(u);
    user_openfiles.erase(pwd);
    cur_user = "";
}

bool access(int operation, inode *file_inode) {
    if (user_openfiles.find(cur_user) == user_openfiles.end()) {
        return false;//没找到该用户
    }
    user_open_table *T = user_openfiles.find(cur_user)->second;
    bool creator = file_inode->dinode.di_uid == T->p_uid;//文件的uid等于用户的uid 说明是创建者
    bool group = file_inode->dinode.di_gid == T->p_gid;//文件的gid等于用户的gid 说明是组内成员
    if (file_inode->dinode.di_gid == 0 && operation == FDELETE)//根目录不能删
        return false;
    if (creator || group && READ)
        return true;
    return false;
}

//判断基础合法性，存在，长度，位于根目录
//清理多余合法符号
//具体待修改.(暂定）


//打开文件
int open_file(string &pathname, int operation) {
    if (judge_path(pathname) != 2)
        return -1;                                               //不是文件格式，返回错误码
    inode *catalog;
    string filename;
    if (pathname.find_last_of('/') == string::npos) {//当前目录的子文件     绝对路径
        catalog = cur_dir_inode;
        filename = pathname;
    } else {
        if (pathname[0] == '/')
            pathname = "root" + pathname;
        int pos = pathname.find_last_of('/') + 1;
        string father_path = pathname.substr(0, pos - 1);
        filename = pathname.substr(pos);
        catalog = find_file(father_path);//获取目录文件的内存索引节点
        if (catalog == nullptr) {
            return -1;//无该路径，返回错误码
        }
    }
    if (!access(READ, catalog))
        return -1;                                                  //权限不足，返回错误码
    auto *catalog_dir = (dir *) catalog->content;
    unsigned int file_index;//文件的硬盘i结点id
    int leisure = -1;//目录下的空闲索引
    int i;
    for (i = 0; i < DIRNUM; i++) {
        if (catalog_dir->files[i].d_name == filename) {//查找成功
            //查找成功，获取磁盘索引号
            file_index = catalog_dir->files[i].d_index;
            break;
        }
        if (catalog_dir->files[i].d_index == 0)
            leisure = i;
    }
    inode * new_inode;
    if (i == DIRNUM) {//没查找成功
        if (operation != BUILD_OPEN)//如果不是创建打开，就返回错误码，未找到文件
            return -2;
        else {//是创建打开
            if (leisure == -1)                                             //若目录已满，则返回错误码
                return -3;
            //创建新结点
            file_index = ialloc(1);
            new_inode = iget(file_index);
            new_inode->dinode.di_mode = DIFILE;
            new_inode->ifChange = 1;
            //修改目录的数据
            strcpy(catalog_dir->files[leisure].d_name, filename.data());
            catalog_dir->files[leisure].d_index = file_index;
            catalog_dir->size++;
            catalog->ifChange = 1;
            //写回文件磁盘i结点内容，写回目录磁盘i结点内容
        }
    }
    else// 查找成功，找到内存索引节点
        new_inode = iget(file_index);
    //修改系统打开文件表
    short sys_leisure = 0;
    for (; sys_leisure < SYSOPENFILE; sys_leisure++) {//找到空闲
        if (system_openfiles[sys_leisure].i_count == 0) {
            system_openfiles[sys_leisure].i_count++;
            system_openfiles[sys_leisure].fcb.d_index = file_index;
            strcpy(system_openfiles[sys_leisure].fcb.d_name, filename.data());
            break;
        }
    }
    if (sys_leisure == SYSOPENFILE)
        return -4;//没找到系统打开表空闲的表项
    //修改用户文件打开表
    user_open_table *T = user_openfiles.find(cur_user)->second;
    int usr_leisure = 0;
    for (; usr_leisure < SYSOPENFILE; usr_leisure++) {
        if (T->items[usr_leisure].f_count == 0) {
            T->items[usr_leisure].f_count++;
            if (operation == FP_TAIL_OPEN)
                T->items[usr_leisure].f_offset = iget(file_index)->dinode.di_size;
            else
                T->items[usr_leisure].f_offset = 0;
            T->items[usr_leisure].index_to_sysopen = sys_leisure;
            T->items[usr_leisure].u_default_mode = operation;
            T->items[usr_leisure].f_inode = new_inode;
            return usr_leisure;//返回用户打开表索引
        }
    }
    return -5;//没找到用户打开表空闲表项
}

//关闭文件
int close_file(int fd) {
    user_open_table *T = user_openfiles.find(cur_user)->second;
    T->items[fd].f_count--;//用户打开表进程数-1
    system_openfiles[T->items[fd].index_to_sysopen].i_count--;
    return 1;
}

// 创建文件夹，输入是文件路径
int mkdir(string &pathname) {
    string father_path;
    inode *catalog;
    string filename;
    if (pathname.find_last_of('/') == std::string::npos) {
        catalog = cur_dir_inode;
        filename = pathname;
    } else {
        if (pathname[0] == '/')
            pathname = "root" + pathname;
        int pos = pathname.find_last_of('/') + 1;
        father_path = pathname.substr(0, pos - 1);
        filename = pathname.substr(pos);
        catalog = find_file(father_path);
        if (catalog == nullptr) {
            return -1;//无该路径，返回错误码
        }
    }
    if (!access(CHANGE, catalog))
        return -1;//权限不足，返回错误码
    //判断目录文件数据区是否有空闲
    //小于最大目录数，说明空闲
    dir *catalog_dir = (dir *) catalog->content;
    if (catalog_dir->size < DIRNUM) {
        //判断是否重复
        int leisure;
        for (int i = 0; i < DIRNUM; i++) {
            if (catalog_dir->files[i].d_name == filename) //如果有已经存在的文件夹，则返回错误码
                return -1;
            if (catalog_dir->files[i].d_index == 0)
                leisure = i;
        }


        //申请索引结点和硬盘数据区
        int new_d_index = ialloc(1);
        inode *new_inode = iget(new_d_index);
        new_inode->dinode.di_addr[0] = balloc();
        new_inode->dinode.di_mode = DIDIR;
        new_inode->dinode.di_size = sizeof(dir);
        new_inode->ifChange = 1;


        //初始化硬盘数据区(索引结点区在ialloc中初始化)
        new_inode->content = malloc(sizeof(dir));
        memset(new_inode->content, 0, sizeof(dir));
        strcpy(((dir *) new_inode->content)->files[0].d_name, "root");
        ((dir *) new_inode->content)->files[0].d_index = 1;
        strcpy(((dir *) new_inode->content)->files[1].d_name,
               father_path.substr(0, father_path.find_last_of('/') - 1).c_str());
        ((dir *) new_inode->content)->files[1].d_index = catalog->d_index;
        ((dir *) new_inode->content)->size = 2;
        //找到父目录空闲的目录项,写入文件名和文件磁盘结点
        strcpy(catalog_dir->files[leisure].d_name, filename.data());
        catalog_dir->files[leisure].d_index = new_d_index;
        catalog_dir->size++;
        catalog->ifChange = 1;
        return 1;
    }

    return -1;
}
//硬连接
int hard_link(string &pathname,string &newname){
    string father_path;
    inode *filea;
    string filename;
    if (pathname[0] == '/')
        pathname = "root" + pathname;
    filea = find_file(pathname);
    if (filea == nullptr)
            return -1;//无该路径，返回错误码
    if (!access(READ, filea))
        return -1;//权限不足，返回错误码
    inode * catalog_b = cur_dir_inode;
    if(((dir*)catalog_b->content)->size==DIRNUM)
        return -1;//目录无空闲
    for(int leisure=0;leisure<DIRNUM;leisure++){
        if(((dir*)catalog_b->content)->files[leisure].d_index==0){
            strcpy(((dir*)catalog_b->content)->files[leisure].d_name,newname.data());
            ((dir*)catalog_b->content)->files[leisure].d_index=filea->d_index;
            ((dir*)catalog_b->content)->size++;
            filea->dinode.di_number++;
            filea->ifChange=1;
            break;
        }
    }
    return 1;
}

int rmdir(string &pathname) {
    inode *father_catalog;
    string filename;
    if (pathname.find_last_of('/') == std::string::npos) {
        father_catalog = cur_dir_inode;
    } else {
        if (pathname[0] == '/')
            pathname = "root" + pathname;
        int pos = pathname.find_last_of('/') + 1;
        string father_path = pathname.substr(0, pos - 1);
        father_catalog = find_file(father_path);
        if (father_catalog == nullptr)
            return -1;//无该路径，返回错误码
        filename = pathname.substr(pos);
    }
    inode *catalog = find_file(pathname);
    if (catalog == nullptr)
            return -1;//无该路径，返回错误码
    if (!access(CHANGE, father_catalog))
        return -1;//权限不足，返回错误码
    auto *catalog_dir = (dir *) catalog->content;//得到路径的目录dir数据
    auto *father_dir = (dir *) father_catalog->content;//得到父目录的dir数据
    if (catalog_dir->size > 2) {
        return -1;//该路径的目录有内容，失败。
    } else {
        //将父目录里该项内容删除
        for (auto &i: father_dir->files) {
            if (i.d_index == catalog->d_index) {//查找该文件的下标
                i.d_index = 0;
                memset(i.d_name, 0, DIRSIZ);
                father_dir->size--;
                catalog->ifChange = 1;
                father_catalog->ifChange = 1;
                catalog->dinode.di_number--;
                break;
            }
        }
        return 1;
    }
}

//移动系统当前路径
int chdir(string &pathname) {
    int value=0;//判断是否是绝对路径
    if (judge_path(pathname) != 1) {
        return -1;
    }
    else {
        if (pathname[0] == '/'){
            if(pathname.size() == 1)
                pathname = "";
            pathname = "root" + pathname;
            value=1;
        }
        else if(pathname[0]=='r'&&pathname[1]=='o'&&pathname[2]=='o'&&pathname[3]=='t')
            value = 1;
        inode *catalog = find_file(pathname);
        if (catalog == nullptr) {
            return -2;//无该路径，返回错误码
        }
        if (!access(CHANGE, catalog))
            return -3;//权限不足，返回错误码
        if(value){
            cur_path=pathname;
        } else{
            cur_path=cur_path+'/'+pathname;
        }
        cur_dir_inode = catalog;
    }
    return 1;
}

int show_dir() {
    if (!access(READ, cur_dir_inode))
        return -1;//权限不足，返回错误码
//    for (auto &file: ((dir *) cur_dir_inode->content)->files) {
//        if (file.d_index != 0) {
//            cout << file.d_name << endl;//输出当前路径下的文件内容
//        }
//    }
    for(int i = 2; i < DIRNUM; i++){
        unsigned int id = ((dir *) cur_dir_inode->content)->files[i].d_index;
        if(id != 0){
            bool inMemory = true;
            inode* tmp = findHinode(id);
            if(tmp == nullptr){
                inMemory = false;
                tmp = getDinodeFromDisk(id);
            }
            if(tmp->dinode.di_mode == DIDIR)
                std::cout << "<DIR>  ";
            else
                std::cout << "<FILE> ";
            std::cout << ((dir *) cur_dir_inode->content)->files[i].d_name << std::endl;
            if(!inMemory){
                free(tmp);
            }
        }
    }
    return 0;
}

int show_whole_dir(){
    // 从root开始
    std::cout << "<DIR>  " << "root" << std::endl;
    show_dir_tree(1, 1);
    return 0;
}

int show_dir_tree(unsigned int id, int depth){
    inode* tmp = findHinode(id);

    bool inMemory = true;
    if(tmp == nullptr){
        inMemory = false;
        tmp = getDinodeFromDisk(id);
    }
    dir* dirs = (dir*)(tmp->content);
    int size = dirs->size;
    for(int i = 2; i < DIRNUM; i++){
        if(size == 2)
            break;
        id = dirs->files[i].d_index;
        if(id != 0) {
            for(int j = 0; j < 4 * depth; j++){
                std::cout << " ";
            }
            inMemory = true;
            tmp = findHinode(id);
            if(tmp == nullptr){
                inMemory = false;
                tmp = getDinodeFromDisk(id);
            }
            if(tmp->dinode.di_mode == DIDIR){
                std::cout << "<DIR>  ";
                std::cout << dirs->files[i].d_name << std::endl;
                show_dir_tree(dirs->files[i].d_index, depth + 1);
            }
            else{
                std::cout << "<FILE> ";
                std::cout << dirs->files[i].d_name << std::endl;
            }
            size--;
        }
    }
    if(!inMemory){
        free(tmp);
    }
    return 0;
}

//显示当前用户ss
string whoami() {
    return cur_user;
}


/* 文件名是否合法，需要findfile，判断是否有权限
// 修改父目录数据区并写入磁盘，iput()删除文件
// false删除失败 true删除成功
// 权限未实现 iput未实现*/
int deleteFile(string pathname) {
    inode *catalog;
    string filename;
    if (pathname.find_last_of('/') == string::npos) {//当前目录的子文件     绝对路径
        catalog = cur_dir_inode;
        filename = pathname;
    } else {
        if (pathname[0] == '/')
            pathname = "root" + pathname;
        int pos = pathname.find_last_of('/') + 1;
        string father_path = pathname.substr(0, pos - 1);
        filename = pathname.substr(pos);
        catalog = find_file(father_path);//获取目录文件的内存索引节点
        if (catalog == nullptr)
            return -2;//无该路径，返回错误码
    }
    if (!access(READ, catalog))
        return PERMISSION_DD;                                                  //权限不足，返回错误码
    auto *catalog_dir = (dir *) catalog->content;
    unsigned int file_index;//文件的硬盘i结点id
    int i;
    for (i = 0; i < DIRNUM; i++) {
        if (catalog_dir->files[i].d_name == filename) {//查找成功
            //查找成功，获取磁盘索引号
            file_index = catalog_dir->files[i].d_index;
            break;
        }
    }
    inode *file_inode;
    if (i == DIRNUM) {//没查找成功
        return NOT_FOUND;//无该文件，删除失败，返回错误码
    }
    //查找成功，找到内存索引节点
    file_inode = iget(file_index);
    //修改系统打开文件表
    for (short sys_i = 0; sys_i < SYSOPENFILE; sys_i++) {//找系统打开表的表项
        if (system_openfiles[sys_i].fcb.d_index == file_index && system_openfiles[sys_i].i_count != 0)
            return -1;//该文件正在被系统打开
    }
    //删除文件，若文件硬连接次数为0，则释放将索引节点中内容指针置为空
    file_inode->dinode.di_number--;
    if (file_inode->dinode.di_number == 0) {
        if(file_inode->content){
            free(file_inode->content);
            file_inode->content = nullptr;
        }
        file_inode->content = nullptr;
        catalog_dir->files[i].d_index = 0;
        catalog_dir->size--;
        catalog->ifChange = 1;
    }
    file_inode->ifChange = 1;
    return 0;//成功删除
}

// 关闭一个已经被用户打开了的文件

void closeFile(const string &pathname) {
    // 判断文件名是否合法
    if (judge_path(pathname) != 2) {
        return;
    }
    // 获取用户的打开表
    user_open_table *userOpenTable = user_openfiles[cur_user];
    // 获取用户uid


    // 遍历查询该文件
    unsigned short id;
    bool found = false;
    int i;
    for (i = 0; i < NOFILE; i++) {
        if (userOpenTable->items[i].f_inode == nullptr) {
            continue;
        }
        id = userOpenTable->items[i].index_to_sysopen;
        if (!strcmp(system_openfiles[id].fcb.d_name, pathname.c_str()) && system_openfiles[id].i_count != 0) {
            found = true;
            break;
        }
    }
    // 用户没有打开该文件
    if (!found)
        return;

    // 确定打开了
    // 释放该文件的内存i节点
    iput(userOpenTable->items[i].f_inode);
    userOpenTable->items[i].f_inode = nullptr;
    // 系统打开表判断是否需要关闭
    system_openfiles[id].i_count--;
    if (system_openfiles[id].i_count == 0) {
        // 将这个文件从系统打开表中关闭
        system_openfiles[id].fcb.d_index = 0;
    }
}

// 从用户以打开的文件中读取内容
// 以字符形式返回内容
// 没有实现权限判断
string readFile(int fd,int len) {
    //判断文件是否被用户打开
    // 获取用户的打开表
    user_open_table *userOpenTable = user_openfiles[cur_user];
    // 获取用户uid
    unsigned short p_uid = userOpenTable->p_uid;
    // 使用fd获取打开文件
    struct user_open_item opened_file = userOpenTable->items[fd];

    // 为0说明读取错误
    if(opened_file.f_count == 0)
        return "";

    hinode file_inode = opened_file.f_inode;

    // 判断用户对该文件是否有读权限
    if(!access(Read, file_inode))
        return "";

    if(opened_file.f_offset+len > file_inode->dinode.di_size){
        len = file_inode->dinode.di_size - opened_file.f_offset;
    }

    //读取
    string result = string((char*)file_inode->content+opened_file.f_offset,len);

    //修改offset
    opened_file.f_offset +=len;

    return result;
}

inode *find_file(string addr) {
    dir *temp_dir = (dir *) cur_dir_inode->content;   //用于目录变更
    unsigned int index; //保存根据文件名找到的FCB的d_index
    int isInDir = 0;    //判断是否在目录中
    hinode final = nullptr;//可以保存退出循环后最后一级的文件或目录的内存i结点
    if (addr[0] == '/') {//绝对路径
        addr = addr.substr(1, addr.length());
        temp_dir = (dir *) (iget(1)->content);
        index = 1;
    }
    char *ADDR = new char[addr.length() + 1];
    addr.copy(ADDR, addr.length(), 0); //addr内容复制到ADDR[]
    *(ADDR + addr.length()) = '\0'; //末尾补上'/0'
    char *token = std::strtok(ADDR, "/");
    while (true) {
        for (auto &file: temp_dir->files) {
            if (strcmp(file.d_name, token) == 0) {
                index = file.d_index;
                isInDir = 1;
                final = iget(file.d_index);
                break;
            }
        }
        if (isInDir == 0) {//找不到符合的文件名
            free(ADDR);
            return nullptr;
        }
        isInDir = 0;

        token = std::strtok(nullptr, "/");
        if (token == nullptr)
            break;
        temp_dir = (dir *) iget(index)->content;
    }
    free(ADDR);
    return final;
}

// 权限未实现

int writeFile(int fd, const string& content) {

 //判断文件是否被用户打开
 // 获取用户的打开表
    user_open_table *userOpenTable = user_openfiles[cur_user];
    // 获取用户uid
    unsigned short p_uid = userOpenTable->p_uid;
    // 使用fd获取打开文件
    struct user_open_item opened_file = userOpenTable->items[fd];
    // 为0说明读取错误
    if(opened_file.f_count == 0)
        return -1; // 文件标识符错误

    hinode file_inode = opened_file.f_inode;
    file_inode->ifChange = 1;
    unsigned long offset = opened_file.f_offset;


    // 写文件
    std::string tmp((char*)file_inode->content);
    tmp = tmp.substr(0, offset);
    tmp += content;

    free(file_inode->content);
    file_inode->content = (char*) malloc(tmp.size() + 1);
    strcpy((char*)file_inode->content, tmp.c_str());

    file_inode->dinode.di_size = tmp.size() + 1;
    userOpenTable->items[fd].f_offset = tmp.size();
    return true;
}
int file_seek(int fd,int offset,int fseek_mode){
    user_open_table *T = user_openfiles.find(cur_user)->second;
    int cur_offset = T->items[fd].f_offset;
    int file_capacity = T->items[fd].f_inode->dinode.di_size;
    switch (fseek_mode)
    {
    case HEAD_FSEEK://从头移动
        cur_offset = offset;
        break;
    case CUR_SEEK://从当前移动
        cur_offset += offset;
        break;
    default:
        return -1;//输入格式错误
        break;
    }
    if(cur_offset>file_capacity){
        file_capacity = cur_offset + 1;
        T->items[fd].f_inode->dinode.di_size = file_capacity;
        void *new_content = malloc(file_capacity);
        memset(new_content,0,file_capacity);
        strcpy((char *)new_content,(char *)T->items[fd].f_inode->content);
        free(T->items[fd].f_inode->content);
        T->items[fd].f_inode->content = new_content;
    }
    T->items[fd].f_offset = cur_offset;
    return 1;
}
// 硬链接次数初始化为1
// 需要考虑文件偏移量，此处未实现
// int createFile(string pathname, int operation){
int createFile(string pathname){
//    if (judge_path(pathname) != 2)
//        return -1;                                               //不是文件格式，返回错误码
    inode *catalog;
    string filename;
    if (pathname.find_last_of('/') == string::npos) {//当前目录的子文件     绝对路径
        catalog = cur_dir_inode;
        filename = pathname;
    } else {
        if (pathname[0] == '/')
            pathname = "root" + pathname;
        int pos = pathname.find_last_of('/') + 1;
        string father_path = pathname.substr(0, pos - 1);
        filename = pathname.substr(pos);
        catalog = find_file(father_path);//获取目录文件的内存索引节点
        if (catalog == nullptr)
            return -1;//无该路径，返回错误码
    }
    if (!access(Write, catalog))
        return -1;                                                  //权限不足，返回错误码
    auto *catalog_dir = (dir *) catalog->content;
    unsigned int file_index;//文件的硬盘i结点id
    int leisure = -1;//目录下的空闲索引
    int i;
    for (i = 0; i < DIRNUM; i++) {
        if (catalog_dir->files[i].d_name == filename) {//查找成功
            //直接返回
            return -2;
        }
        if (catalog_dir->files[i].d_index == 0)
            leisure = i;
    }
    inode * new_inode;
    if (i == DIRNUM) {//没查找成功
        if (leisure == -1)                                             //若目录已满，则返回错误码
            return -3;
        //创建新结点
        file_index = ialloc(1);
        new_inode = iget(file_index);
        new_inode->dinode.di_mode = DIFILE;
        new_inode->ifChange = 1;
        //
        new_inode->dinode.di_number = 1;
        //修改目录的数据
        strcpy(catalog_dir->files[leisure].d_name, filename.data());
        catalog_dir->files[leisure].d_index = file_index;
        catalog_dir->size++;
        catalog->ifChange = 1;

    }
    return 0;//创建成功
}

// 展示所有用户
void show_all_users(){
    std::cout << "uid" << "     gid" << "     pwd" << std::endl;
    for(int i = 0; i < USERNUM; i++){
        if(pwds[i].password[0] != '\0'){
            std::cout << pwds[i].p_uid << "       "
            << pwds[i].p_gid << "       "
            << pwds[i].password << std::endl;
        }
    }
}

// 展示所有登录用户
void show_login_users(){
    std::cout << "uid" << "     gid" << "     pwd" << std::endl;
    for(const auto& user: user_openfiles){
        for(int i = 0; i < USERNUM; i++){
            if(!strcmp(pwds[i].password, user.first.c_str())){
                std::cout << pwds[i].p_uid << "       "
                          << pwds[i].p_gid << "       "
                          << pwds[i].password << std::endl;
            }
        }
    }
}

int switch_user(const string& pwd){
    int i;
    //检查是否有匹配的PWD
    for (i = 0; i < PWDNUM && strcmp(pwd.c_str(), pwds[i].password) != 0; i++);
    if (i == PWDNUM)
        return -1; // pwd无效

    //检查是否已经登录
    if(!strcmp(pwd.c_str(), cur_user.c_str()))
        return -2; // pwd即当前用户

    //
    if (user_openfiles.find(pwd) != user_openfiles.end()) {
        cur_user = pwd;
        return 0; // 切换用户成功
    }
    return -1;
}

// 更改用户所在组
int usermod(int uid, int gid){
    for(int i = 0; i < USERNUM; i++){
        if(!strcmp(pwds[i].password, "")){
            continue;
        }
        if(uid == pwds[i].p_uid){
            pwds[i].p_gid = gid;
            return 0;
        }
    }
    return -1; // uid无效
}

// 创建用户
int useradd(int gid, const std::string& pwd){
    int uid = 0;
    int index = 0;
    for(int i = 0; i < USERNUM; i++){
        if(!strcmp(pwds[i].password, "")){
            index = i;
            continue;
        }
        if(uid <= pwds[i].p_uid){
            uid = pwds[i].p_uid + 1;
        }
    }
    if(index == 0){
        return -1; // 用户达到上限
    }
    pwds[index].p_uid = uid;
    pwds[index].p_gid = gid;
    strcpy(pwds[index].password, pwd.c_str());
    return 0; // 添加用户成功
}

int change_file_owner(string& pathname, int uid){
    inode* tmp =  find_file(pathname);
    if(tmp == nullptr)
        return -1; // 路径无效
    int o_uid = tmp->dinode.di_uid;

    for(int i = 0; i < USERNUM; i++){
        if(!strcmp(pwds[i].password, cur_user.c_str())){
            if(pwds[i].p_uid == 0){
                pwds[i].p_uid = uid;
                return 0; // 管理员可以直接改
            }
            else if(o_uid != pwds[i].p_uid){
                return -2; // 其他非创建者无法修改
            }
            // 创建者也可以改
            pwds[i].p_uid = uid;
            return 0;
        }
    }
    return -1;
}

int change_file_group(string& pathname, int gid){
    inode* tmp =  find_file(pathname);
    if(tmp == nullptr)
        return -1; // 路径无效
    int o_gid = tmp->dinode.di_gid;

    for(int i = 0; i < USERNUM; i++){
        if(!strcmp(pwds[i].password, cur_user.c_str())){
            if(pwds[i].p_uid == 0){
                pwds[i].p_gid = gid;
                return 0; // 管理员可以直接改
            }
            else if(o_gid != pwds[i].p_gid){
                return -2; // 其他非同组用户无法修改
            }
            // 同组用户也可以改
            pwds[i].p_gid = gid;
            return 0;
        }
    }
    return -1;
}

// 显示当前用户打开的文件信息
void show_user_opened_files(){
    auto items = user_openfiles.find(cur_user)->second->items;
    std::cout << "filename" << "          fd" << "   count"<< "    offset" << std::endl;
    for(int i = 0; i < NOFILE; i++){
        if(items[i].f_count != 0)
            std::cout << setiosflags(ios::left)  << setw(17) << system_openfiles[items[i].index_to_sysopen].fcb.d_name
                      << items[i].index_to_sysopen
                      << "     " << items[i].f_count
                      << "        " << items[i].f_offset
                      << std::endl;
    }
}
// 显示所有用户打开的文件信息
void show_opened_files(){

    std::cout <<"uid" << "    filename" << "         fd" << "    count" << std::endl;
    for(auto user_openfile: user_openfiles){
        if(user_openfile.second == nullptr)
            continue;
        auto items = user_openfile.second->items;
        for(int i = 0; i < NOFILE; i++){
            if(items[i].f_count != 0)
                std::cout << setiosflags(ios::left)  << user_openfile.second->p_uid
                          << "      "<< setw(17) << system_openfiles[items[i].index_to_sysopen].fcb.d_name
                          << " " << items[i].index_to_sysopen
                          << "    " << items[i].f_count
                          << std::endl;
        }
    }
}
// 显示系统打开表
void show_sys_opened_files(){
    std::cout << "filename" << "         d_index" << "    count" << std::endl;
    for(int i = 0; i < SYSOPENFILE; i++){
        if(system_openfiles[i].i_count != 0){
            std::cout << setiosflags(ios::left) << setw(17) << system_openfiles[i].fcb.d_name
                      << system_openfiles[i].fcb.d_index
                      << "          " << system_openfiles[i].i_count
                      << std::endl;
        }
    }
}