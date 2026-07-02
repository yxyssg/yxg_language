# YXG Compiler / YXG 编译器

[![License](https://img.shields.io/badge/license-Apache-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green.svg)](https://github.com/yxyssg/yxg_language/releases)

Want to read the document? Click here

想阅读文档？点击这里
https://yxyssg.github.io/yxg_language/

**YXG** is a statically-typed C-like language that compiles to native Windows x86 executables. The compiler is entirely hand-written in C without Flex/Bison/LLVM, making it ideal for learning the full compilation pipeline.  
**YXG** 是一门静态类型的类 C 语言，编译到原生 Windows x86 可执行文件。编译器完全由 C 语言手写，不依赖 Flex/Bison/LLVM，适合学习编译原理全流程。

## Features / 特性

### Language Features / 语言特性
- **Basic types / 基本类型**: `int`, `uint`, `float`, `ufloat`, `char`, `string`, `bool`, `void`
- **Control flow / 控制流**: `if`/`else if`/`else`, `switch`/`case`/`default`, `while`, `do-while`, `for`, `for-each`
- **Functions / 函数**: polymorphic functions, methods, constructors, function overloading / 多态函数、方法、构造函数、函数重载
- **Arrays / 数组**: multi-dimensional, dynamic, subscript access, `.length`, `new` allocation / 多维数组、动态数组、下标访问、`.length`、`new` 分配
- **Classes & Objects / 类与对象**: single/multiple inheritance, private/protected/public fields, methods, constructors, `this`, `new` / 单/多继承、私有/保护/公有字段、方法、构造函数、`this`、`new`
- **Enums / 枚举**: explicit/implicit assignment / 支持显式/隐式赋值
- **Module system / 模块系统**: `import` statement, path-based search / `import` 语句，按路径搜索
- **Compound assignment / 复合赋值**: `+=`, `-=`, `*=`, `/=`, `%=`, `++`, `--`
- **Bitwise operations / 位运算**: `&`, `|`, `~`
- **Short-circuit logic / 逻辑短路**: `&&`, `||`
- **Output statement / 输出语句**: `print` accepts any number of basic-type arguments, space-separated / `print` 可接受任意数量、任意基本类型的参数，以空格分隔

### Compiler Features / 编译器特性
- 🔍 Hand-written lexer (state machine) / 手写词法分析器（状态机）
- 🌳 Recursive descent parser with error recovery / 递归下降语法分析器（带错误恢复）
- 📝 Full semantic analysis (type checking, scoping, symbol table) / 完整语义分析（类型检查、作用域、符号表）
- 🔧 Three-address code IR (30+ instructions) / 三地址码 IR（含 30+ 种指令）
- ⚡ SSA optimizer (constant folding, constant propagation, copy propagation, dead code elimination) / SSA 优化器（常量折叠、常量传播、复制传播、死代码消除）
- 🎯 x86-32 Windows code generation (NASM assembly, Chaitin-Briggs graph-coloring register allocation) / x86-32 Windows 代码生成（NASM 汇编，含 Chaitin-Briggs 图着色寄存器分配）
- 🗑️ Built-in garbage collector (mark-sweep, experimental) / 内置垃圾回收器（标记-清除，实验阶段）
- 🎨 GCC/Clang-style colored error output / GCC/Clang 风格彩色错误输出

## Quick Start / 快速开始

### Requirements / 环境要求

| Tool / 工具 | Purpose / 用途 | Download / 下载 |
|-------------|----------------|-----------------|
| GCC (MinGW) | C compiler + linker / C 编译器 + 链接器 | [MinGW-w64](https://www.mingw-w64.org/) |
| NASM        | Assembler / 汇编器 | [NASM](https://www.nasm.us/) |
| Windows     | Target platform / 目标平台 | x86 or x64 (compiles to 32-bit) / x86 或 x64（编译为 32 位） |

Make sure both `gcc` and `nasm` are in your `PATH`:  
确保 `gcc` 和 `nasm` 都在 `PATH` 中：

    gcc --version
    nasm --version

### Building the Compiler / 编译编译器

    git clone https://github.com/yxyssg/yxg_language.git
    cd yxg

    # Windows (MinGW)
    gcc -m32 -O2 -o yxg.exe main.c lexer.c token.c \
     parser.c semantics.c ir.c codegen_win32.c \
     regmap.c CFG.c SSA.c error.c \
     ystring.c -I.

### Building the Runtime (first time) / 构建运行时库（首次使用）

The generated executables require the runtime object files. Assemble them with NASM:  
生成的可执行文件需要链接运行时对象文件，请先用 NASM 汇编它们：

    nasm -f win32 runtime_print.asm -o runtime_print.obj
    nasm -f win32 runtime_string.asm -o runtime_string.obj
    nasm -f win32 runtime_memory.asm -o runtime_memory.obj
    nasm -f win32 runtime_gc.asm -o runtime_gc.obj
    nasm -f win32 runtime_float.asm -o runtime_float.obj
    nasm -f win32 runtime_array.asm -o runtime_array.obj
    nasm -f win32 runtime_system.asm -o runtime_system.obj
    nasm -f win32 runtime_bounds.asm -o runtime_bounds.obj

### First Program / 第一个程序

Create `hello.yxg`:  
创建 `hello.yxg`：

    pub void main() {
        print "Hello, YXG!";
    }

Compile and run / 编译并运行：

    yxg hello.yxg -r

Output / 输出：

    Hello, YXG!

### More Examples / 更多示例

**Variables & Types / 变量与类型**：

    pub void main() {
        int age = 25;
        float pi = 3.14;
        string name = "YXG";
        bool is_cool = true;
        print name, " is ", age, " years old";
        print "Pi ≈ ", pi;
    }

**Arrays / 数组**：

    pub void main() {
        int[] numbers = {1, 2, 3, 4, 5};
        for int i = 0; i < numbers.length; i++ {
            print numbers[i], " ";
        }
        // foreach syntax / foreach 语法
        for int n : numbers {
            print n, " ";
        }
    }

**Classes & Inheritance / 类与继承**：

    class Animal {
        string name;
        void speak() {
            print name, " makes a sound";
        }
    }

    class Dog : Animal {
        void speak() {
            print name, " barks!";
        }
    }

    pub void main() {
        Dog dog = new Dog();
        dog.name = "Rex";
        dog.speak();   // Rex barks!
    }

## Command-line Arguments / 命令行参数

    yxg <file.yxg> [options]

| Argument / 参数 | Description / 说明 |
|-----------------|-------------------|
| -o <name>       | Output executable name (default: derived from source file, e.g. hello.exe) / 指定输出文件名（默认以源文件名为基础，如 hello.exe） |
| -r              | Run immediately after compilation / 编译后立即运行 |
| -a              | Generate assembly file only (no linking) / 只生成汇编文件（不链接） |
| -help           | Show help / 显示帮助 |
| -version        | Show version / 显示版本 |
| -time           | Show current time / 显示当前时间 |

## Project Structure / 项目结构

    yxg/
    ├── main.c              # Entry, compilation flow, command-line parsing
    │                       # 入口、编译流程、命令行解析
    ├── lexer.c/h           # Lexical analyzer (hand-written state machine)
    │                       # 词法分析器（手写状态机）
    ├── token.c/h           # Token types, keyword matching
    │                       # Token 类型、关键字匹配
    ├── parser.c/h          # Recursive descent parser (with error recovery)
    │                       # 递归下降语法分析器（带错误恢复）
    ├── ast.h               # Abstract syntax tree node definitions
    │                       # 抽象语法树节点定义
    ├── semantics.c/h       # Semantic analysis (type checking, symbol table, scoping)
    │                       # 语义分析（类型检查、符号表、作用域）
    ├── ir.c/h              # Intermediate representation generation
    │                       # 中间表示生成
    ├── regmap.c/h          # Register allocation (graph coloring)
    │                       # 寄存器分配（图着色）
    ├── CFG.c/h             # Control flow graph construction & basic block analysis
    │                       # 控制流图构建与基本块分析
    ├── SSA.c/h             # SSA construction, φ-node insertion & destruction
    │                       # SSA 形式构造、Phi 节点插入与降级
    ├── codegen.h           # Code generation interface
    │                       # 代码生成接口
    ├── codegen_win32.c     # x86-32 Windows code generation
    │                       # x86-32 Windows 代码生成
    ├── error.c/h           # Error reporting system
    │                       # 错误报告系统
    ├── ystring.c/h         # Dynamic string library
    │                       # 动态字符串库
    ├── runtime_print.asm   # Runtime: print functions
    │                       # 运行时：打印函数
    ├── runtime_string.asm  # Runtime: string operations
    │                       # 运行时：字符串操作
    ├── runtime_memory.asm  # Runtime: memory management
    │                       # 运行时：内存管理
    ├── runtime_gc.asm      # Runtime: garbage collection
    │                       # 运行时：垃圾回收
    ├── runtime_float.asm   # Runtime: floating-point comparison
    │                       # 运行时：浮点比较
    ├── runtime_array.asm   # Runtime: array to string
    │                       # 运行时：数组转字符串
    ├── runtime_system.asm  # Runtime: system calls
    │                       # 运行时：系统调用
    ├── runtime_bounds.asm  # Runtime: array bounds checking
    │                       # 运行时：数组越界检查
    ├── lib/                # Standard library (.yxg modules)
    │   │                   # 标准库（.yxg 模块）
    │   ├── yxg/
    │   │   ├── String.yxg  
    │   │   ├── Math.yxg
    │   │   ├── System.yxg
    │   └── ...

## Compilation Pipeline / 编译流程

    Source (.yxg) / 源文件 (.yxg)
        │
        ▼
    [Lexer / 词法分析] ──→ Token stream / Token 流
        │
        ▼
    [Parser / 语法分析] ──→ AST / AST 树
        │
        ▼
    [Semantic Analysis / 语义分析] ──→ Annotated AST / 带类型标注的 AST
        │
        ▼
    [IR Generation / IR 生成]  ──→ Three-address code IR / 三地址码 IR
        │
        ▼
    [CFG Construction / CFG 构建] ──→ Control flow graph (with liveness) / 控制流图（含活性分析）
        │
        ▼
    [SSA Construction / SSA 构造] ──→ SSA form + φ nodes / SSA 形式 + φ 节点
        │
        ▼
    [SSA Optimization / SSA 优化] ──→ Const/Copy prop, folding, DCE / 常量传播/折叠、复制传播、DCE
        │
        ▼
    [SSA Destruction / SSA 降级] ──→ Out-of-SSA, copy insertion / 退出 SSA，插入副本指令
        │
        ▼
    [Register Allocation / 寄存器分配] ──→ Graph coloring (Chaitin-Briggs) / 图着色（Chaitin-Briggs）
        │
        ▼
    [Code Generation / 代码生成] ──→ NASM assembly (.asm) / NASM 汇编 (.asm)
        │
        ▼
    [NASM]     ──→ Object file (.obj) + Runtime (.obj) / 目标文件 (.obj) + 运行时 (.obj)
        │
        ▼
    [GCC Link / GCC 链接] ──→ Executable (.exe) / 可执行文件 (.exe)

## Contributing / 贡献

Issues and pull requests are welcome!  
欢迎提交 Issue 和 Pull Request！

- Report bugs: attach a reproducible `.yxg` source file / 报告 Bug：附上能复现的 .yxg 源文件
- Feature suggestions: describe syntax and expected behavior / 功能建议：描述语法和期望行为
- Code contributions: follow the existing code style (C23, snake_case, Chinese comments) / 代码贡献：遵循现有代码风格（C23，snake_case，中文注释）
- Before submitting a PR, ensure your code passes existing tests (if any) and add corresponding tests / 提交 PR 前请确保代码通过现有测试（如果有），并添加相应测试

## License / 许可证

Apache-2.0 License — see the LICENSE file for details.  
Apache-2.0 许可证 — 详见 LICENSE 文件。

## Acknowledgments / 致谢

- NASM assembler / NASM 汇编器
- MinGW-w64 toolchain / MinGW-w64 工具链
- Everyone who has provided feedback and suggestions / 所有提供反馈和建议的用户