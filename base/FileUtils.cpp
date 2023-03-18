#include "base/FileUtils.h"
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h> //posix接口头文件 里面的函数大多是针对系统调用的封装
#include <base/Logger.h>

AppendFile::AppendFile(const string& filename):fp_(fopen(filename.c_str(),"ae")),//a = append e = O_CLOEXEC
writtenBytes_(0) 
{
    assert(fp_);
    setbuffer(fp_,buffer_,sizeof buffer_);
}

AppendFile::~AppendFile()
{
    fclose(fp_);
}

void AppendFile::append(const char *logline, size_t len)//底层调用的是无锁的fwrite_unlocked
{
    size_t written = 0;
    while (written < len)
    {
        size_t remain = len - written;
        size_t n = write(logline+written,remain); //返回写入的字节数
        if(n != remain){
            int err = ferror(fp_);
            if(err){
                fprintf(stderr,"AppendFile::Append() falied %s\n",strerror_tl(err)); //strerror_tl() from Logger.h
                break;
            }
        }
        written += n;
    }
    writtenBytes_ += len ;
}   

void AppendFile::flush()
{
    fflush(fp_);
}

size_t AppendFile::write(const char *logline, size_t len)
{
    return fwrite_unlocked(logline,1,len,fp_); //无锁版本的写文件
}
