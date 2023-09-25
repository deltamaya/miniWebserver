#pragma once

#include "utils.h"
#include <source_location>
using namespace std::string_view_literals;

namespace minilog
{
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
    namespace details
    {
        inline LogLevel g_minlevel = LogLevel::debug;
        inline std::ofstream g_logfile = []
        {if(auto path=getenv("MINILOG_PATH")){return std::ofstream{path,std::ios::app};}return std::ofstream(); }();

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
    }

    inline void set_logfile(const std::filesystem::path &path)
    {
        details::g_logfile = std::ofstream{path};
    }
    inline void set_loglev(LogLevel lev)
    {
        details::g_minlevel = lev;
    }
    namespace details
    {

#if defined(__linux__) || defined(__APPLE__)
        inline constexpr char k_level_ansi_colors[(std::uint8_t)LogLevel::fatal + 1][8] = {
            "\E[32m",
            "\E[34m",
            "\E[33m",
            "\E[31m",
            "\E[31;1m",
        };
        inline constexpr char k_reset_ansi_color[4] = "\E[m";
#define _MINILOG_IF_HAS_ANSI_COLORS(x) x
#else
#define _MINILOG_IF_HAS_ANSI_COLORS(x)
        inline constexpr char k_level_ansi_colors[(std::uint8_t)log_level::fatal + 1][1] = {
            "",
            "",
            "",
            "",
            "",
            "",
            "",
        };
        inline constexpr char k_reset_ansi_color[1] = "";
#endif
        inline std::string getstr_from_level(LogLevel lev)
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

        inline LogLevel getlevel_from_str(std::string_view str)
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
        void log_generic(LogLevel lev, details::_WithSourceLoc<std::format_string<Args...>> fmt_with_loc, Args &&...args)
        {

            const auto &loc = fmt_with_loc.location();
            auto msg = std::vformat(fmt_with_loc.data().get(), std::make_format_args(args...));
            std::chrono::zoned_time now{std::chrono::current_zone(), std::chrono::high_resolution_clock::now()};
            msg = std::format("{} {}:{} [{}] {}", now, loc.file_name(), loc.line(), details::getstr_from_level(lev), msg);
            if (g_logfile)
            {
                g_logfile << msg << std::endl;
            }
            if (lev >= g_minlevel)
            {
                std::cout << _MINILOG_IF_HAS_ANSI_COLORS(k_level_ansi_colors[(std::uint8_t)lev] +)
                    msg _MINILOG_IF_HAS_ANSI_COLORS(+ k_reset_ansi_color) + '\n';
            }
        }
    }
#define _Function(name)                                                                                       \
    template <typename... Args>                                                                               \
    void log_##name(details::_WithSourceLoc<std::format_string<Args...>> loc, Args &&...args)                      \
    {                                                                                                         \
        details::log_generic(details::getlevel_from_str(#name), std::move(loc), std::forward<Args>(args)...); \
    }
    FOREACH_LEVEL(_Function)
#undef _Function
        ;

}