#pragma once

#include "mynet/Endian.h" //包含了主机字节序和网络字节序的转化函数
#include <vector>
#include <algorithm>
#include <assert.h>
#include <string>
#include <string.h>

// Buffer类其实是封装了一个用户缓冲区，以及向这个缓冲区写数据读数据等一系列控制方法。属于应用层缓冲区
// 对外表现为一块连续内存,通过vector实现,方便客户代码编写,易用性强
// 维护两个下标 从头部读出 向尾部写人
// 从内核缓冲区读取收到的数据 对应到 Buffer类中 其实是 将数据写入 input buff中 通过 readFd()完成

class Buffer
{
public:
    static const size_t kCheapPrepend = 8; // 预留的空间
    static const size_t kInitialSize = 1024;
    explicit Buffer(size_t initialSize = kInitialSize) : buffer_(initialSize + kCheapPrepend),
                                                         readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend)
    {

        assert(readableBytes() == 0);
        assert(writableBytes() == initialSize);
        assert(prependableBytes() == kCheapPrepend);
    };

    void swap(Buffer &rhs)
    {
        buffer_.swap(rhs.buffer_);
        std::swap(readerIndex_, rhs.readerIndex_);
        std::swap(writerIndex_, rhs.writerIndex_);
    }
    // 可读空间大小
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }
    size_t writableBytes() const // 可写空间大小
    {
        return buffer_.size() - writerIndex_;
    };
    // 可重复使用的预留的空间大小
    size_t prependableBytes() const
    {
        return readerIndex_;
    };

    char *beginWrite() // 最开始的可写位置
    {
        return begin() + writerIndex_;
    }

    const char *beginWrite() const // 最开始的可写位置
    {
        return begin() + writerIndex_;
    }
    // 返回当前的可读位置
    const char *peek() const
    {
        return begin() + readerIndex_;
    }

    const char *findCRLF() const // 在readerable区域检索\r\n
    {
        const char *crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
        return crlf == beginWrite() ? NULL : crlf;
    }
    const char *findCRLF(const char *start) const // 在readerable区域检索\r\n
    {
        assert(peek() < start);
        assert(start <= beginWrite());
        const char *crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2); // 在[first1:last1)中查找[first2:last2)序列
        return crlf == beginWrite() ? NULL : crlf;
    }
    const char *findEOL() const // 在readerable区域检索EOF
    {
        const void *eol = memchr(peek(), '\n', readableBytes());
        // memchr(const char* str,int c,size_t n) 参数str所指向的字符串的前 n 个字节中搜索第一次出现字符c。头文件(string.h)

        return static_cast<const char *>(eol);
    }

    const char *findEOL(const char *start) const
    {
        assert(start >= peek());
        assert(start <= beginWrite());

        auto eol = memchr(start, '\n', beginWrite() - start);
        return static_cast<const char *>(eol);
    }

    /* retrieve从readable头部取走最多长度为len byte的数据. 会导致readable空间变化, 可能导致writable空间变化.
     * 这里取走只是移动readerIndex_, writerIndex_, 并不会直接读取或清除readable, writable空间数据　*/
    void retrieve(size_t len) // 理解为从buffer读出/取回 信息;但实际上只是调整了可读下标的位置 没有做任何实际处理
    {
        assert(len <= readableBytes());
        if (len < readableBytes())
        {
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }

    void retrieveAll() // 读完全部数据 重置readerIndex_和writerIndex_
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    void retrieveUntill(const char *end)
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    void retrieveInt64()
    {
        retrieve(sizeof(int64_t));
    }
    void retrieveInt32()
    {
        retrieve(sizeof(int32_t));
    }
    void retrieveInt16()
    {
        retrieve(sizeof(int16_t));
    }
    void retrieveInt8()
    {
        retrieve(sizeof(int8_t));
    }
    std::string retrieveAsString(size_t len)
    {
        assert(len <= readableBytes());
        std::string res(peek(), len);
        retrieve(len);
        return res;
    }
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    // std::string_view
    std::string toStringPiece() const
    {
        return std::string(peek(), static_cast<int>(readableBytes()));
    }

    void ensureWritaleBytes(size_t len) // 扩容
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    void append(const char *data, size_t len)
    {
        ensureWritaleBytes(len); // 扩容操作
        std::copy(data, data + len, beginWrite());
        hasWritten(len);
    }
    void append(const void *data, size_t len)
    {
        append(static_cast<const char *>(data), len);
    }
    void append(const std::string &str)
    {
        append(str.c_str(), str.size());
    }

    void hasWritten(size_t len)
    {
        assert(len <= writableBytes());
        writerIndex_ += len;
    }

    void unWrite(size_t len)
    {
        assert(len <= writableBytes());
        writerIndex_ -= len;
    }

    // 将int64_t转化为网络字节序并写入buffer
    void appendInt64(int64_t x)
    {
        int64_t be64 = sockets::hostToNetwork64(x);
        append(&be64, sizeof be64);
    }

    void appendInt32(int32_t x)
    {
        int32_t be32 = sockets::hostToNetwork32(x);
        append(&be32, sizeof be32);
    }
    void appendInt16(int16_t x)
    {
        int16_t be16 = sockets::hostToNetwork16(x);
        append(&be16, sizeof be16);
    }

    // 大端与小端之间 单字节不需要转化
    void appendInt8(int8_t x)
    {
        append(&x, sizeof x);
    }

    int64_t readInt64()
    {
        int64_t result = peekInt64();
        retrieveInt64();
        return result;
    }
    int32_t readInt32()
    {
        int32_t result = peekInt32();
        retrieveInt32();
        return result;
    }
    int16_t readInt16()
    {
        int16_t result = peekInt16();
        retrieveInt16();
        return result;
    }
    int8_t readInt8()
    {
        int8_t result = peekInt8();
        retrieveInt8();
        return result;
    }

    int64_t peekInt64() const
    {
        assert(readableBytes() >= sizeof(int64_t));
        int64_t be64 = 0;
        memcpy(&be64, peek(), sizeof be64);
        return sockets::networkToHost64(be64);
    }

    int32_t peekInt32() const
    {
        assert(readableBytes() >= sizeof(int32_t));
        int32_t be32 = 0;
        memcpy(&be32, peek(), sizeof be32);
        return sockets::networkToHost32(be32);
    }

    int16_t peekInt16() const
    {
        assert(readableBytes() >= sizeof(int16_t));
        int16_t be16 = 0;
        memcpy(&be16, peek(), sizeof be16);
        return sockets::networkToHost16(be16);
    }

    int8_t peekInt8() const
    {
        assert(readableBytes() >= sizeof(int8_t));
        int8_t x = *peek();
        return x;
    }

    void prependInt64(int64_t x)
    {
        int64_t be64 = sockets::hostToNetwork64(x);
        prepend(&be64, sizeof be64);
    }

    void prependInt32(int32_t x)
    {
        int32_t be32 = sockets::hostToNetwork32(x);
        prepend(&be32, sizeof be32);
    }
    void prependInt16(int16_t x)
    {
        int16_t be16 = sockets::hostToNetwork16(x);
        prepend(&be16, sizeof be16);
    }
    void prependInt8(int8_t x)
    {
        prepend(&x, sizeof x);
    }

    /**
     * 把data数据 往预留区写入（一般用于应用层程序给数据添加头部信息）
     */
    void prepend(const void *data, size_t len)
    {
        assert(len <= prependableBytes());
        readerIndex_ -= len;
        const char *d = static_cast<const char *>(data);
        std::copy(d, d + len, begin() + readerIndex_);
    }

    // 伸缩空间 保留reserve个字节
    void shrink(size_t reserve)
    {
        Buffer other;
        other.ensureWritaleBytes(readableBytes() + reserve);
        other.append(toStringPiece()); // 把所有已有的可读区域内容append
        swap(other);
    }

    size_t inernalCapacity() const
    {
        return buffer_.capacity();
    }

    ssize_t readFd(int fd, int *savedErrno); // 。利用readfd()，在栈上开辟一块65536字节额外缓冲区，利用readv()来读。

private:
    char *begin() // buffer的最开始位置
    {
        return &*buffer_.begin();
    }

    const char *begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            assert(kCheapPrepend < readerIndex_);
            size_t readable = readableBytes();
            // 将所有可读区域重新腾挪到最开始的可读下标的位置
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = kCheapPrepend + readable;
            assert(readable == readableBytes());
        }
    }

private:
    std::vector<char> buffer_;
    size_t readerIndex_; // 可读区域的开始下标
    size_t writerIndex_; // 可写区域的开始下标
    static const char kCRLF[];
};