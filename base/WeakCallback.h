#pragma once

#include <functional>
#include <memory>

template <typename T, typename... ARGS>
class WeakCallback
{
private:
    std::weak_ptr<T> object_;
    std::function<void(T *, ARGS...)> function_;

public:
    WeakCallback(const std::weak_ptr<T> &object,
                 const std::function<void(T *, ARGS...)> &function) : object_(object), function_(function){};
    ~WeakCallback() = default;

    void operator()(ARGS &&...args) const
    {
        std::shared_ptr<T> ptr(object_.lock()); // 提权
        if (ptr)
        {
            function_(ptr.get(), std::forward<ARGS>(args)...);
        }
    }
};

template <typename T, typename... ARGS>
WeakCallback<T, ARGS...> makeWeakCallback(const std::shared_ptr<T> &object, void (T::*function)(ARGS...))
{
    return WeakCallback<T, ARGS...>(object, function);
}

template <typename T, typename... ARGS>
WeakCallback<T, ARGS...> makeWeakCallback(const std::shared_ptr<T> &object, void (T::*function)(ARGS...) const)
{
    return WeakCallback<T, ARGS...>(object, function);
}