#pragma once

#ifdef _DBG
void dprintf(const char *format, ...);
#else
#define dprintf
#endif

#define dbg(format, ...) dprintf("[%s:%d] "##format##"\n",__FUNCTION__,__LINE__,__VA_ARGS__);
