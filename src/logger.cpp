//
//  logger.cpp - Console/file/syslog logging interface
//  ExFATRestore
//
//  Created by Paul Ciarlo on 5 March 2019.
//
//  Copyright (C) 2019 Paul Ciarlo <paul.ciarlo@gmail.com>.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

#include <iomanip>
#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <future>

#include "logger.hpp"

namespace io {
namespace github {
namespace paulyc {

class LoggerInterface
{
public:
    LoggerInterface() {}
    virtual ~LoggerInterface() {}

    virtual void writeToLog(const std::string &msg) = 0;
};


#if USE_LOG4CPLUS

class Log4cplusInterface : public LoggerInterface
{
public:
    Log4cplusInterface() {}

    virtual void writeToLog(const std::string &msg);
};

typedef Log4cplusInterface DefaultLoggerInterface;

#else

class ConsoleInterface : public LoggerInterface
{
public:
    ConsoleInterface() {}

    virtual void writeToLog(const std::string &msg) { std::cerr << msg << std::endl; }
};

typedef ConsoleInterface DefaultLoggerInterface;

#endif

namespace {
    std::future<LoggerInterface*> logger = std::async([]() -> LoggerInterface* { return new DefaultLoggerInterface; });

    LoggerInterface * getLoggerInterface()
    {
        return logger.get();
    }
}

Loggable::Loggable() :
    _level(DEBUG),
    _type_str(typeid(this).name())
{
}

void Loggable::logf(LogLevel l, const char *fmt, ...)
{
    if (l < _level) return;

    std::ostringstream prefix;
    char msg[4096];

    formatLogPrefix(prefix, _level);

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (n > sizeof(msg)) {
        char *tmpMsg = new char[n+1];
        va_start(args, fmt);
        vsnprintf(tmpMsg, n, fmt, args);
        va_end(args);
        getLoggerInterface()->writeToLog(prefix.str() + std::string(tmpMsg));
        delete[] tmpMsg;
    } else {
        getLoggerInterface()->writeToLog(prefix.str() + std::string(msg));
    }
}

void Loggable::formatLogPrefix(std::ostringstream &prefix, LogLevel l)
{
    switch (l) {
        case TRACE:
            prefix << "[TRACE]    [";
            break;
        case DEBUG:
            prefix << "[DEBUG]    [";
            break;
        case INFO:
            prefix << "[INFO]     [";
            break;
        case WARN:
            prefix << "[WARN]     [";
            break;
        case ERROR:
            prefix << "[ERROR]    [";
            break;
        case CRITICAL:
            prefix << "[CRITICAL] [";
            break;
        default:
            prefix << "[UNKNOWN]  [";
            break;
    }

    std::time_t current_tm = std::time(nullptr);
    prefix << std::put_time(std::localtime(&current_tm), "%F %T%z") << "] ";// << "] [" << _type_str << "] ";
}

}
}
}
