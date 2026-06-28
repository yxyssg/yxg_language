#include "Error.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

ErrorManager *error_mgr = NULL;
bool          has_error = false;

static bool terminal_supports_color(void) {
#ifdef _WIN32
    return true;
#else
    static int cached = -1;
    if (cached == -1) {
        const char *term = getenv("TERM");
        if (!term) { cached = 0; return false; }
        cached = (strstr(term, "color") || strstr(term, "xterm") ||
                  strstr(term, "screen") || strstr(term, "256color"))
                     ? 1
                     : 0;
    }
    return cached == 1;
#endif
}

typedef enum {
    TCOL_RESET = 0,
    TCOL_RED, TCOL_GREEN, TCOL_YELLOW, TCOL_BLUE,
    TCOL_MAGENTA, TCOL_CYAN, TCOL_WHITE,
    TCOL_BRIGHT_RED,
    TCOL_BOLD,
} TermColor;

#ifdef _WIN32
static WORD win_color(TermColor c) {
    switch (c) {
    case TCOL_RED:        return FOREGROUND_RED;
    case TCOL_GREEN:      return FOREGROUND_GREEN;
    case TCOL_YELLOW:     return FOREGROUND_RED | FOREGROUND_GREEN;
    case TCOL_BLUE:       return FOREGROUND_BLUE;
    case TCOL_MAGENTA:    return FOREGROUND_RED | FOREGROUND_BLUE;
    case TCOL_CYAN:       return FOREGROUND_GREEN | FOREGROUND_BLUE;
    case TCOL_WHITE:      return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    case TCOL_BRIGHT_RED: return FOREGROUND_RED | FOREGROUND_INTENSITY;
    case TCOL_BOLD:       return FOREGROUND_INTENSITY;
    default:              return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }
}
#else
static const char *ansi_color(TermColor c) {
    switch (c) {
    case TCOL_RED:        return "\033[31m";
    case TCOL_GREEN:      return "\033[32m";
    case TCOL_YELLOW:     return "\033[33m";
    case TCOL_BLUE:       return "\033[34m";
    case TCOL_MAGENTA:    return "\033[35m";
    case TCOL_CYAN:       return "\033[36m";
    case TCOL_WHITE:      return "\033[37m";
    case TCOL_BRIGHT_RED: return "\033[91m";
    case TCOL_BOLD:       return "\033[1m";
    default:              return "\033[0m";
    }
}
#endif

static FILE *err_out(void) { return stderr; }

static void set_tcolor(TermColor c) {
    if (!error_mgr || !error_mgr->options.use_color) return;
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), win_color(c));
#else
    fprintf(err_out(), "%s", ansi_color(c));
#endif
}

static void reset_tcolor(void) {
    if (!error_mgr || !error_mgr->options.use_color) return;
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE),
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    fprintf(err_out(), "\033[0m");
#endif
}

static const char *code_str(ErrorCode code) {
    switch (code) {
    case ERR_LEX_UNTERMINATED_STRING:  return "E1001";
    case ERR_LEX_UNTERMINATED_CHAR:    return "E1002";
    case ERR_LEX_UNTERMINATED_COMMENT: return "E1003";
    case ERR_LEX_UNKNOWN_CHAR:         return "E1004";
    case ERR_LEX_MULTICHAR_LITERAL:    return "E1005";
    case ERR_PARSE_UNEXPECTED_TOKEN:   return "E2001";
    case ERR_PARSE_EXPECTED_TOKEN:     return "E2002";
    case ERR_PARSE_UNEXPECTED_EOF:     return "E2003";
    case ERR_PARSE_ILLEGAL_OPERAND:    return "E2004";
    case ERR_PARSE_INVALID_TYPE:       return "E2005";
    case ERR_PARSE_MODULE_NOT_FOUND:   return "E2006";
    case ERR_SEM_UNDECLARED_VAR:       return "E3001";
    case ERR_SEM_DUPLICATE_VAR:        return "E3002";
    case ERR_SEM_TYPE_MISMATCH:        return "E3003";
    case ERR_SEM_INCOMPATIBLE_TYPES:   return "E3004";
    case ERR_SEM_DIV_BY_ZERO:          return "E3005";
    case ERR_SEM_NOT_ARRAY:            return "E3006";
    case ERR_SEM_INVALID_INDEX:        return "E3007";
    case ERR_SEM_NOT_FUNCTION:         return "E3008";
    case ERR_SEM_ARG_COUNT_MISMATCH:   return "E3009";
    case ERR_SEM_VOID_VALUE:           return "E3010";
    case ERR_SEM_NO_MAIN:              return "E3011";
    case ERR_SEM_INVALID_BREAK:        return "E3012";
    case ERR_SEM_INVALID_CONTINUE:     return "E3013";
    case ERR_SEM_FIELD_NOT_FOUND:      return "E3014";
    case ERR_SEM_METHOD_NOT_FOUND:     return "E3015";
    case ERR_SEM_CLASS_NOT_FOUND:      return "E3016";
    case ERR_SEM_CANNOT_ASSIGN_NEG:    return "E3017";
    case ERR_SEM_DUPLICATE_CASE:       return "E3018";
    case ERR_SEM_RETURN_TYPE_MISMATCH: return "E3019";
    case ERR_SEM_CONDITION_NOT_BOOL:   return "E3020";
    case ERR_IR_NULL_AST:              return "E4001";
    case ERR_CG_UNKNOWN_VAR:           return "E5001";
    case ERR_INTERNAL:                 return "E9001";
    case ERR_OUT_OF_MEMORY:            return "E9002";
    case ERR_UNREACHABLE:              return "E9003";
    case ERR_UNHANDLED_AST_TYPE:       return "E9004";
    default:                           return "E????";
    }
}

static const char *sev_label(Severity s) {
    switch (s) {
    case SEV_FATAL:   return "fatal error";
    case SEV_ERROR:   return "error";
    case SEV_WARNING: return "warning";
    case SEV_NOTE:    return "note";
    case SEV_HINT:    return "hint";
    default:          return "message";
    }
}

static TermColor sev_color(Severity s) {
    switch (s) {
    case SEV_FATAL:   return TCOL_BRIGHT_RED;
    case SEV_ERROR:   return TCOL_RED;
    case SEV_WARNING: return TCOL_YELLOW;
    case SEV_NOTE:    return TCOL_CYAN;
    case SEV_HINT:    return TCOL_GREEN;
    default:          return TCOL_WHITE;
    }
}

void error_init(void) {
    if (error_mgr) return;

    error_mgr = calloc(1, sizeof(ErrorManager));
    if (!error_mgr) {
        fprintf(stderr, "FATAL: cannot allocate error manager\n");
        exit(EXIT_FAILURE);
    }

    error_mgr->options.max_errors        = 100;
    error_mgr->options.warnings_as_errors = false;
    error_mgr->options.suppress_warnings  = false;
    error_mgr->options.use_color          = terminal_supports_color();
    error_mgr->options.show_context       = true;

    error_mgr->source_file_capacity = 16;
    error_mgr->source_files = calloc(
        (size_t)error_mgr->source_file_capacity, sizeof(SourceFile));

    has_error = false;
}

void error_init_options(ErrorOptions opts) {
    if (!error_mgr) error_init();
    error_mgr->options = opts;
}

void error_cleanup(void) {
    if (!error_mgr) return;
    free(error_mgr->source_files);
    free(error_mgr);
    error_mgr = NULL;
    has_error = false;
}

void error_register_source(const char *filename, const char *source_text) {
    if (!error_mgr) error_init();
    if (!filename || !source_text) return;

    for (int i = 0; i < error_mgr->source_file_count; i++) {
        if (error_mgr->source_files[i].filename &&
            strcmp(error_mgr->source_files[i].filename, filename) == 0)
            return;
    }

    if (error_mgr->source_file_count >= error_mgr->source_file_capacity) {
        error_mgr->source_file_capacity *= 2;
        error_mgr->source_files = realloc(error_mgr->source_files,
            sizeof(SourceFile) * (size_t)error_mgr->source_file_capacity);
    }

    error_mgr->source_files[error_mgr->source_file_count].filename    = filename;
    error_mgr->source_files[error_mgr->source_file_count].source_text = source_text;
    error_mgr->source_file_count++;
}

const char *error_get_source_line(const char *filename, int line, int *line_len) {
    if (!error_mgr || !filename || line < 1) return NULL;

    for (int i = 0; i < error_mgr->source_file_count; i++) {
        if (!error_mgr->source_files[i].filename) continue;
        if (strcmp(error_mgr->source_files[i].filename, filename) != 0) continue;

        const char *src = error_mgr->source_files[i].source_text;
        if (!src) return NULL;

        int cur_line = 1;
        const char *p = src;
        while (cur_line < line && *p) {
            if (*p == '\n') cur_line++;
            p++;
        }
        if (cur_line != line) return NULL;

        const char *start = p;
        while (*p && *p != '\n' && *p != '\r') p++;

        if (line_len) *line_len = (int)(p - start);
        return start;
    }
    return NULL;
}

void diagnostic_v(Severity sev, ErrorCode code, SourceLoc loc,
                  const char *fmt, va_list args) {
    if (!error_mgr) error_init();

    if (sev == SEV_WARNING && error_mgr->options.suppress_warnings)
        return;

    if (sev == SEV_WARNING && error_mgr->options.warnings_as_errors)
        sev = SEV_ERROR;

    if ((sev == SEV_ERROR || sev == SEV_FATAL) &&
        error_mgr->error_count >= error_mgr->options.max_errors) {
        if (error_mgr->error_count == error_mgr->options.max_errors) {
            error_mgr->error_count++;
            set_tcolor(TCOL_BRIGHT_RED);
            fprintf(err_out(),
                    "Too many errors, stopping compilation.\n");
            reset_tcolor();
        }
        return;
    }

    switch (sev) {
    case SEV_FATAL:
        error_mgr->fatal_count++;
        error_mgr->has_error = true;
        has_error = true;
        break;
    case SEV_ERROR:
        error_mgr->error_count++;
        error_mgr->has_error = true;
        has_error = true;
        break;
    case SEV_WARNING:
        error_mgr->warning_count++;
        break;
    default:
        break;
    }

    if (loc.file) {
        set_tcolor(TCOL_WHITE);
        if (loc.line > 0)
            fprintf(err_out(), "%s:%d:%d: ", loc.file, loc.line, loc.column);
        else
            fprintf(err_out(), "%s: ", loc.file);
    }

    set_tcolor(sev_color(sev));
    fprintf(err_out(), "%s", sev_label(sev));

    if (code != 0) {
        set_tcolor(TCOL_WHITE);
        fprintf(err_out(), "[%s]", code_str(code));
    }

    set_tcolor(TCOL_WHITE);
    fprintf(err_out(), ": ");
    reset_tcolor();
    vfprintf(err_out(), fmt, args);
    fprintf(err_out(), "\n");

    if (loc.file && loc.line > 0 && error_mgr->options.show_context) {
        int line_len = 0;
        const char *line_text = error_get_source_line(loc.file, loc.line, &line_len);
        if (line_text && line_len > 0) {
            set_tcolor(TCOL_WHITE);
            fprintf(err_out(), " %5d | ", loc.line);
            reset_tcolor();
            for (int i = 0; i < line_len; i++) {
                if (line_text[i] == '\t')
                    fputs("    ", err_out());
                else
                    fputc(line_text[i], err_out());
            }
            fprintf(err_out(), "\n");

            fprintf(err_out(), "       | ");
            int visual_col = 1;
            for (int i = 0; i < loc.column - 1 && i < line_len; i++) {
                if (line_text[i] == '\t') {
                    fputs("    ", err_out());
                    visual_col += 4;
                } else {
                    fputc(' ', err_out());
                    visual_col++;
                }
            }
            set_tcolor(sev_color(sev));
            fprintf(err_out(), "^");
            reset_tcolor();
            fprintf(err_out(), "\n");
        }
    }

    if (sev == SEV_FATAL) {
        fprintf(err_out(), "\nCompilation aborted due to fatal error.\n");
        fflush(err_out());
        exit(EXIT_FAILURE);
    }
}

void diagnostic(Severity sev, ErrorCode code, SourceLoc loc,
                const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    diagnostic_v(sev, code, loc, fmt, args);
    va_end(args);
}

bool error_occurred(void) { return error_mgr ? error_mgr->has_error : false; }
int  error_count(void)    { return error_mgr ? error_mgr->error_count : 0; }
int  warning_count(void)  { return error_mgr ? error_mgr->warning_count : 0; }

static TermColor legacy_to_termcolor(ConsoleColor c) {
    switch (c) {
    case COLOR_RED:          return TCOL_RED;
    case COLOR_GREEN:        return TCOL_GREEN;
    case COLOR_YELLOW:       return TCOL_YELLOW;
    case COLOR_BLUE:         return TCOL_BLUE;
    case COLOR_MAGENTA:      return TCOL_MAGENTA;
    case COLOR_CYAN:         return TCOL_CYAN;
    case COLOR_WHITE:        return TCOL_WHITE;
    case COLOR_BRIGHT_RED:   return TCOL_BRIGHT_RED;
    default:                 return TCOL_WHITE;
    }
}

void vprint_color(ConsoleColor color, const char *format, va_list args) {
    if (color == COLOR_BRIGHT_RED || color == COLOR_RED) {
        if (!error_mgr) error_init();
        error_mgr->has_error = true;
        has_error = true;
    }

    set_tcolor(legacy_to_termcolor(color));
    vfprintf(err_out(), format, args);
    reset_tcolor();
}

void print_color(ConsoleColor color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprint_color(color, format, args);
    va_end(args);
}