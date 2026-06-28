# 基本类型

## 概述

YXG 是一门静态类型语言，每个变量在编译时就必须确定类型。共有 7 种基本类型：

| 类型 | 关键字 | 占用字节 | 取值范围 | 默认值 |
|------|--------|----------|----------|--------|
| 有符号整数 | int | 4 | -2147483648 ~ 2147483647 | 0 |
| 无符号整数 | uint | 4 | 0 ~ 4294967295 | 0 |
| 浮点数 | float | 4 | ±1.18e-38 ~ ±3.4e+38 | 0.0 |
| 无符号浮点 | ufloat | 4 | 0 ~ 3.4e+38 | 0.0 |
| 字符 | char | 1 | 0 ~ 255 | '\0' |
| 布尔 | bool | 1 | true / false | false |
| 字符串 | string | 4（指针） | 任意长度 | null |

## int：有符号整数

最常用的整数类型，32 位有符号：

    int main() {
        int a = 42;
        int b = -100;
        int c = 0;

        int hex = 0xFF;
        int oct = 077;
        int bin = 0b1010;

        int sum = a + b;
        int diff = a - b;
        int prod = a * b;
        int quot = a / b;
        int mod = a % b;

        print sum, diff, prod, quot, mod;
        return 0;
    }

整数除法向零截断。例如 7 / 3 = 2，-7 / 3 = -2。取模结果的符号与被除数相同。YXG 不检查整数溢出，行为与底层 CPU 一致。

## uint：无符号整数

32 位无符号，不能存储负数：

    int main() {
        uint a = 100;
        uint b = 200;
        uint max = 4294967295;

        uint sum = a + b;

        // uint x = -1;          // 编译错误！

        if a < b {
            print "a is less than b";
        }

        return 0;
    }

uint 和 int 混合运算时，结果提升为 uint。比较时使用无符号比较指令。

## float：浮点数

32 位 IEEE 754 单精度浮点：

    int main() {
        float pi = 3.14159;
        float e = 2.71828;
        float small = 1.5e-3;
        float big = 6.02e23;

        float sum = pi + e;
        float area = pi * 5.0 * 5.0;

        float half = 1 / 2.0;
        float mixed = 3 + 0.14;

        print pi, area, half, mixed;
        return 0;
    }

浮点数字面量必须包含小数点或使用科学计数法。整数与浮点混合运算时，整数自动提升为浮点。单精度约 7 位有效数字，存在舍入误差。

更复杂的数学运算可使用 Math 标准库：

    import yxg.Math;

    int main() {
        Math m = new Math();

        float x = m.sqrt(16.0);
        float y = m.sin(1.5708);
        float z = m.fabs(-3.14);

        print x, y, z;
        return 0;
    }

Math 库还提供了 cos、tan、pow、min、max、clamp 以及随机数等功能。

## ufloat：无符号浮点

与 float 相同的内存布局，但语义上不允许负值：

    int main() {
        ufloat price = 19.99;
        ufloat distance = 100.5;

        // ufloat x = -1.5;      // 编译错误！

        float discount = 0.8;
        ufloat final_price = price * discount;

        print final_price;
        return 0;
    }

ufloat 与 float 混合运算时，结果提升为 ufloat。编译器在语义分析阶段检查负值赋值。

## char：字符

单字节，存储 0 到 255 之间的值：

    int main() {
        char ch = 'A';
        char newline = '\n';
        char tab = '\t';
        char quote = '\"';
        char backslash = '\\';

        char next = ch + 1;
        int code = ch;

        if ch >= 'A' && ch <= 'Z' {
            print ch, "is uppercase";
        }

        print ch, next, code;
        return 0;
    }

转义序列：

| 序列 | 含义 | ASCII |
|------|------|-------|
| \n | 换行 | 10 |
| \t | 制表符 | 9 |
| \\ | 反斜杠 | 92 |
| \' | 单引号 | 39 |
| \" | 双引号 | 34 |
| \0 | 空字符 | 0 |

char 和 int 可以互相赋值和运算。char 赋值给 int 时零扩展。

## bool：布尔值

只有两个值：true 和 false：

    int main() {
        bool flag = true;
        bool done = false;

        bool result = 10 > 5;
        bool equal = 3 == 4;

        bool and = true && false;
        bool or = true || false;
        bool not = !true;

        if flag {
            print "flag is true";
        }

        int asInt = true;
        int asInt2 = false;

        print result, and, or, not, asInt;
        return 0;
    }

bool 在内存中占 1 字节。true 存储为 1，false 存储为 0。bool 可以隐式转换为 int，但 int 不能隐式转换为 bool（条件表达式除外）。

## string：字符串

可变长度的字符序列。详细用法见字符串章节，这里展示基本操作：

    import yxg.String;

    int main() {
        String str = new String();

        string s1 = "Hello";
        string s2 = "World";

        string s3 = s1 + " " + s2;

        int len = str.len(s1);

        int c = str.charAt(s1, 0);

        if s1 == s2 {
            print "equal";
        } else {
            print "not equal";
        }

        print s3, len, c;
        return 0;
    }

字符串在底层以 null 结尾，存储了长度信息。字符串是不可变的，每次拼接创建新对象，旧对象由 GC 回收。

## 类型转换

### 隐式转换

    int main() {
        float f = 42;
        int i = 'A';
        string s1 = "value:" + 42;
        string s2 = "letter:" + 'A';
        int b = true;
        uint u = 100;

        return 0;
    }

### 类型提升规则

二元运算时的类型提升：

| 左操作数 | 右操作数 | 结果类型 |
|----------|----------|----------|
| int | int | int |
| int | float | float |
| int | uint | uint |
| float | int | float |
| float | float | float |
| uint | uint | uint |
| uint | float | ufloat |
| char | char | int |
| char | int | int |
| string | char | string |
| string | int | string |
| string | string | string |

比较运算的结果始终是 bool。

## 类型默认值

未初始化的变量具有以下默认值：

    int main() {
        int i;
        uint u;
        float f;
        ufloat uf;
        char c;
        bool b;
        string s;

        print i, f, b;
        return 0;
    }

建议始终显式初始化变量。特别注意未初始化的 string 是 null，访问其属性会出错。

## 类型相关错误

### 类型不匹配

    // int x = "hello";     // 错误！error[E3003]

### 无符号类型赋负值

    // uint x = -5;         // 错误！error[E3017]

### 条件表达式类型错误

    int x = 10;
    // if x { ... }         // 错误！int 不能直接作为条件
    if x != 0 { ... }       // 正确

## 继续阅读

掌握了基本类型后，下一章将学习变量的声明、赋值和作用域。