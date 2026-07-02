# 运算符

## 概述

YXG 的运算符按功能分为七类：

| 类别 | 运算符 |
|------|--------|
| 算术 | + - * / % |
| 比较 | == != > < >= <= |
| 逻辑 | && \|\| ! |
| 位运算 | & \| ~ ^ << >> |
| 赋值 | = += -= *= /= %= <<= >>= ^= |
| 自增自减 | ++ -- |
| 三目 | ? : |

## 算术运算符

    int main() {
        int a = 17;
        int b = 5;

        int sum = a + b;
        int diff = a - b;
        int prod = a * b;
        int quot = a / b;
        int mod = a % b;

        print sum, diff, prod, quot, mod;

        float x = 17.0;
        float y = 5.0;
        float fdiv = x / y;
        // float fmod = x % y;   // 错误：浮点不支持 % 运算符

        print fdiv;
        return 0;
    }

整数除法向零截断：7 / 3 = 2，-7 / 3 = -2。

取模结果的符号与被除数相同：-17 % 5 = -2，17 % -5 = 2。

% 运算符只适用于整数类型（int、uint、char）。浮点数取模需使用内置函数。

## 比较运算符

比较运算的结果是 bool 类型：

    int main() {
        int a = 10;
        int b = 20;

        bool eq = a == b;
        bool ne = a != b;
        bool lt = a < b;
        bool gt = a > b;
        bool le = a <= b;
        bool ge = a >= b;

        print eq, ne, lt, gt, le, ge;

        string s1 = "apple";
        string s2 = "banana";
        bool strCmp = s1 == s2;
        bool strLess = s1 < s2;

        print strCmp, strLess;
        return 0;
    }

字符串比较按字典序逐字符进行。== 和 != 比较字符串内容，而不是地址。

## 逻辑运算符

    int main() {
        bool a = true;
        bool b = false;

        bool and = a && b;
        bool or = a || b;
        bool not = !a;

        print and, or, not;

        // 短路求值
        int x = 0;
        bool result = x != 0 && 10 / x > 5;
        // 由于 x == 0，左侧为 false，右侧不会执行

        print result;
        return 0;
    }

&& 和 || 使用短路求值：
- 对于 A && B，如果 A 为 false，B 不会求值
- 对于 A || B，如果 A 为 true，B 不会求值

## 位运算符

位运算符将操作数视为二进制位序列：

    int main() {
        int a = 0x0F;
        int b = 0x3C;

        int and = a & b;
        int or = a | b;
        int xor = a ^ b;
        int not = ~a;

        int shl = a << 2;
        int shr = b >> 2;

        print and, or, xor, not, shl, shr;
        return 0;
    }

注意事项：
- ~ 按位取反，结果与操作数类型相关
- << 左移，低位补 0
- >> 对有符号数使用算术右移（高位补符号位），对无符号数使用逻辑右移（高位补 0）
- 位运算符要求操作数为整数类型（int、uint、char、bool）

## 赋值运算符

基本赋值 = 已在前一章介绍。复合赋值将运算与赋值合并：

    int main() {
        int x = 10;

        x += 5;
        x -= 3;
        x *= 2;
        x /= 4;
        x %= 4;
        x <<= 2;
        x >>= 1;
        x ^= 0xFF;

        print x;
        return 0;
    }

赋值表达式的值是赋值后左操作数的值：

    int a;
    int b = (a = 5) + 3;     // a = 5, b = 8

## 自增自减运算符

    int main() {
        int x = 5;

        int a = ++x;
        int b = x++;
        int c = --x;
        int d = x--;

        print a, b, c, d, x;
        return 0;
    }

输出：

    6 6 6 6 5

自增自减只能用于变量（左值），不能用于字面量或表达式：

    int x = 5;
    // 5++;           // 错误
    // (x + 1)++;     // 错误

## 三目运算符

三目运算符 ? : 是 YXG 中唯一的三个操作数运算符，用于根据条件选择两个值之一：

    条件 ? 为真时的值 : 为假时的值

### 基本用法

    int main() {
        int a = 10;
        int b = 20;

        int max = a > b ? a : b;
        print "max:", max;                   // max: 20

        int min = a < b ? a : b;
        print "min:", min;                   // min: 10

        return 0;
    }

### 嵌套三目

三目运算符是右结合的，可以嵌套使用：

    int main() {
        int a = 10;
        int b = 20;

        // 等价于 a > b ? 1 : (a == b ? 0 : -1)
        int result = a > b ? 1 : a == b ? 0 : -1;
        print result;                        // -1

        // 多分支选择
        int score = 85;
        string grade = score >= 90 ? "A" :
                       score >= 80 ? "B" :
                       score >= 70 ? "C" :
                       score >= 60 ? "D" : "F";
        print grade;                         // B

        return 0;
    }

### 三目作为表达式的一部分

三目运算符可以嵌入到更大的表达式中：

    int main() {
        int a = 10;
        int b = 20;

        int c = (a < b ? a : b) + 5;
        print c;                             // 15

        // 在函数参数中使用
        print "The larger is", a > b ? a : b;

        return 0;
    }

### 不同分支的类型提升

当两个分支的类型不同时，结果类型会自动提升：

    int main() {
        int a = 10;
        int b = 5;

        // int 和 float → 结果为 float
        float f = a > b ? 3.14 : 2;
        print f;                             // 3.140000

        // int 和 char → 结果为 int
        int c = a > b ? 65 : 'Z';
        print c;                             // 65

        // string 和 char → 结果为 string
        string s = a > b ? "hello" : 'X';
        print s;                             // hello

        return 0;
    }

类型提升规则与二元运算一致。

### 三目与 if/else 对比

三目运算符是表达式，可以出现在任何需要值的地方；if/else 是语句，不能直接返回值：

    // 三目：简洁，适合简单条件
    int max = a > b ? a : b;

    // if/else：适合复杂逻辑
    int max2;
    if a > b {
        max2 = a;
    } else {
        max2 = b;
    }

选择建议：
- 简单的二选一赋值，用三目
- 需要执行多条语句，用 if/else
- 嵌套超过两层，用 if/else if 更清晰

## 运算符优先级

从高到低排列：

| 优先级 | 运算符 | 结合性 |
|--------|--------|--------|
| 1（最高） | ++ --（后缀） | 左到右 |
| 2 | ++ --（前缀）! ~ -（一元负） | 右到左 |
| 3 | * / % | 左到右 |
| 4 | + - | 左到右 |
| 5 | << >> | 左到右 |
| 6 | < <= > >= | 左到右 |
| 7 | == != | 左到右 |
| 8 | & | 左到右 |
| 9 | ^ | 左到右 |
| 10 | \| | 左到右 |
| 11 | && | 左到右 |
| 12 | \|\| | 左到右 |
| 13 | ? : | 右到左 |
| 14（最低） | = += -= *= /= %= <<= >>= ^= | 右到左 |

如果不确定优先级，使用括号明确求值顺序：

    int result = (a + b) * (c - d);
    int unclear = a + b * c - d;

## 运算符与类型

### 整数与浮点混合运算

    int main() {
        int i = 5;
        float f = 2.5;

        float result1 = i + f;
        float result2 = i * f;
        float result3 = i / f;

        float a = 5 / 2;
        float b = 5.0 / 2;
        float c = 5 / 2.0;

        print result1, a, b, c;
        return 0;
    }

注意 5 / 2 是整数除法，结果为 2。要得到浮点结果，至少一个操作数是浮点。

### 有符号与无符号混合

    int main() {
        int si = -10;
        uint ui = 20;

        uint result = si + ui;
        print result;
        return 0;
    }

有符号和无符号混合运算时，有符号数被转换为无符号数。这可能导致意外结果，建议避免混合使用。

### 字符串拼接

    int main() {
        string s = "Hello";

        string s1 = s + " World";
        string s2 = s + '!';
        string s3 = "Value: " + 42;
        string s4 = "Pi: " + 3.14;

        print s1, s2, s3, s4;
        return 0;
    }

+ 运算符用于字符串时表示拼接。char、int、float 等类型会自动转换为字符串后拼接。

## 常见错误

### 除零错误

    int x = 10;
    int y = 0;
    // int z = x / y;       // 编译时警告或运行时崩溃

如果除数是字面量 0，编译器会报错。如果是变量且值为 0，运行时会产生硬件异常。

### 赋值与比较混淆

    int x = 5;
    if x = 10 {              // 可能本意是 x == 10
        print "x is 10";     // 总是执行
    }

= 是赋值，== 是比较。在条件中使用 = 通常是一个 bug。

### 位运算符优先级

    int x = 5;
    bool result = x & 1 == 1;    // 可能本意是 (x & 1) == 1
    // 实际是 x & (1 == 1)，即 x & 1

== 的优先级高于 &。建议始终加括号。

## 继续阅读

掌握了运算符后，下一章将学习 YXG 的控制流语句。
