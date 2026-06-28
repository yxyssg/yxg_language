# 变量与赋值

## 变量声明

YXG 中变量必须先声明后使用。声明变量的语法为：

    类型 变量名 [= 初始值];

### 带初始化的声明

    int main() {
        int age = 25;
        float price = 19.99;
        string name = "Alice";
        bool active = true;
        char grade = 'A';

        print age, price, name, active, grade;
        return 0;
    }

### 不初始化的声明

    int main() {
        int count;
        float score;
        string title;
        bool ready;

        count = 10;
        score = 95.5;
        title = "Manager";
        ready = true;

        print count, score, title, ready;
        return 0;
    }

虽然 YXG 允许不初始化，但建议始终在声明时赋值，避免使用默认值带来的意外行为。

### 一次声明多个变量

每条声明语句只能声明一个变量。不能像 C 语言那样用逗号分隔：

    int a = 1;
    int b = 2;
    // int a = 1, b = 2;   // 错误！不支持

## 赋值

使用 = 进行赋值。赋值语句本身是一个表达式，返回被赋的值：

    int main() {
        int x;
        int y;

        x = 10;
        y = x;

        int z = (x = 20);

        int a, b, c;
        a = b = c = 100;

        print x, y, z, a, b, c;
        return 0;
    }

## 复合赋值运算符

YXG 支持所有常见的复合赋值：

    int main() {
        int x = 10;

        x += 5;
        x -= 3;
        x *= 2;
        x /= 4;
        x %= 4;

        print x;
        return 0;
    }

位运算的复合赋值：

    int main() {
        int x = 0x0F;

        x <<= 4;
        x >>= 2;
        x ^= 0xFF;

        print x;
        return 0;
    }

## 自增与自减

    int main() {
        int a = 5;
        int b;

        b = ++a;
        print "前置++: a =", a, "b =", b;

        b = a++;
        print "后置++: a =", a, "b =", b;

        b = --a;
        print "前置--: a =", a, "b =", b;

        b = a--;
        print "后置--: a =", a, "b =", b;

        return 0;
    }

输出：

    前置++: a = 6 b = 6
    后置++: a = 7 b = 6
    前置--: a = 6 b = 6
    后置--: a = 5 b = 6

自增自减只能用于变量，不能用于表达式：

    int x = 5;
    // (x + 1)++;     // 错误！不能对表达式自增

## 变量命名规则

变量名必须遵循以下规则：

1. 只能包含字母（a-z、A-Z）、数字（0-9）和下划线（_）
2. 不能以数字开头
3. 区分大小写（name 和 Name 是不同的变量）
4. 不能与关键字同名

合法示例：

    int age;
    int player1_score;
    int _internal;
    int MAX_VALUE;

不合法示例：

    // int 1st_place;     // 不能以数字开头
    // int my-var;        // 连字符不允许
    // int class;         // class 是关键字
    // int first name;    // 空格不允许

## 作用域

变量在声明它的代码块内可见。代码块由花括号 {} 界定：

    int main() {
        int outer = 100;

        {
            int inner = 200;
            print outer;         // 可以访问外层变量
            print inner;         // 200
        }

        print outer;             // 100
        // print inner;          // 错误！inner 已超出作用域

        return 0;
    }

内层作用域可以声明与外层同名的变量，这会遮蔽外层变量：

    int main() {
        int x = 10;

        {
            int x = 20;
            print x;             // 20
        }

        print x;                 // 10（外层 x 未被修改）

        return 0;
    }

函数参数的作用域是整个函数体：

    void greet(string name) {
        print "Hello,", name;
    }

## 常量

YXG 目前没有 const 关键字。约定用全大写命名表示不应修改的变量：

    int main() {
        int MAX_SIZE = 100;
        float PI = 3.14159;
        string APP_NAME = "MyApp";

        print MAX_SIZE, PI, APP_NAME;
        return 0;
    }

枚举成员也可以作为具名常量使用：

    enum Limits {
        MAX_PLAYERS = 64,
        MAX_LEVEL = 99,
        TIMEOUT = 30
    }

    int main() {
        int players = Limits.MAX_PLAYERS;
        print "Max players:", players;
        return 0;
    }

## 类型别名（通过类名）

YXG 没有 typedef，但可以用类名作为类型：

    class Point {
        int x;
        int y;
    }

    int main() {
        Point p;
        p.x = 10;
        p.y = 20;
        print p.x, p.y;
        return 0;
    }

## 常见错误

### 使用未声明的变量

    int main() {
        x = 10;              // 错误：x 未声明
        return 0;
    }

编译器报错：

    error[E3001]: variable 'x' is not declared

### 重复声明

    int main() {
        int x = 10;
        int x = 20;          // 错误：x 已声明
        return 0;
    }

编译器报错：

    error[E3002]: duplicate variable 'x'

### 类型不匹配

    int main() {
        int x = "hello";     // 错误：类型不匹配
        return 0;
    }

编译器报错：

    error[E3003]: type mismatch: expected 'int', got 'string'

### 无符号类型赋负值

    int main() {
        uint x = -5;         // 错误
        return 0;
    }

编译器报错：

    error[E3017]: cannot assign negative value to unsigned type 'uint'

## 继续阅读

掌握了变量与赋值后，下一章将学习 YXG 的运算符体系。