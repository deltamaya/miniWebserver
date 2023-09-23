#pragma once

#include "utils.h"
#include <source_location>
using namespace std::string_view_literals;

inline std::ofstream g_logfile = []
{if(auto path=getenv("MINILOG_PATH")){return ofstream{path,ios::app};}return ofstream(); }();

#define FOREACH_LEVEL(f) f(debug) \
    f(info)                       \
        f(warning)                \
            f(error)              \
                f(fatal)

enum class LogLevel : uint8_t
{
#define _Function(name) name,
    FOREACH_LEVEL(_Function)
#undef _Function
};
inline LogLevel g_minlevel = LogLevel::debug;
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

void set_logfile(const std::filesystem::path &path)
{
    g_logfile = ofstream{path};
}
void set_loglev(LogLevel lev)
{
    g_minlevel = lev;
}

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
        return "unknown";
    }
}

LogLevel getlevel_from_str(string_view str)
{
#define _Function(lev)        \
    if (#lev == str)          \
    {                         \
        return LogLevel::lev; \
    }
    FOREACH_LEVEL(_Function)
#undef _Function
    return LogLevel::info;
}

template <class... Args>
void log_generic(LogLevel lev, _WithSourceLoc<std::format_string<Args...>> fmt_with_loc, Args &&...args)
{

    const auto &loc = fmt_with_loc.location();
    auto msg = std::vformat(fmt_with_loc.data().get(), std::make_format_args(args...));
    std::chrono::zoned_time now{std::chrono::current_zone(), std::chrono::high_resolution_clock::now()};
    msg = std::format("{} {}:{} [{}] {}", now, loc.file_name(), loc.line(), getstr_from_level(lev), msg);
    if (g_logfile)
    {
        g_logfile << msg <<endl;
    }
    if (lev >= g_minlevel)
    {
        cout << msg << endl;
    }
}

#define _Function(name)                                                                     \
    template <typename... Args>                                                             \
    void log_##name(_WithSourceLoc<format_string<Args...>> loc, Args &&...args)             \
    {                                                                                       \
        log_generic(getlevel_from_str(#name), std::move(loc), std::forward<Args>(args)...); \
    }
FOREACH_LEVEL(_Function)
#undef _Function