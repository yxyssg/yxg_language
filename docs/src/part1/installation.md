# 安装与构建

## 环境要求

YXG 编译器目前仅支持 Windows 平台，需要以下工具链：

| 工具 | 最低版本 | 用途 |
|------|----------|------|
| GCC (MinGW) | 8.0+ | 编译 C 源码、链接目标文件 |
| NASM | 2.15+ | 汇编生成的 .asm 文件 |
| GNU Make | 4.0+ | 自动化构建 |
| Git | 任意版本 | 获取源码 |

### 安装 MinGW

推荐使用 MSYS2 安装，它提供了完整的类 Unix 环境：

1. 从 https://www.msys2.org/ 下载安装 MSYS2
2. 打开 MSYS2 终端（UCRT64 或 MINGW64），执行：

    pacman -Syu
    pacman -S mingw-w64-ucrt-x86_64-gcc
    pacman -S mingw-w64-ucrt-x86_64-nasm
    pacman -S make
    pacman -S git

3. 将 MSYS2 的 bin 目录添加到系统 PATH：

    C:\msys64\ucrt64\bin

4. 验证安装：

    gcc --version
    nasm --version
    make --version

### 手动安装各组件

如果不想用 MSYS2，也可以分别安装：

- MinGW-w64：从 https://www.mingw-w64.org/ 下载，选择 i686 或 x86_64 版本
- NASM：从 https://www.nasm.us/ 下载安装包
- Make：从 https://gnuwin32.sourceforge.net/packages/make.htm 获取

确保所有工具的 bin 目录都在 PATH 中。

## 获取源码

    git clone https://github.com/yxyssg/yxg_language.git
    cd yxg

目录结构概览：

    yxg_language/
    ├── src/
    │   ├── main.c              # 主程序入口
    │   ├── Lexer.c             # 词法分析器
    │   ├── parser.c            # 语法分析器
    │   ├── Semantics.c         # 语义分析
    │   ├── IR.c                # 中间代码生成
    │   ├── CFG.c               # 控制流图构建
    │   ├── SSA.c               # SSA 构造与优化
    │   ├── regmap.c            # 寄存器分配
    │   ├── Codegen_win32.c     # x86 代码生成
    │   ├── Token.c             # Token 定义与工具函数
    │   ├── Error.c             # 错误报告系统
    │   ├── YString.c           # 动态字符串库
    │   └── *.h                 # 头文件
    ├── runtime/                # 运行时汇编库
    │   ├── runtime_print.asm
    │   ├── runtime_string.asm
    │   ├── runtime_memory.asm
    │   ├── runtime_gc.asm
    │   ├── runtime_float.asm
    │   ├── runtime_array.asm
    │   ├── runtime_system.asm
    │   └── runtime_bounds.asm
    ├── lib/                    # YXG 标准库
    │   ├── Math.yxg
    │   ├── String.yxg
    │   └── System.yxg
    ├── examples/               # 示例程序
    ├── Makefile                # 构建脚本
    └── docs/                   # 本文档

## 编译编译器

### 一键构建

项目提供了 Makefile，在项目根目录执行：

    make

这会依次完成：
1. 编译所有 .c 文件为目标文件
2. 汇编所有运行时 .asm 文件
3. 链接生成 yxg.exe

### 分步构建

如果需要调试，可以分步执行：

    # 第一步：编译 C 源码
    gcc -m32 -g -c src/main.c -o build/main.o
    gcc -m32 -g -c src/Lexer.c -o build/Lexer.o
    gcc -m32 -g -c src/parser.c -o build/parser.o
    gcc -m32 -g -c src/Semantics.c -o build/Semantics.o
    gcc -m32 -g -c src/IR.c -o build/IR.o
    gcc -m32 -g -c src/CFG.c -o build/CFG.o
    gcc -m32 -g -c src/SSA.c -o build/SSA.o
    gcc -m32 -g -c src/regmap.c -o build/regmap.o
    gcc -m32 -g -c src/Codegen_win32.c -o build/Codegen_win32.o
    gcc -m32 -g -c src/Token.c -o build/Token.o
    gcc -m32 -g -c src/Error.c -o build/Error.o
    gcc -m32 -g -c src/YString.c -o build/YString.o

    # 第二步：汇编运行时库
    nasm -g -f win32 -O0 runtime/runtime_print.asm -o runtime_print.obj
    nasm -g -f win32 -O0 runtime/runtime_string.asm -o runtime_string.obj
    nasm -g -f win32 -O0 runtime/runtime_memory.asm -o runtime_memory.obj
    nasm -g -f win32 -O0 runtime/runtime_gc.asm -o runtime_gc.obj
    nasm -g -f win32 -O0 runtime/runtime_float.asm -o runtime_float.obj
    nasm -g -f win32 -O0 runtime/runtime_array.asm -o runtime_array.obj
    nasm -g -f win32 -O0 runtime/runtime_system.asm -o runtime_system.obj
    nasm -g -f win32 -O0 runtime/runtime_bounds.asm -o runtime_bounds.obj

    # 第三步：链接
    gcc -m32 -g build/*.o build/*.obj -o yxg.exe

### 编译选项说明

| 选项 | 含义 |
|------|------|
| -m32 | 生成 32 位代码（YXG 编译器目前仅支持 x86） |
| -g | 包含调试信息 |
| -O0 | 关闭优化（编译器自身不需要优化，便于调试） |
| -f win32 | NASM 输出 Win32 目标格式 |

## 验证安装

编译完成后，测试编译器是否正常工作：

    yxg --version

预期输出：

    version: 1.0.0-alpha.1

查看帮助信息：

    yxg --help

预期输出：

    Usage: yxg file_name [Parameter]
    Parameter:
    -o: Change output name
    -r: Compile and run
    -d: Debug mode
    -a: Compile to assembly only
    -time: Show current time
    -version: Show current version
    -help: Show this help information

## 编译并运行第一个程序

创建一个测试文件 hello.yxg：

    int main() {
        print "Hello, YXG!";
        return 0;
    }

编译运行：

    yxg hello.yxg -r

预期输出：

    Hello, YXG!
    Compilation complete, output file is: output.exe
    Total compilation time: 0.123456 second

### 命令行参数详解

    yxg hello.yxg              # 编译为 output.exe
    yxg hello.yxg -o myapp.exe # 指定输出文件名
    yxg hello.yxg -r           # 编译后立即运行
    yxg hello.yxg -a           # 只生成汇编文件 output.asm，不链接
    yxg hello.yxg -d           # 调试模式，打印更多信息

### 编译流程

当你执行 yxg hello.yxg 时，编译器内部经历了以下步骤：

    hello.yxg
       │
       ▼
    [词法分析]  → Token 序列
       │
       ▼
    [语法分析]  → AST 抽象语法树
       │
       ▼
    [语义分析]  → 标注类型的 AST + 符号表
       │
       ▼
    [IR 生成]   → IR 中间表示链表
       │
       ▼
    [CFG 构建]  → 基本块 + 控制流图
       │
       ▼
    [SSA 构造]  → SSA 形式 + 优化 Pass
       │
       ▼
    [寄存器分配] → 寄存器映射表
       │
       ▼
    [代码生成]  → output.asm (NASM 汇编)
       │
       ▼
    [NASM 汇编] → output.obj
       │
       ▼
    [GCC 链接]  → output.exe (含运行时库)

可以使用 -a 参数停在汇编阶段，查看生成的 output.asm 来理解代码生成结果。

## 常见问题

### Q: 提示 "nasm 不是内部或外部命令"

NASM 未安装或未添加到 PATH。重新运行 NASM 安装程序，确保勾选"添加到 PATH"选项，或手动将 NASM 安装目录添加到系统环境变量。

### Q: 提示 "gcc 不是内部或外部命令"

MinGW 未安装或未添加到 PATH。检查 C:\msys64\ucrt64\bin 是否在 PATH 中。

### Q: 链接时提示 undefined reference

通常是运行时库的 .obj 文件未正确生成。检查 runtime/ 目录下的 .asm 文件是否完整，重新执行 nasm 汇编步骤。

### Q: 编译时提示 "无法解析的外部符号 _ExitProcess@4"

链接时缺少 Windows API 库。确保使用 MinGW 的 gcc 进行链接，它会自动链接 kernel32 等系统库。

### Q: 生成的 exe 无法运行，提示缺少 DLL

YXG 编译器生成的是 32 位程序，需要 32 位运行时。在 64 位 Windows 上通常可以直接运行。如果提示缺少 libgcc_s_dw2-1.dll，将 MinGW 的 bin 目录添加到 PATH 即可。

## 继续阅读

环境搭建完成后，下一章我们将编写第一个 YXG 程序，深入了解语言的基本语法。