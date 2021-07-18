#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
} log_level;

#define DECLARE_ANSI_SGR(name) \
    const char *ansi_##name();

DECLARE_ANSI_SGR(reset);

DECLARE_ANSI_SGR(fg_gray);
DECLARE_ANSI_SGR(fg_red);
DECLARE_ANSI_SGR(fg_green);
DECLARE_ANSI_SGR(fg_yellow);
DECLARE_ANSI_SGR(fg_blue);
DECLARE_ANSI_SGR(fg_magenta);
DECLARE_ANSI_SGR(fg_cyan);
DECLARE_ANSI_SGR(fg_white);
DECLARE_ANSI_SGR(bg_gray);
DECLARE_ANSI_SGR(bg_red);
DECLARE_ANSI_SGR(bg_green);
DECLARE_ANSI_SGR(bg_yellow);
DECLARE_ANSI_SGR(bg_blue);
DECLARE_ANSI_SGR(bg_magenta);
DECLARE_ANSI_SGR(bg_cyan);
DECLARE_ANSI_SGR(bg_white);

DECLARE_ANSI_SGR(fg_bright_gray);
DECLARE_ANSI_SGR(fg_bright_red);
DECLARE_ANSI_SGR(fg_bright_green);
DECLARE_ANSI_SGR(fg_bright_yellow);
DECLARE_ANSI_SGR(fg_bright_blue);
DECLARE_ANSI_SGR(fg_bright_magenta);
DECLARE_ANSI_SGR(fg_bright_cyan);
DECLARE_ANSI_SGR(fg_bright_white);
DECLARE_ANSI_SGR(bg_bright_gray);
DECLARE_ANSI_SGR(bg_bright_red);
DECLARE_ANSI_SGR(bg_bright_green);
DECLARE_ANSI_SGR(bg_bright_yellow);
DECLARE_ANSI_SGR(bg_bright_blue);
DECLARE_ANSI_SGR(bg_bright_magent);
DECLARE_ANSI_SGR(bg_bright_cyan);
DECLARE_ANSI_SGR(bg_bright_white);

#undef DECLARE_ANSI_SGR

void idobata_log_level(log_level level);
void idobata_log_colored(int flag);
void idobata_log(log_level level, const char *filename, int linenum, const char *fmt, ...);

#define LOG(level, ...) idobata_log(level, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...)  idobata_log(FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)  idobata_log(ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)   idobata_log(WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)   idobata_log(INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...)  idobata_log(DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_TRACE(...)  idobata_log(TRACE, __FILE__, __LINE__, __VA_ARGS__)

#endif
