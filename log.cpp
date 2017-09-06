#include "log.h"

FILE* Log_t::log = nullptr;

bool Log_t::create(const char* filepath)
{
     if(log) destroy();
     log = fopen(filepath, "w");
     return log != NULL;
}

void Log_t::destroy()
{
     if(!log) return;
     fclose(log);
     log = nullptr;
}
