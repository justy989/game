#pragma once

#include <stdio.h>

struct Log_t{
     static FILE* log;

     static bool create(const char* filepath);
     static void destroy();
};

#ifdef WIN32
     #define LOG(message, ...) fprintf(Log_t::log, message, __VA_ARGS__);
#else
     // log to file and stdout on linux
     #define LOG(msg...) fprintf(Log_t::log, msg); printf(msg);
#endif
