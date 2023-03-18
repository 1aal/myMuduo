#include "Buffer.h"
#include "mynet/SocketsOps.h"
#include <errno.h>
#include <sys/uio.h>
// 类的静态变量 类外声明一次分配内存空间
const char Buffer::kCRLF[] = "\r\n";
const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;


//在栈上分配64KB的空间 
ssize_t Buffer::readFd(int fd, int *savedErrno) 
{//从fd的内核缓冲区读取收到的数据
    char extraBuf[65536];//64KB的BUF 足够大 不用调用ioctl()系统调用 询问需要读多大的数据
    iovec vec[2];
    const size_t writeable = writableBytes();

    vec[0].iov_base = begin() + readerIndex_; //第一块缓冲区
    vec[0].iov_len = writeable;
    vec[1].iov_base = extraBuf;  //第二块缓冲区
    vec[1].iov_len = sizeof extraBuf;

    const int iovcnt = (writeable < sizeof extraBuf) ? 2 : 1;
    const ssize_t n = sockets::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (n <= writeable)
    {
        writerIndex_ += n;
    }else{
        writerIndex_ = buffer_.size();
        append(extraBuf,n - writeable);
    }
    return n;
}
