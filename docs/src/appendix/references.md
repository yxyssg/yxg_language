# 附录：参考资料

## 编译原理经典教材

**《Compilers: Principles, Techniques, and Tools》（龙书）**
Alfred V. Aho, Monica S. Lam, Ravi Sethi, Jeffrey D. Ullman
编译原理领域最权威的教材，涵盖词法分析、语法分析、语义分析、中间代码生成、代码优化和目标代码生成。

**《Engineering a Compiler》（虎书）**
Keith D. Cooper, Linda Torczon
以工程实践为导向的编译器教材，对 SSA 形式、寄存器分配、指令调度等主题有深入讲解。

**《Modern Compiler Implementation in C》（鲸书）**
Andrew W. Appel
使用 C 语言实现编译器的经典教材，涵盖 Tiger 语言的完整编译器实现。

## SSA 形式相关论文

**Efficiently Computing Static Single Assignment Form and the Control Dependence Graph**
Ron Cytron, Jeanne Ferrante, Barry K. Rosen, Mark N. Wegman, F. Kenneth Zadeck
ACM Transactions on Programming Languages and Systems (TOPLAS), 1991
SSA 形式的奠基性论文，提出了基于支配边界的 φ 节点插入算法。

**Practical Improvements to the Construction and Destruction of Static Single Assignment Form**
Preston Briggs, Keith D. Cooper, Timothy J. Harvey, L. Taylor Simpson
Software: Practice and Experience, 1998
对 SSA 构造和解构的实用改进，包括剪枝优化。

## 寄存器分配相关论文

**Register Allocation via Coloring**
Gregory J. Chaitin, Marc A. Auslander, Ashok K. Chandra, John Cocke, Martin E. Hopkins, Peter W. Markstein
Computer Languages, 1981
图着色寄存器分配的原始论文。

**Register Allocation via Graph Coloring**
Preston Briggs
PhD Thesis, Rice University, 1992
Chaitin-Briggs 寄存器分配算法的详细描述，引入了乐观着色策略。

## x86 汇编参考

**NASM - The Netwide Assembler**
https://www.nasm.us/
YXG 编译器使用的汇编器，网站提供完整的指令参考和文档。

**Intel 64 and IA-32 Architectures Software Developer's Manual**
https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
Intel 官方 x86/x64 指令集参考手册，包含每条指令的详细说明。

## 相关工具

**MSYS2**
https://www.msys2.org/
Windows 平台上的类 Unix 环境，提供 MinGW GCC、NASM 等工具。

**mdBook**
https://rust-lang.github.io/mdBook/
本文档使用的构建工具，用 Markdown 编写，生成静态 HTML 网站。

## 在线资源

**Compiler Explorer**
https://godbolt.org/
在线编译器探索工具，可以查看各种编译器生成的汇编代码，对理解代码生成非常有帮助。

**LLVM**
https://llvm.org/
工业级编译器基础设施，其 IR 设计和优化 Pass 架构对理解现代编译器有重要参考价值。

**Crafting Interpreters**
https://craftinginterpreters.com/
Robert Nystrom 的在线书籍，手把手教你用 Java 和 C 实现一个完整的脚本语言解释器。