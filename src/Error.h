#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

typedef enum {
    SEV_FATAL,
    SEV_ERROR,
    SEV_WARNING,
    SEV_NOTE,
    SEV_HINT,
} Severity;

typedef enum {
    ERR_LEX_UNTERMINATED_STRING  = 1001,
    ERR_LEX_UNTERMINATED_CHAR    = 1002,
    ERR_LEX_UNTERMINATED_COMMENT = 1003,
    ERR_LEX_UNKNOWN_CHAR         = 1004,
    ERR_LEX_MULTICHAR_LITERAL    = 1005,

    ERR_PARSE_UNEXPECTED_TOKEN   = 2001,
    ERR_PARSE_EXPECTED_TOKEN     = 2002,
    ERR_PARSE_UNEXPECTED_EOF     = 2003,
    ERR_PARSE_ILLEGAL_OPERAND    = 2004,
    ERR_PARSE_INVALID_TYPE       = 2005,
    ERR_PARSE_MODULE_NOT_FOUND   = 2006,

    ERR_SEM_UNDECLARED_VAR       = 3001,
    ERR_SEM_DUPLICATE_VAR        = 3002,
    ERR_SEM_TYPE_MISMATCH        = 3003,
    ERR_SEM_INCOMPATIBLE_TYPES   = 3004,
    ERR_SEM_DIV_BY_ZERO          = 3005,
    ERR_SEM_NOT_ARRAY            = 3006,
    ERR_SEM_INVALID_INDEX        = 3007,
    ERR_SEM_NOT_FUNCTION         = 3008,
    ERR_SEM_ARG_COUNT_MISMATCH   = 3009,
    ERR_SEM_VOID_VALUE           = 3010,
    ERR_SEM_NO_MAIN              = 3011,
    ERR_SEM_INVALID_BREAK        = 3012,
    ERR_SEM_INVALID_CONTINUE     = 3013,
    ERR_SEM_FIELD_NOT_FOUND      = 3014,
    ERR_SEM_METHOD_NOT_FOUND     = 3015,
    ERR_SEM_CLASS_NOT_FOUND      = 3016,
    ERR_SEM_CANNOT_ASSIGN_NEG    = 3017,
    ERR_SEM_DUPLICATE_CASE       = 3018,
    ERR_SEM_RETURN_TYPE_MISMATCH = 3019,
    ERR_SEM_CONDITION_NOT_BOOL   = 3020,

    ERR_IR_NULL_AST              = 4001,

    ERR_CG_UNKNOWN_VAR           = 5001,

    ERR_INTERNAL                 = 9001,
    ERR_OUT_OF_MEMORY            = 9002,
    ERR_UNREACHABLE              = 9003,
    ERR_UNHANDLED_AST_TYPE       = 9004,
} ErrorCode;

typedef struct {
    const char *file;
    int         line;
    int         column;
} SourceLoc;

#define LOC_NONE     ((SourceLoc){NULL, 0, 0})
#define LOC(f,l,c)   ((SourceLoc){(f), (l), (c)})

typedef struct {
    const char *filename;
    const char *source_text;
} SourceFile;

typedef struct {
    bool warnings_as_errors;
    bool suppress_warnings;
    int  max_errors;
    bool use_color;
    bool show_context;
} ErrorOptions;

typedef struct {
    ErrorOptions options;

    int  error_count;
    int  warning_count;
    int  fatal_count;
    bool has_error;

    SourceFile *source_files;
    int         source_file_count;
    int         source_file_capacity;
} ErrorManager;

extern ErrorManager *error_mgr;
extern bool          has_error;

void error_init(void);
void error_init_options(ErrorOptions opts);
void error_cleanup(void);

void error_register_source(const char *filename, const char *source_text);

const char *error_get_source_line(const char *filename, int line, int *line_len);

void diagnostic(Severity sev, ErrorCode code, SourceLoc loc,
                const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 4, 5)))
#endif
    ;

void diagnostic_v(Severity sev, ErrorCode code, SourceLoc loc,
                  const char *fmt, va_list args);

#define ERROR(code, loc, ...) \
    diagnostic(SEV_ERROR, (code), (loc), __VA_ARGS__)

#define WARNING(code, loc, ...) \
    diagnostic(SEV_WARNING, (code), (loc), __VA_ARGS__)

#define FATAL(code, loc, ...) \
    diagnostic(SEV_FATAL, (code), (loc), __VA_ARGS__)

#define NOTE(loc, ...) \
    diagnostic(SEV_NOTE, 0, (loc), __VA_ARGS__)

#define HINT(loc, ...) \
    diagnostic(SEV_HINT, 0, (loc), __VA_ARGS__)

#define INTERNAL_ERROR(fmt, ...) \
    diagnostic(SEV_FATAL, ERR_INTERNAL, LOC_NONE, \
               "Internal compiler error: " fmt, ##__VA_ARGS__)

#define OUT_OF_MEMORY() \
    diagnostic(SEV_FATAL, ERR_OUT_OF_MEMORY, LOC_NONE, "Out of memory")

bool error_occurred(void);
int  error_count(void);
int  warning_count(void);

typedef enum {
    COLOR_RESET = 0,
    COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE,
    COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE,
    COLOR_BRIGHT_BLACK, COLOR_BRIGHT_RED, COLOR_BRIGHT_GREEN,
    COLOR_BRIGHT_YELLOW, COLOR_BRIGHT_BLUE, COLOR_BRIGHT_MAGENTA,
    COLOR_BRIGHT_CYAN, COLOR_BRIGHT_WHITE,
    COLOR_BG_BLACK, COLOR_BG_RED, COLOR_BG_GREEN, COLOR_BG_YELLOW,
    COLOR_BG_BLUE, COLOR_BG_MAGENTA, COLOR_BG_CYAN, COLOR_BG_WHITE,
    COLOR_BG_BRIGHT_BLACK, COLOR_BG_BRIGHT_RED, COLOR_BG_BRIGHT_GREEN,
    COLOR_BG_BRIGHT_YELLOW, COLOR_BG_BRIGHT_BLUE, COLOR_BG_BRIGHT_MAGENTA,
    COLOR_BG_BRIGHT_CYAN, COLOR_BG_BRIGHT_WHITE
} ConsoleColor;

void vprint_color(ConsoleColor color, const char *format, va_list args);
void print_color(ConsoleColor color, const char *format, ...);

#endif