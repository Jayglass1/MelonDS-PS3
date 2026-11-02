// Tiny crash-safe logger for the PS3 frontend.
//
// There is no screen output and no debugger on the console, so every stage
// appends one line to a file and closes it again right away. If the program
// dies, the last line in the file is the last thing that completed.
//
// Files (both written, whichever location exists):
//   /dev_hdd0/game/MELONDS00/USRDIR/log.txt
//   /dev_hdd0/tmp/melonds-ps3-log.txt
#ifndef PS3_FRONTEND_LOG_H
#define PS3_FRONTEND_LOG_H

#include <stdarg.h>
#include <stdio.h>

namespace melonDS::PS3Frontend
{

inline void LogTo(const char* path, const char* mode, const char* text)
{
    FILE* f = fopen(path, mode);
    if (!f) return;
    fputs(text, f);
    fputc('\n', f);
    fclose(f);
}

inline void Log(const char* fmt, ...)
{
    char text[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    LogTo("/dev_hdd0/game/MELONDS00/USRDIR/log.txt", "a", text);
    LogTo("/dev_hdd0/tmp/melonds-ps3-log.txt", "a", text);
    fprintf(stderr, "%s\n", text);
}

// Start a fresh log for this run.
inline void LogReset()
{
    LogTo("/dev_hdd0/game/MELONDS00/USRDIR/log.txt", "w", "--- melonDS PS3 start ---");
    LogTo("/dev_hdd0/tmp/melonds-ps3-log.txt", "w", "--- melonDS PS3 start ---");
}

}

#endif // PS3_FRONTEND_LOG_H
