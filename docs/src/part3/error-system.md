# 错误处理系统

## 概述

YXG 编译器有一套完整的错误报告系统，贯穿编译的各个阶段。它能够精确报告错误位置、错误码、错误级别，并显示源代码上下文。

## 错误级别

系统定义了五种严重级别：

| 级别 | 枚举值 | 含义 | 行为 |
|------|--------|------|------|
| 致命错误 | SEV_FATAL | 无法继续编译 | 立即终止程序 |
| 错误 | SEV_ERROR | 编译失败 | 记录错误，继续检查 |
| 警告 | SEV_WARNING | 可能有问题 | 记录警告，继续编译 |
| 提示 | SEV_NOTE | 附加信息 | 仅输出，不影响编译 |
| 建议 | SEV_HINT | 修复建议 | 仅输出，不影响编译 |

## 错误码体系

每个错误都有唯一的错误码，按阶段分类：

| 阶段 | 错误码范围 | 示例 |
|------|-----------|------|
| 词法分析 | E1001 ~ E1005 | E1001: 未终止的字符串 |
| 语法分析 | E2001 ~ E2006 | E2001: 意外的 Token |
| 语义分析 | E3001 ~ E3020 | E3001: 未声明的变量 |
| IR 生成 | E4001 | E4001: 空 AST 节点 |
| 代码生成 | E5001 | E5001: 未知变量 |
| 内部错误 | E9001 ~ E9004 | E9001: 内部编译器错误 |

## 错误报告格式

编译器输出包含文件名、行号、列号、错误级别、错误码和描述信息：

    hello.yxg:5:10: error[E3001]: variable 'x' is not declared
        5 |     print x;
          |           ^

如果启用了颜色输出（默认），不同级别会显示不同颜色：

- 红色：致命错误和普通错误
- 黄色：警告
- 青色：提示
- 绿色：建议

## 源代码上下文

当错误发生在某一行时，编译器会显示该行的源代码并在错误位置标注 ^ 符号：

    hello.yxg:3:5: error[E3003]: type mismatch: expected 'int', got 'string'
        3 |     int x = "hello";
          |     ^^^

这需要编译器在词法分析阶段记录源代码和位置信息。

## 错误管理器的结构

定义在 Error.h：

    typedef struct {
        const char* filename;       // 源文件名
        const char* source_text;    // 源文件内容
    } SourceFile;

    typedef struct {
        bool warnings_as_errors;    // 警告视为错误
        bool suppress_warnings;     // 抑制所有警告
        int max_errors;             // 最大错误数（超过后停止）
        bool use_color;             // 是否使用颜色输出
        bool show_context;          // 是否显示源代码上下文
    } ErrorOptions;

    typedef struct {
        ErrorOptions options;
        int error_count;            // 错误计数
        int warning_count;          // 警告计数
        int fatal_count;            // 致命错误计数
        bool has_error;             // 是否有错误
        SourceFile* source_files;   // 已注册的源文件
        int source_file_count;      // 源文件数量
    } ErrorManager;

## 使用方式

### 报告错误

使用预定义的宏来报告不同级别的诊断信息：

    // 报告错误
    ERROR(ERR_SEM_UNDECLARED_VAR,
          LOC("hello.yxg", 5, 10),
          "variable '%s' is not declared", "x");

    // 报告警告
    WARNING(ERR_SEM_TYPE_MISMATCH,
            LOC("hello.yxg", 3, 5),
            "type mismatch: expected '%s', got '%s'",
            "int", "string");

    // 致命错误（立即终止）
    FATAL(ERR_INTERNAL,
          LOC_NONE,
          "cannot allocate memory for symbol table");

    // 附加提示
    NOTE(LOC("hello.yxg", 2, 1),
         "variable 'x' was declared here");

    // 修复建议
    HINT(LOC("hello.yxg", 5, 10),
         "did you mean 'y'?");

### 内部错误

编译器内部异常使用专门的宏：

    // 内部编译器错误
    INTERNAL_ERROR("IR: unhandled AST type %d", node->type);

    // 内存不足
    OUT_OF_MEMORY();

### 位置信息

使用 LOC 宏构造源码位置：

    #define LOC_NONE     ((SourceLoc){NULL, 0, 0})
    #define LOC(f,l,c)   ((SourceLoc){(f), (l), (c)})

## 错误恢复策略

不同阶段的错误恢复策略不同：

### 词法分析阶段

遇到无法识别的字符时，跳过该字符继续分析：

    // 遇到非法字符 @
    ERROR(ERR_LEX_UNKNOWN_CHAR, loc, "unexpected character '@'");
    // 跳过该字符，继续词法分析

### 语法分析阶段

遇到语法错误时，使用恐慌模式恢复：跳过后续 Token 直到找到同步点（通常是分号、右花括号等）：

    while (!parser_current_is_type(parser, TOKEN_SEMICOLON) &&
           !parser_current_is_type(parser, TOKEN_EOF) &&
           !is_Modifier(parser)) {
        parser_advance(parser);
    }
    if (parser_current_is_type(parser, TOKEN_SEMICOLON))
        parser_advance(parser);

### 语义分析阶段

遇到类型错误时，将节点类型标记为 DATA_ERROR 并继续分析，避免级联错误：

    if (!types_compatible(declared, actual)) {
        ERROR(ERR_SEM_TYPE_MISMATCH, loc, "...");
        node->dataType = DATA_ERROR;  // 标记为错误，抑制后续级联错误
    }

## 最大错误数限制

默认最多报告 100 个错误。超过后编译器停止报告并退出：

    if (error_mgr->error_count >= error_mgr->options.max_errors) {
        set_tcolor(TCOL_BRIGHT_RED);
        fprintf(err_out(), "Too many errors, stopping compilation.\n");
        reset_tcolor();
        exit(EXIT_FAILURE);
    }

## 颜色输出

编译器自动检测终端是否支持颜色。在 Windows 上默认启用，在 Linux/Mac 上检测 TERM 环境变量：

    static bool terminal_supports_color(void) {
        const char* term = getenv("TERM");
        if (!term) return false;
        return (strstr(term, "color") || strstr(term, "xterm"));
    }

可通过编译选项关闭颜色输出。

## 常见错误示例

### 词法错误

    // 未终止的字符串
    string s = "Hello, World!;
    // error[E1001]: unterminated string literal

    // 未终止的注释
    /* This comment never ends
    // error[E1003]: unterminated multi-line comment

### 语法错误

    // 缺少分号
    int x = 10
    // error[E2001]: expected ';' before 'int'

    // 意外的 Token
    int 123 = 456;
    // error[E2001]: unexpected token '123'

### 语义错误

    // 未声明的变量
    print x;
    // error[E3001]: variable 'x' is not declared

    // 类型不匹配
    int x = "hello";
    // error[E3003]: type mismatch: expected 'int', got 'string'

    // 重复声明
    int x = 1;
    int x = 2;
    // error[E3002]: duplicate variable 'x'

    // 除零错误
    int z = 10 / 0;
    // error[E3005]: division by zero

    // 条件不是布尔类型
    if 42 { }
    // error[E3020]: condition must be boolean, got 'int'

## 继续阅读

下一部分将深入词法分析器的实现细节。