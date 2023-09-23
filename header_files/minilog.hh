#pragma once

#include <unistd.h>
#include <format>
#include <iostream>
#include <cerrno>
#include <string_view>
#include <cstdint>
#include <string>
#include <source_location>
using namespace std::string_view_literals;

#define FOREACH_LEVEL(f) f(Debug) \
    f(Info)                       \
        f(Warning)                \
            f(Error)              \
                f(Fatal)

enum class LogLevel : uint8_t
{
#define _Function(name) name,
    FOREACH_LEVEL(_Function)
#undef _Function
};

#define _Function(name) log_##name,
FOREACH_LEVEL(_Function)
#undef _Function

class MiniLog
{
private:
    FILE *fd_;
    LogLevel mlev_;
    std::string getstr_from_level(LogLevel lev)
    {
        switch (lev)
        {
#define _Function(name)    \
    case (LogLevel::name): \
        return #name;
            FOREACH_LEVEL(_Function);
#undef _Function

        default:
            return "Unknown";
        }
    }

    template <class T>
    struct _WithSourceLoc
    {
        T data_;
        std::source_location cur_;

    public:
        template <typename U>
            requires std::constructible_from<T, U>
        consteval _WithSourceLoc(U &&data, std::source_location cur = std::source_location::current()) : data_(std::forward<U>(data)), cur_(std::move(cur))
        {
        }
        constexpr T const &data() const
        {
            return data_;
        }
        constexpr auto location() const
        {
            return cur_;
        }
    };

public:
    MiniLog() : fd_(stdout), mlev_(LogLevel::Debug) {}

    MiniLog(std::string_view path, std::string_view mode="a"sv, LogLevel mlev = LogLevel::Debug) : fd_(fopen(path.data(), mode.data())), mlev_(mlev)
    {
        if (!fd_)
        {
            perror("fopen");
        }
    }

    ~MiniLog()
    {
        fclose(fd_);
    }

public:
    void set_min_level(LogLevel mlev)
    {
        mlev_ = mlev;
    }

    template <class... Args>
    void log(LogLevel lev, _WithSourceLoc<std::format_string<Args...>> fmt_with_loc, Args &&...args)
    {
        if (lev < mlev_)
            return;
        auto cur = fmt_with_loc.location();
        auto fmt = fmt_with_loc.data();
        fprintf(fd_, "%s\n", format("[{}] in file {}, line {}: {}\n", getstr_from_level(lev), cur.file_name(), cur.line(), format(fmt, std::forward<Args>(args)...)).c_str());
    }
};