# 整体流水线

## 概述

YXG 编译器将源代码转换为可执行文件，经历以下阶段：

    hello.yxg
       │
       ▼
    ┌──────────┐
    │  词法分析  │  → Token 序列
    │  Lexer    │
    └──────────┘
       │
       ▼
    ┌──────────┐
    │  语法分析  │  → AST 抽象语法树
    │  Parser   │
    └──────────┘
       │
       ▼
    ┌──────────┐
    │  语义分析  │  → 标注类型的 AST + 符号表
    │ Semantics │
    └──────────┘
       │
       ▼
    ┌──────────┐
    │  IR 生成  │  → IR 中间表示链表
    │  IR Gen   │
    └──────────┘
       │
       ▼
    ┌──────────┐
    │  CFG 构建 │  → 基本块 + 控制流图
    │  CFG      │
    └──────────┘
       │
       ▼
    ┌──────────┐
    │  SSA 构造 │  → SSA 形式 + 优化 Pass
    │  SSA      │
    └──────────┘
       │
       ▼
    ┌──────────┐
    │ 寄存器分配│  → 寄存器映射表
    │ RegAlloc  │
    └──────────┘
       │
       ▼
    ┌──────────┐
    │  代码生成  │  → output.asm (NASM 汇编)
    │ Codegen   │
    └──────────┘
       │
       ▼
    ┌──────────┐
    │ NASM 汇编 │  → output.obj
    └──────────┘
       │
       ▼
    ┌──────────┐
    │ GCC 链接  │  → output.exe (含运行时库)
    └──────────┘

## 各阶段职责

### 词法分析（Lexer）

将源代码字符串转换为 Token 序列。Token 是编译器内部的最小语义单元，例如关键字 `int`、标识符 `main`、运算符 `+`、字面量 `42` 等。

输入：

    int x = 10;

输出：

    TOKEN_INT  TOKEN_IDENTIFIER("x")  TOKEN_ASSIGN  TOKEN_NUMBER("10")  TOKEN_SEMICOLON

### 语法分析（Parser）

将 Token 序列按照语法规则组织成抽象语法树（AST）。AST 以树形结构表示程序的语法层次。

输入：

    int x = 10;

输出：

    AST_VAR
    ├── type: int
    ├── name: x
    └── value: AST_LITERAL(10)

### 语义分析（Semantics）

遍历 AST，进行类型检查、符号表管理、作用域分析。标注每个节点的数据类型，检查类型匹配、变量是否声明等。

输入：未标注类型的 AST

输出：标注了类型的 AST + 完整的符号表

### IR 生成（IR Gen）

将标注类型的 AST 转换为三地址码形式的中间表示（IR）。IR 是一种接近汇编但独立于具体硬件的线性指令序列。

输入：AST

输出：

    .t1 = 10
    x = .t1

### CFG 构建

将线性 IR 链表划分为基本块，构建控制流图。基本块是一段顺序执行的 IR 序列，以跳转或返回结尾。

输入：IR 链表

输出：基本块数组 + 前驱/后继关系

### SSA 构造

将 IR 转换为静态单赋值形式，并进行优化。SSA 要求每个变量只被赋值一次，通过 φ 节点合并不同路径的值。

输入：普通 IR + CFG

输出：SSA 形式 IR + 优化后的 IR

### 寄存器分配

将 IR 中的虚拟变量映射到物理寄存器（eax、ecx、edx、ebx、esi、edi）。使用 Chaitin-Briggs 图着色算法。

输入：SSA 形式 IR + 活跃区间

输出：寄存器映射表

### 代码生成

将 IR 指令翻译为 x86 NASM 汇编代码。处理函数序言/结语、调用约定、数据段布局等。

输入：IR + 寄存器映射

输出：output.asm

### 汇编与链接

调用 NASM 将汇编文件汇编为目标文件，再调用 GCC 链接运行时库生成可执行文件。

输入：output.asm + 运行时 .obj 文件

输出：output.exe

## 核心源文件速查

| 阶段 | 主要文件 | 核心函数 |
|------|----------|----------|
| 词法分析 | Lexer.c | lexer() → lexer_parse() |
| 语法分析 | parser.c | parser_parse() → parser_Sentence() |
| 语义分析 | Semantics.c | Semantics() → main_parse() |
| IR 生成 | IR.c | parse_IR() → generate_IR() |
| CFG | CFG.c | cfg_build() |
| SSA | SSA.c | ssa_construct() → ssa_destruct() |
| 寄存器分配 | regmap.c | regmap_allocate_from_cfg() |
| 代码生成 | Codegen_win32.c | codegen_win32() → codegen_win32_end() |
| 主程序 | main.c | main() — 编排整个流水线 |

## 关键数据结构的关系

    Token ──组成──▶ ASTNode ──转换──▶ IR
      │                │                │
      │                ▼                ▼
      │          Symbol Table        CFG (BasicBlock[])
      │          (符号表)            (控制流图)
      │                                  │
      │                                  ▼
      │                            SSA 形式 IR
      │                                  │
      │                                  ▼
      │                            RegMap (寄存器映射)
      │
      ▼
    TokenType / Token (定义在 Token.h)

## 编译过程示例

以 `int x = 10;` 为例，追踪整个编译流程：

词法分析：

    Token[0]: {value: "int", type: TOKEN_INT}
    Token[1]: {value: "x", type: TOKEN_IDENTIFIER}
    Token[2]: {value: "=", type: TOKEN_ASSIGN}
    Token[3]: {value: "10", type: TOKEN_NUMBER}
    Token[4]: {value: ";", type: TOKEN_SEMICOLON}

语法分析：

    AST_VAR
    ├── type: TOKEN_INT
    ├── name: "x"
    └── value: AST_LITERAL
        └── value: "10", type: TOKEN_NUMBER

语义分析：

    AST_VAR
    ├── type: TOKEN_INT
    ├── dataType: DATA_INT          ← 标注了类型
    ├── internal_name: "_var0"      ← 生成了内部名
    └── value: AST_LITERAL
        └── dataType: DATA_INT      ← 标注了类型

IR 生成：

    IR[0]: Literal: .t1 = 10
    IR[1]: Var: _var0 = .t1

代码生成：

    mov eax, 10
    mov dword [_var0], eax

## 继续阅读

下一章将介绍编译器中各核心数据结构的详细设计。