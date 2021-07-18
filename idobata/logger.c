#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "logger.h"


static log_level threshold = WARN;
void idobata_log_level(log_level level) { threshold = level; }


static int use_color = 1;
void idobata_log_colored(int flag) { use_color = flag; }


#define DEFINE_ANSI_SGR(name, code) \
    const char *ansi_##name() { \
        return use_color ? "\e[" #code "m" : ""; \
    }

DEFINE_ANSI_SGR(reset, 0);

DEFINE_ANSI_SGR(fg_gray,    30);
DEFINE_ANSI_SGR(fg_red,     31);
DEFINE_ANSI_SGR(fg_green,   32);
DEFINE_ANSI_SGR(fg_yellow,  33);
DEFINE_ANSI_SGR(fg_blue,    34);
DEFINE_ANSI_SGR(fg_magenta, 35);
DEFINE_ANSI_SGR(fg_cyan,    36);
DEFINE_ANSI_SGR(fg_white,   37);
DEFINE_ANSI_SGR(bg_gray,    40);
DEFINE_ANSI_SGR(bg_red,     41);
DEFINE_ANSI_SGR(bg_green,   42);
DEFINE_ANSI_SGR(bg_yellow,  43);
DEFINE_ANSI_SGR(bg_blue,    44);
DEFINE_ANSI_SGR(bg_magenta, 45);
DEFINE_ANSI_SGR(bg_cyan,    46);
DEFINE_ANSI_SGR(bg_white,   47);

DEFINE_ANSI_SGR(fg_bright_gray,     90);
DEFINE_ANSI_SGR(fg_bright_red,      91);
DEFINE_ANSI_SGR(fg_bright_green,    92);
DEFINE_ANSI_SGR(fg_bright_yellow,   93);
DEFINE_ANSI_SGR(fg_bright_blue,     94);
DEFINE_ANSI_SGR(fg_bright_magenta,  95);
DEFINE_ANSI_SGR(fg_bright_cyan,     96);
DEFINE_ANSI_SGR(fg_bright_white,    97);
DEFINE_ANSI_SGR(bg_bright_gray,    100);
DEFINE_ANSI_SGR(bg_bright_red,     101);
DEFINE_ANSI_SGR(bg_bright_green,   102);
DEFINE_ANSI_SGR(bg_bright_yellow,  103);
DEFINE_ANSI_SGR(bg_bright_blue,    104);
DEFINE_ANSI_SGR(bg_bright_magenta, 105);
DEFINE_ANSI_SGR(bg_bright_cyan,    106);
DEFINE_ANSI_SGR(bg_bright_white,   107);

#undef DEFINE_ANSI_SGR

void idobata_log(log_level level, const char *filename, int linenum, const char *fmt, ...)
{
    if (level < threshold) return;

    time_t now = time(NULL);
    struct tm *tt = localtime(&now);

    printf("%s", ansi_fg_bright_gray());
    printf("%04d-%02d-%02d %02d:%02d:%02d", tt->tm_year + 1900, tt->tm_mon, tt->tm_mday, tt->tm_hour, tt->tm_min, tt->tm_sec);
    printf("%s", ansi_reset());

    printf(" ");

    switch (level) {
    case TRACE:
        printf("%s%-5s", ansi_reset(), "TRACE");
        break;

    case DEBUG:
        printf("%s%-5s", ansi_fg_cyan(), "DEBUG");
        break;

    case INFO:
        printf("%s%-5s", ansi_fg_green(), "INFO");
        break;

    case WARN:
        printf("%s%-5s", ansi_fg_yellow(), "WARN");
        break;

    case ERROR:
        printf("%s%-5s", ansi_fg_red(), "ERROR");
        break;

    case FATAL:
        printf("%s%-5s", ansi_fg_magenta(), "FATAL");
        break;
    }
    printf("%s", ansi_reset());

    printf(" ");

    printf("%s", ansi_fg_bright_gray());
    printf("%s:%d:", filename, linenum);
    printf("%s", ansi_reset());

    printf(" ");

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");

    fflush(stdout);
}
