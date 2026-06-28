# 数据结构总览

## 概述

YXG 编译器在编译过程中使用多种核心数据结构来承载不同阶段的信息。理解这些数据结构是理解整个编译器架构的关键。

## 数据结构的流转

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

## Token（词法单元）

Token 是词法分析器的输出，代表源代码中最小的语义单元。定义在 Token.h：

    typedef struct Token {
        char* value;        // 词法值，如 "int"、"42"、"x"
        TokenType type;     // 词法类型，如 TOKEN_INT、TOKEN_NUMBER
        int line;           // 所在行号
        int column;         // 所在列号
    } Token;

TokenType 枚举包含约 80 种类型，涵盖关键字、运算符、字面量、分隔符等。

## ASTNode（抽象语法树节点）

ASTNode 是语法分析器的输出，以树形结构表示程序的语法层次。定义在 AST.h：

    typedef struct ASTNODE {
        ASTType type;       // 节点类型
        Token value;        // 关联的词法单元
        struct ASTNODE* next;  // 兄弟节点（语句链表）
    } ASTNode;

ASTType 枚举包含约 30 种节点类型：

| 类型 | 对应结构体 | 用途 |
|------|-----------|------|
| AST_LITERAL | AST_Literal | 字面量、标识符、函数调用 |
| AST_BINARY | AST_Binary | 二元运算表达式 |
| AST_UNARY | AST_Unary | 一元运算表达式 |
| AST_VAR | AST_Var | 变量声明（带初始化） |
| AST_VAR_ASS | AST_Var | 变量赋值 |
| AST_UNINIT_VAR | AST_Var | 变量声明（未初始化） |
| AST_IF | AST_If | if/else if/else 语句 |
| AST_SWITCH | AST_Switch | switch/case 语句 |
| AST_WHILE | AST_While | while/do-while 循环 |
| AST_FOR | AST_For | for 循环 |
| AST_FOREACH | AST_ForEach | foreach 循环 |
| AST_FUNCTION | AST_Function | 函数定义 |
| AST_RETURN | AST_Return | return 语句 |
| AST_PRINT | AST_Print | print 语句 |
| AST_CLASS_DEF | AST_ClassDef | 类定义 |
| AST_ENUM_DEF | AST_EnumDef | 枚举定义 |
| AST_ARRAY_DECL | AST_ArrayDecl | 数组声明 |
| AST_ARRAY_ACCESS | AST_ArrayAccess | 数组元素访问 |
| AST_ARRAY_ASS | AST_ArrayAss | 数组元素赋值 |

每个 AST 类型对应一个具体的结构体，它们都以 ASTNode 作为第一个字段（base），实现简单的继承：

    typedef struct AST_BINARY {
        ASTNode base;           // 基类
        ASTNode* left;          // 左操作数
        Token op;               // 运算符
        ASTNode* right;         // 右操作数
        DataType resultType;    // 结果类型（语义分析后填充）
        bool is_unsigned;       // 是否无符号运算
    } AST_Binary;

## Symbol 与符号表

符号表用于语义分析阶段，存储变量、函数、类等符号的信息。定义在 Semantics.h：

    typedef struct SYMBOL {
        struct SYMBOL* next;    // 链表（哈希桶）
        Token value;            // 词法信息
        DataType type;          // 数据类型
        char* key;              // 符号名
        char* internal_name;    // 编译器内部名（如 _var0）
        char* class_name;       // 所属类名（如果是类成员）
        int dim_count;          // 数组维度
        int* dim_sizes;         // 各维度大小
        bool is_field;          // 是否是类字段
        int field_offset;       // 字段在类中的偏移
        bool is_function;       // 是否是函数
        int field_modifier;     // 访问修饰符
    } Symbol;

符号表使用作用域链组织：

    typedef struct SCOPE {
        Symbol_Table* table;    // 当前作用域的哈希表
        struct SCOPE* parent;   // 父作用域
        int depth;              // 作用域深度
    } Scope;

哈希表结构：

    typedef struct SYMBOL_TABLE {
        Symbol** buckets;       // 哈希桶数组
        int count;              // 符号数量
        int size;               // 桶数量
    } Symbol_Table;

## IR（中间表示）

IR 是编译器中间层的核心数据结构，采用三地址码形式。定义在 IR.h：

    typedef struct IR {
        IRType type;            // 指令类型
        char* result;           // 结果变量名
        int def_id;             // 定义 ID（用于 SSA）
        IR_OP_Type op;          // 操作类型（二元运算时使用）
        char* args1;            // 第一操作数
        int args1_def;          // 第一操作数的定义 ID
        DataType args1_type;    // 第一操作数类型
        char* args2;            // 第二操作数
        int args2_def;          // 第二操作数的定义 ID
        DataType args2_type;    // 第二操作数类型
        DataType data_type;     // 结果数据类型
        struct IR* next;        // 下一条指令（链表）
        bool has_return;        // 是否是函数入口标签
        int array_size;         // 数组大小
        int elem_size;          // 元素字节数
        int phi_arg_count;      // φ 节点参数数量
        char** phi_arg_names;   // φ 节点参数名
        char** label;           // 标签数组
        int label_count;        // 标签数量
        DataType* call_func_args_type;  // 函数调用参数类型
        int call_func_args_num;         // 函数调用参数数量
    } IR;

IRType 枚举包含约 30 种指令类型：

| 类型 | 用途 |
|------|------|
| IR_LITERAL | 字面量赋值 |
| IR_BINARY | 二元运算 |
| IR_VAR | 变量声明 |
| IR_VAR_ASS | 变量赋值 |
| IR_VAR_UNINIT | 未初始化变量声明 |
| IR_JMP | 无条件跳转 |
| IR_JMPZ | 为零跳转 |
| IR_JMPNZ | 非零跳转 |
| IR_LABEL | 标签 |
| IR_CALL | 函数调用 |
| IR_RET | 函数返回 |
| IR_PUSH | 压栈 |
| IR_PRINT | 打印输出 |
| IR_ARRAY_DECL | 数组声明 |
| IR_ARRAY_LOAD | 数组元素加载 |
| IR_ARRAY_STORE | 数组元素存储 |
| IR_PHI | φ 节点（SSA） |
| IR_BOUNDS_CHECK | 数组越界检查 |

## BasicBlock 与 CFG

控制流图由基本块组成。定义在 CFG.h：

    typedef struct BasicBlock {
        int id;                     // 块编号
        IR* first_ir;               // 第一条 IR
        IR* last_ir;                // 最后一条 IR
        int ir_count;               // IR 数量
        struct BasicBlock** succs;  // 后继块数组
        int succ_count;             // 后继数量
        struct BasicBlock** preds;  // 前驱块数组
        int pred_count;             // 前驱数量
        struct BasicBlock* idom;    // 直接支配者
        struct BasicBlock** dom_children;  // 支配子节点
        struct BasicBlock** df;     // 支配边界
        bool* live_gen;             // 活跃变量 gen 集
        bool* live_kill;            // 活跃变量 kill 集
        bool* live_in;              // 活跃变量 in 集
        bool* live_out;             // 活跃变量 out 集
        IR* phi_nodes;              // φ 节点链表
        char** def_names;           // 本块定义的变量名
        char** use_names;           // 本块使用的变量名
        int linear_start;           // 线性起始编号
        int linear_end;             // 线性结束编号
    } BasicBlock;

CFG 结构：

    typedef struct CFG {
        BasicBlock** blocks;        // 基本块数组
        int block_count;            // 基本块数量
        BasicBlock* entry;          // 入口块
        BasicBlock* exit_block;     // 出口块
        char** var_names;           // 所有变量名
        int var_count;              // 变量数量
        LiveInterval* intervals;    // 活跃区间数组
        int interval_count;         // 活跃区间数量
    } CFG;

## RegMap（寄存器映射）

寄存器分配的结果，记录每个变量映射到哪个寄存器或溢出槽。定义在 regmap.h：

    typedef enum Reg {
        R_NONE = -1,
        R_EAX  = 0,
        R_ECX  = 1,
        R_EDX  = 2,
        R_EBX  = 3,
        R_ESI  = 4,
        R_EDI  = 5,
    } Reg;

    typedef struct RegMapEntry {
        Reg reg;            // 分配的寄存器
        bool is_float;      // 是否是浮点变量
        int spill_slot;     // 溢出槽偏移（ebp 相对）
    } RegMapEntry;

    typedef struct RegMap {
        char** names;               // 变量名数组
        RegMapEntry* entries;       // 映射条目数组
        int count;                  // 条目数量
        int capacity;               // 容量
        unsigned callee_saved_mask; // 被调用者保存寄存器掩码
    } RegMap;

## 数据结构的生命周期

    Token:      词法分析创建 → 语法分析后释放
    ASTNode:    语法分析创建 → 代码生成后释放
    Symbol:     语义分析创建 → 语义分析结束后释放
    IR:         IR 生成创建 → 代码生成后释放
    CFG:        CFG 构建创建 → 寄存器分配后释放
    RegMap:     寄存器分配创建 → 代码生成后释放

## 继续阅读

下一章将介绍编译器的错误处理系统。