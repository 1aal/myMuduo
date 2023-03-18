#pragma once
class noncopyable
{
protected: // 子类可调用
    noncopyable(/* args */) = default;
    ~noncopyable() = default;

public:
    noncopyable(const noncopyable &) = delete;
    void operator=(const noncopyable &) = delete;
};