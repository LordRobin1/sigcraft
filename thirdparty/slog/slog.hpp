#pragma once

#include <iostream>
#include <mutex>
#include <string>
#include <format>

#define RESET   "\033[0m"
#define GRAY    "\033[90m"
#define BLUE    "\033[34m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define GREEN   "\033[32m"

/// @brief slog Is a super lightweight logging library. It provides
/// colored output, thread safety and variable log arguments.
namespace slog {

    /// @brief Enum representing the different log levels. Log level can be changed using setLevel()
    enum class Level {
        TRACE,
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    inline auto logLevel = Level::DEBUG;

    class Logger {
    public:
        template<typename... Args>
        static void trace(const std::string& fmt, const char* file, const int line, Args&&... args) {
            log(fmt, Level::TRACE, file, line, GRAY, "TRC", std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug(const std::string& fmt, const char* file, const int line, Args&&... args) {
            log(fmt, Level::DEBUG, file, line, BLUE, "DBG", std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info(const std::string& fmt, const char* file, const int line, Args&&... args) {
            log(fmt, Level::INFO, file, line, GREEN, "NFO", std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warning(const std::string& fmt, const char* file, const int line, Args&&... args) {
            log(fmt, Level::WARNING, file, line, YELLOW, "WRN", std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void error(const std::string& fmt, const char* file, const int line, Args&&... args) {
            log(fmt, Level::ERROR, file, line, RED, "ERR", std::forward<Args>(args)...);
        }


        static void setLevel(const Level newLevel) {
            logLevel = newLevel;
        }

    private:
        template<typename... Args>
        static void log(const std::string& fmt, const Level level, const char* file, const int line, const char* color, const char* levelStr, Args&&... args) {
            if (logLevel > level) return;
            std::lock_guard<std::mutex> lock(m_mutex);

            std::cout << color << "[" << levelStr << "] " << RESET;
            std::cout << std::vformat(fmt, std::make_format_args(args...));
            std::cout << " " << file << ":" << line << std::endl;
        }

        inline static std::mutex m_mutex;
    };

} // namespace slog

// Macros used for simplified logging, should be the preferred way to log.

#define trc(fmt, ...) slog::Logger::trace(fmt, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define dbg(fmt, ...) slog::Logger::debug(fmt, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define nfo(fmt, ...) slog::Logger::info(fmt, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define wrn(fmt, ...) slog::Logger::warning(fmt, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define err(fmt, ...) slog::Logger::error(fmt, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
