# 附录：术语表

## A

**AST（抽象语法树）**
Abstract Syntax Tree。源代码经过语法分析后得到的树形数据结构，每个节点代表程序中的一个语法结构，如表达式、语句、声明等。

## B

**基本块（Basic Block）**
一段顺序执行的指令序列，只有一个入口和一个出口。控制流图由基本块组成。

**变量重命名（Variable Renaming）**
SSA 构造的第二步。遍历支配树，将每个变量的定义替换为带版本号的新名称，使用处替换为当前栈顶版本。

**编译器前端（Compiler Frontend）**
编译器的前三个阶段：词法分析、语法分析、语义分析。负责将源代码转换为标注类型的 AST。

**编译器中端（Compiler Middle-end）**
编译器在语义分析和代码生成之间的阶段。YXG 的中端包括 IR 生成、CFG 构建、SSA 构造和优化。

**编译器后端（Compiler Backend）**
编译器的最后阶段：寄存器分配和代码生成。负责将 IR 转换为目标平台的汇编代码。

## C

**CFG（控制流图）**
Control Flow Graph。由基本块和它们之间的跳转关系组成的有向图，表示程序的控制流结构。

**常量传播（Constant Propagation）**
一种编译器优化。将编译时已知的常量值替换到使用处，减少运行时计算。

**常量折叠（Constant Folding）**
一种编译器优化。在编译时计算常量表达式的结果，例如将 2+3 替换为 5。

**成员访问控制**
YXG 中控制类成员可见性的机制，包括 pub（公开）、pro（受保护）、pri（私有）三种级别。

## D

**递归下降分析（Recursive Descent Parsing）**
一种手写语法分析器的方法。为每个语法规则编写一个函数，通过递归调用解析嵌套结构。

**短路求值（Short-circuit Evaluation）**
逻辑表达式的求值策略。对于 A && B，如果 A 为 false 则不计算 B；对于 A || B，如果 A 为 true 则不计算 B。

## F

**φ 节点（Phi Node）**
SSA 形式中的特殊节点，用于在控制流汇合点合并来自不同路径的变量值。

**符号表（Symbol Table）**
存储程序中所有标识符信息的数据结构，包括变量名、类型、作用域等。

## G

**getter / setter**
编译器为 pri 字段自动生成的访问方法。getter 用于读取字段值，setter 用于修改字段值。

## H

**活跃分析（Liveness Analysis）**
数据流分析的一种。确定每个变量在程序的哪些位置是"活跃的"（其值在后续会被使用）。

## I

**IR（中间表示）**
Intermediate Representation。编译器内部使用的、介于高级语言和机器语言之间的程序表示形式。YXG 使用三地址码形式的 IR。

## J

**寄存器分配（Register Allocation）**
将程序中的虚拟变量映射到 CPU 物理寄存器的过程。YXG 使用 Chaitin-Briggs 图着色算法。

## L

**立即数（Immediate）**
直接嵌入在指令中的常量值，如 mov eax, 42 中的 42。

**令牌（Token）**
词法分析器输出的最小语义单元，如关键字、标识符、运算符、字面量等。

## N

**NASM**
Netwide Assembler。YXG 编译器使用的 x86 汇编器，将 .asm 文件汇编为 .obj 目标文件。

## P

**parser（语法分析器）**
将 Token 序列按照语法规则组织成 AST 的程序。

**pub / pro / pri**
YXG 中的三种访问控制修饰符。pub 公开，pro 仅子类可访问，pri 仅本类可访问。

## S

**SSA（静态单赋值）**
Static Single Assignment。一种 IR 形式，要求每个变量只被赋值一次。通过 φ 节点合并不同控制流路径的值。

**SSA 解构（SSA Destruction）**
将 SSA 形式 IR 转换回普通 IR 的过程。移除 φ 节点，在前驱基本块末尾插入复制指令。

**三地址码（Three-address Code）**
一种 IR 形式，每条指令最多有三个操作数：两个源操作数和一个目标操作数，如 x = y + z。

## T

**图着色算法（Graph Coloring）**
寄存器分配的核心算法。将变量视为图中的节点，冲突（同时活跃）的变量之间有边。用 k 种颜色（寄存器）给图着色，无法着色的变量溢出到栈上。

## Y

**YXG 语言**
本书的主角——一门为教学设计的编译型编程语言，语法类似 C，支持面向对象。

**YXG 编译器**
将 YXG 源代码编译为 Windows 可执行文件的程序，完全用 C 语言手写。

## Z

**支配树（Dominator Tree）**
控制流图的一种衍生结构。节点 A 支配节点 B 表示从入口到 B 的所有路径都必须经过 A。支配树以树形结构表示支配关系。

**支配边界（Dominance Frontier）**
SSA 构造的关键概念。节点 B 的支配边界是那些不被 B 严格支配、但被 B 的某个前驱支配的节点的集合。φ 节点就插入在支配边界处。

**中间代码生成（IR Generation）**
将 AST 转换为 IR 的过程。遍历 AST，为每个节点生成对应的 IR 指令序列。