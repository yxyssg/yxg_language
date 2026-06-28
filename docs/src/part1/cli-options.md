# 命令行参数说明

## 基本用法

YXG 编译器的命令行格式为：

    yxg [源文件...] [选项]

可以一次编译多个源文件：

    yxg main.yxg utils.yxg -o program.exe

## 所有参数详解

### -o：指定输出文件名

    yxg hello.yxg -o myapp.exe

默认输出文件名为 output.exe。使用 -o 可以自定义。

### -r：编译后立即运行

    yxg hello.yxg -r

编译成功后自动执行生成的程序。如果程序需要命令行参数，可以在 -r 之后传递：

    yxg hello.yxg -r -- arg1 arg2

-- 用于分隔编译器参数和程序参数。

### -a：仅生成汇编文件

    yxg hello.yxg -a

只执行编译流程的前半部分，生成 output.asm 后停止。不会调用 NASM 和 GCC 进行汇编和链接。这对于理解代码生成结果非常有用。

生成的汇编文件示例：

    section .text
    global _main
    extern _ExitProcess@4
    extern _print
    extern _SetConsoleOutputCP@4

    _main:
        push ebp
        mov ebp, esp
        sub esp, 32
        push 65001
        call _SetConsoleOutputCP@4
        finit
        push 3
        push _str0
        call _print
        add esp, 8
        xor eax, eax
        push eax
        call _ExitProcess@4

    section .data
    _str0 db `Hello, YXG!`,0

### -d：调试模式

    yxg hello.yxg -d

在调试模式下，编译器会输出额外的中间信息，包括：

- Token 列表（词法分析结果）
- AST 树结构（语法分析结果）
- IR 指令序列（中间代码）
- CFG 基本块划分
- SSA 形式及优化过程
- 寄存器分配结果

调试输出示例：

    ┌──────────────────────────────────────────────────────────────┐
    │      Chaitin-Briggs 图着色 + Bias 寄存器分配                 │
    ├──────────────────────────────────────────────────────────────┤
    │ 变量: 12   寄存器: 6   溢出: 2 (16.7%)                       │
    ├──────┬──────────────┬──────────┬────────┬────────┬──────────┤
    │ 序号 │ 变量         │ 寄存器   │ 溢出槽  │ 循环深 │ 代价     │
    ├──────┼──────────────┼──────────┼────────┼────────┼──────────┤
    │    1 │ _var0@0      │ eax      │ ebp-0  │ 0      │ 3        │
    │    2 │ _var1@0      │ ecx      │ ebp-0  │ 0      │ 5        │
    │    3 │ .t1          │ [溢出]   │ ebp-4  │ 1      │ 120      │
    └──────┴──────────────┴──────────┴────────┴────────┴──────────┘

### --version：显示版本信息

    yxg --version

输出：

    version: 1.0.0-alpha.1

### --help：显示帮助

    yxg --help

输出：

    Usage: yxg file_name [Parameter]
    Parameter:
    -o: Change output name
    -r: Compile and run
    -d: Debug mode
    -a: Compile to assembly only
    -time: Show current time
    -version: Show current version
    -help: Show this help information

### -time：显示编译耗时

    yxg hello.yxg -time

输出示例：

    Compilation complete, output file is: output.exe
    Total compilation time: 0.234567 second

## 编译流程控制

### 完整编译（默认）

    yxg hello.yxg

执行全部阶段：词法分析 → 语法分析 → 语义分析 → IR 生成 → SSA 优化 → 寄存器分配 → 代码生成 → NASM 汇编 → GCC 链接。

### 查看汇编输出

    yxg hello.yxg -a
    type output.asm

生成 output.asm 后停止。可以用任意文本编辑器查看生成的汇编代码。

### 调试编译过程

    yxg hello.yxg -d

输出每个阶段的详细信息，帮助定位问题。

### 编译并运行

    yxg hello.yxg -r

编译成功后自动运行。适合快速测试。

## 多文件编译

YXG 支持两种方式组织多文件项目：

### 方式一：命令行指定多个文件

    yxg main.yxg math.yxg string.yxg -o app.exe

所有文件会被依次解析，合并为一个编译单元。

### 方式二：使用 import 语句

main.yxg：

    import "math.yxg";
    import "string.yxg";

    int main() {
        float r = math.sqrt(16.0);
        string s = string.toUpper("hello");
        print(r, s);
        return 0;
    }

编译时只需要指定主文件：

    yxg main.yxg -o app.exe

编译器会自动查找并编译 import 引用的模块。模块搜索路径包括：

1. 当前目录
2. lib/ 目录（相对于编译器所在位置）
3. YXG_PATH 环境变量指定的目录（多个路径用分号分隔）

### 设置模块搜索路径

Windows 命令提示符：

    set YXG_PATH=C:\my_yxg_libs;D:\shared_yxg

PowerShell：

    $env:YXG_PATH = "C:\my_yxg_libs;D:\shared_yxg"

Linux / MSYS2：

    export YXG_PATH=/home/user/yxg_libs:/opt/yxg_shared

## 运行时库依赖

YXG 编译器会根据源码中使用的功能，自动链接所需的运行时库：

| 源码中使用 | 自动链接的运行时库 |
|-----------|-------------------|
| print() | runtime_print.obj |
| 字符串操作 | runtime_string.obj |
| new / 类实例化 | runtime_gc.obj + runtime_memory.obj |
| 浮点比较 | runtime_float.obj |
| 数组转字符串 | runtime_array.obj |
| input() / format() / 系统调用 | runtime_system.obj |
| 数组越界检查 | runtime_bounds.obj |

编译器通过追踪 IR 中的指令类型来判断需要哪些运行时库。例如，遇到 IR_PRINT 指令时会设置 has_print = true，最终在链接阶段加入 runtime_print.obj。

## 退出码

| 退出码 | 含义 |
|--------|------|
| 0 | 编译成功 |
| 1 | 编译失败（有错误） |

编译过程中的警告不会导致非零退出码，但错误会。

## 继续阅读

掌握了命令行参数后，下一章将开始系统学习 YXG 的基本类型系统。