#pragma once

#include "base/Noncopyable.h"
#include <string.h> //memcpy
#include <thread>
#include <string>
#include <limits>

const int kSmallBuffer = 1024 * 4;        // 4kB
const int KlargeBuffer = 1000 * 4 * 1024; // 4MB
/**FixedBuffer，可变缓冲区，有SmallBuffer和largeBuffer两种
 *SmallBuffer默认大小4KB，用于存放一条log消息。为前端类LogStream持有。
相对的，还有Large Buffer，也是FixedBuffer的一个具现，FixedBuffer，默认大小4MB，
用于存放多条log消息。为后端类AsyncLogging持有*/
template <int SIZE>
class FixedBuffer : noncopyable
{
private:
    char data_[SIZE];
    char *cur_; // 当前待写数据位置

public:
    FixedBuffer(/* args */) : cur_(data_){};
    ~FixedBuffer(){};

    void append(const char *buf, size_t len) // 从buf拷贝len长度字节内容到data_
    {
        if (static_cast<size_t>(avail()) > len)
        { // 不能是等于 因为需要'\0'
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }
    const char *data() const
    { // 返回FixedBuffer的数据
        return data_;
    }

    int length() const
    { // 返回长度
        return static_cast<int>(cur_ - data_);
    }

    char *current()
    { // 返回当前cur_指针
        return cur_;
    }

    int avail() const
    { // 返回FixedBuffer可用空间
        return static_cast<int>(end() - cur_);
    }

    void add(size_t len)
    { // 移动cur_指针
        cur_ += len;
    }

    void reset()
    {
        cur_ = data_;
    }

    void bzero()
    {
        memset(data_, '\0', sizeof data);
    }

    std::string toString() const
    {
        return std::string(data_, length());
    }

    // StringPiece toStringPiece() const { return StringPiece(data_, length()); } 高性能字符串转化
private:
    const char *end() const
    { // 返回最后一个位置
        return data_ + sizeof data_;
    }
};

/**
 * LogStream 主要提供operator<<操作，将用户提供的整型数、
 * 浮点数、字符、字符串、字符数组、二进制内存、另一个Small Buffer，格式化为字符串，
 * 并加入当前类的Small Buffer。
 */
class LogStream : noncopyable
{
public : 
    typedef FixedBuffer<kSmallBuffer> Buffer; // Small Buffer，是模板类FixedBuffer<>的一个具现 ;类中的typedef受private影响
private:
    typedef LogStream self;
    Buffer buffer_;

public:
  
    self &operator<<(bool v)
    {
        buffer_.append(v ? "1" : "0", 1);
        return *this;
    }
    self &operator<<(short v)
    {
        *this << static_cast<int>(v);
        return *this;
    }
    self &operator<<(unsigned short v)
    {
        *this << static_cast<unsigned int>(v);
        return *this;
    }
    self &operator<<(int a)
    {
        formatInteger(a);
        return *this;
    }
    self &operator<<(unsigned int a)
    {
        formatInteger(a);
        return *this;
    }
    self &operator<<(long a)
    {
        formatInteger(a);
        return *this;
    }
    self &operator<<(unsigned long a)
    {
        formatInteger(a);
        return *this;
    }
    self &operator<<(unsigned long long a)
    {
        formatInteger(a);
        return *this;
    }

    self &operator<<(void *ptr)
    { // 转为16进制
        uintptr_t v = reinterpret_cast<uintptr_t>(ptr);
        if (buffer_.avail() >= kMaxNumericSize)
        {
            char *buff = buffer_.current();
            int res = snprintf(buff, buffer_.avail(), "%X", v);
            buffer_.add(res);
        }
        return *this;
    }

    self &operator<<(float v)
    {
        *this << static_cast<double>(v);
        return *this;
    }

    self &operator<<(double a)
    {
        if (buffer_.avail() >= kMaxNumericSize)
        {
            int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", a); // 将v转换为字符串, 并填入buffer_当前尾部. %g 自动选择%f, %e格式, 并且不输出无意义0. %.12g 最多保留12位小数
            buffer_.add(static_cast<size_t>(len));
        }
        return *this;
    }

    self &operator<<(char v)
    {
        buffer_.append(&v, 1);
        return *this;
    }

    self &operator<<(const char *str)
    {
        if (str)
        {
            buffer_.append(str, strlen(str) ); //不能用sizeof 而要用strlen ;sizeof多一个'\0'导致被截断
        }
        else
        {
            buffer_.append("(null)", 6);
        }
        return *this;
    }

    self &operator<<(const unsigned char *str)
    {
        return operator<<(reinterpret_cast<const char *>(str));
    }

    self &operator<<(const std::string &v)
    {
        buffer_.append(v.c_str(), v.size());
        return *this;
    }
    self &operator<<(std::string &&v)
    {
        buffer_.append(v.c_str(), v.size());
        return *this;
    }
    self &operator<<(const Buffer &v)
    {
        return operator<<(v.toString());
    }
    
    self &operator<<(const std::thread::id &id){
        char buff[32] = "\0";
        snprintf(buff,32,"%lld",id);
        return *this<<(buff); 
    }
    /***/
    void append(const char *data, int len)
    { // 添加log信息
        buffer_.append(data, len);
    }

    const Buffer &Getbuffer() const
    {
        return buffer_;
    }

    void resetBuff()
    {
        buffer_.reset();
    }

private:
    // void staticCheck(); ///用于静态编译时的一些断言检查 具体作用是检查设置的kMaxNumbericSize是否足够大 大于
    void staticCheck()
    {
        static_assert(kMaxNumericSize - 10 > std::numeric_limits<double>::digits10,
                      "kMaxNumericSize is large enough");
        static_assert(kMaxNumericSize - 10 > std::numeric_limits<long double>::digits10,
                      "kMaxNumericSize is large enough");
        static_assert(kMaxNumericSize - 10 > std::numeric_limits<long>::digits10,
                      "kMaxNumericSize is large enough");
        static_assert(kMaxNumericSize - 10 > std::numeric_limits<long long>::digits10,
                      "kMaxNumericSize is large enough");
    }

    template <class T> // 用于整数格式化为字符串输入到buff
    void formatInteger(T a)
    {
        auto str = std::to_string(a);
        buffer_.append(str.c_str(), strlen(str.c_str()));
    }

    static const int kMaxNumericSize = 48;
};
