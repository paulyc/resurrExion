//
//  logger.hpp - Console/file/syslog logging interface
//  resurrExion
//
//  Created by Paul Ciarlo on 5 March 2019
//
//  Copyright (C) 2020 Paul Ciarlo <paul.ciarlo@gmail.com>
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

#ifndef _github_paulyc_logger_hpp_
#define _github_paulyc_logger_hpp_

#if USE_LOG4CPLUS
#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/initializer.h>
#endif

#include <iostream>

namespace github {
namespace paulyc {

class LoggerInterface
{
public:
    LoggerInterface() = default;
    virtual ~LoggerInterface() = default;

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
    ConsoleInterface() = default;
    virtual ~ConsoleInterface() = default;
    virtual void writeToLog(const std::string &msg) { std::cerr << msg << std::endl; }
};

typedef ConsoleInterface DefaultLoggerInterface;

#endif

LoggerInterface * getLoggerInterface();

class Loggable
{
public:
    enum LogLevel {
        TRACE    = 1,
        DEBUG_    = 2,
        INFO     = 3,
        WARN     = 4,
        ERROR    = 5,
        CRITICAL = 6
    };

    Loggable();

    ~Loggable() = default;

    void logf(LogLevel l, const char *fmt, ...);

    void formatLogPrefix(std::ostringstream &prefix, LogLevel l);

    void setLogLevel(LogLevel l) { _level = l; }
private:
    LogLevel _level;
    std::string _type_str;
};


} /* namespace paulyc */
} /* namespace github */

#endif /* _github_paulyc_logger_hpp_ */
