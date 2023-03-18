//对于c/c++的文件读写操作 fread/fwrite 速度比 ifstream/ofstream快 100倍; 在linux系统下和mmap差距不大
#include "base/Noncopyable.h"
#include <stdio.h>
#include <string>
#include <fcntl.h>
#include <string>
using namespace std;

class AppendFile : noncopyable{
    public:
    explicit AppendFile(const string &filename);
    ~AppendFile();

    void append(const char* logline,size_t len); //对外的写文件接口

    void flush(); //刷新接口
    
    off_t writtenBytes() const { return writtenBytes_; } //返回已写长度
    
    private:

    FILE* fp_;
    char buffer_[64*1024];//64KB的缓存区大小
    off_t writtenBytes_; //已写入字节

    size_t write(const char *logline,size_t len);
};