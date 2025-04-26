#pragma once

#include <iostream>
#include <ctime>
#include <cstdarg>
#include <string>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>

#define INFO 0
#define DEBUG 1
#define WARNING 2
#define ERROR 3
#define FATAL 4

#define SCREEN 1
#define ONEFILE 2

#define DEF_NAME "log.txt"
#define DEF_PATH "/home/mufeng/Search_Engines/log/"
#define SIZE 1024

class Log
{
public:
    // 获取线程安全单例
    static Log &getInstance(int method = ONEFILE)
    {
        static Log instance(method);
        return instance;
    }

    void operator()(int level, const char *format, ...)
    {
        time_t t = time(nullptr);
        struct tm *ctime = localtime(&t);

        char leftbuffer[SIZE];
        snprintf(leftbuffer, sizeof(leftbuffer), "[%s][%04d-%02d-%02d %02d:%02d:%02d]", levelToString(level).c_str(),
                 ctime->tm_year + 1900, ctime->tm_mon + 1, ctime->tm_mday,
                 ctime->tm_hour, ctime->tm_min, ctime->tm_sec);

        va_list args;
        va_start(args, format);
        char rightbuffer[SIZE];
        vsnprintf(rightbuffer, sizeof(rightbuffer), format, args);
        va_end(args);

        char logtxt[SIZE * 2];
        snprintf(logtxt, sizeof(logtxt), "%s %s\n", leftbuffer, rightbuffer);

        printLog(logtxt);
    }

private:
    Log(int method = SCREEN)
        : method_(method), path_(DEF_PATH) {}

    ~Log() {}

    Log(const Log &) = delete;
    Log &operator=(const Log &) = delete;

    std::string levelToString(int level)
    {
        switch (level)
        {
        case INFO:
            return "INFO";
        case DEBUG:
            return "DEBUG";
        case WARNING:
            return "WARNING";
        case ERROR:
            return "ERROR";
        case FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
        }
    }

    void printLog(const std::string &logtxt)
    {
        std::lock_guard<std::mutex> lock(mutex_); // 所有打印操作都加锁

        switch (method_)
        {
        case SCREEN:
            std::cout << logtxt;
            break;
        case ONEFILE:
            printOneFile(logtxt);
            break;
        default:
            break;
        }
    }

    void printOneFile(const std::string &info)
    {
        std::string path = path_ + DEF_NAME;
        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd >= 0)
        {
            write(fd, info.c_str(), info.size());
            close(fd);
        }
    }

private:
    int method_;
    std::string path_;
    std::mutex mutex_; // 加锁保护写入
};
