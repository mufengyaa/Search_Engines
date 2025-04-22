#pragma once

#include <iostream>
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define INFO 0
#define DEBUG 1
#define WARNING 2
#define ERROR 3
#define FATAL 4 // 致命的错误

#define SCREEN 1
#define ONEFILE 2

#define DEF_NAME "log.txt"
#define DEF_PATH "/home/mufeng/c++/Search_Engines/log/"

#define SIZE 1024

class Log
{
public:
    Log(int method = SCREEN)
        : method_(method), path_(DEF_PATH){}
    void operator()(int level, const char *format, ...)
    {
        time_t t = time(nullptr);
        struct tm *ctime = localtime(&t);

        char leftbuffer[SIZE];
        snprintf(leftbuffer, sizeof(leftbuffer), "[%s][%d-%d-%d %d:%d:%d]", levelToString(level).c_str(),
                 ctime->tm_year + 1900, ctime->tm_mon + 1, ctime->tm_mday,
                 ctime->tm_hour, ctime->tm_min, ctime->tm_sec);

        va_list s;
        va_start(s, format);
        char rightbuffer[SIZE];
        vsnprintf(rightbuffer, sizeof(rightbuffer), format, s);
        va_end(s);

        // 格式：默认部分+自定义部分
        char logtxt[SIZE * 2];
        snprintf(logtxt, sizeof(logtxt), "%s %s\n", leftbuffer, rightbuffer);

        printLog(logtxt);
    }
    ~Log(){}

private:
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
            return "NONE";
        }
    }
    void printLog(const std::string &logtxt)
    {
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
        if (fd > 0)
        {
            write(fd, info.c_str(), info.size());
            close(fd);
        }
        else
        {
            return;
        }
    }

private:
    int method_;
    std::string path_;
};

Log lg(ONEFILE);