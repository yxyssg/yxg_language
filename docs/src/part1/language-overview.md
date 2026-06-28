# YXG 语言简介

## 设计初衷

YXG 是一门为教学而生的编译型编程语言。它有四个设计目标：

1. 足够简单——能够在有限时间内手写出完整的编译器
2. 足够丰富——涵盖静态类型、面向对象、模块系统等现代语言特性
3. 可调试——生成的汇编代码清晰可读，方便理解底层原理
4. 自包含——除了 C 标准库和操作系统 API，不依赖任何第三方框架

YXG 的语法设计受到 C 语言的强烈影响，同时吸收了 Java/C# 的面向对象特性和脚本语言的便利性。它的编译器完全用 C 语言手写，没有任何自动生成工具（如 Yacc/Lex），是学习编译原理的绝佳实践项目。

## 一分钟速览

下面是一个完整的 YXG 程序，定义了枚举、类、函数，实现了冒泡排序：

    enum Color { Red, Green, Blue }

    class Animal {
        string name;
        int age;

        pub void bark() {
            print name, " is making sound";
        }
    }

    int[] bubbleSort(int[] arr) {
        int n = arr.length;
        for int i = 0; i < n; i++ {
            for int j = 0; j < n - i - 1; j++ {
                if arr[j] > arr[j + 1] {
                    int tmp = arr[j];
                    arr[j] = arr[j + 1];
                    arr[j + 1] = tmp;
                }
            }
        }
        return arr;
    }

    int main(string[] args) {
        int[] nums = {5, 2, 8, 1, 9};
        Color c = Color.Green;
        int[] sorted = bubbleSort(nums);
        print "Sorted array:", sorted;

        Animal dog;
        dog.name = "Buddy";
        dog.age = 3;
        dog.bark();

        return 0;
    }

## 核心特性一览

### 基本类型系统

YXG 有 7 种基本类型：

| 类型 | 说明 | 示例 |
|------|------|------|
| int | 32 位有符号整数 | int x = 42; |
| uint | 32 位无符号整数 | uint u = 100; |
| float | 32 位浮点数 | float pi = 3.14; |
| ufloat | 无符号浮点 | ufloat f = 2.5; |
| char | 单字符（0-255） | char ch = 'A'; |
| bool | 布尔值 | bool flag = true; |
| string | 字符串 | string s = "hello"; |

类型之间支持隐式转换。int 可以赋值给 float，char 可以参与整数运算。uint 和 ufloat 不允许存储负数，编译器在语义分析阶段会检查。

### 变量与数组

    int x = 10;
    float pi = 3.14159;
    string name = "YXG";

    int[] scores = {90, 85, 100, 78};
    int[][] matrix = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

    int[] dyn = new int[100];
    matrix[1][2] = 99;
    int first = scores[0];
    int len = scores.length;

数组声明时可用花括号初始化，也可用 new 动态分配。.length 属性返回数组长度，类型为 int。多维数组内存布局连续，按行优先存储。

### 运算符

- 算术运算符：+  -  *  /  %
- 比较运算符：==  !=  >  <  >=  <=
- 逻辑运算符：&&  ||  !
- 位运算符：&  |  ~  ^  <<  >>
- 复合赋值：+=  -=  *=  /=  %=  <<=  >>=  ^=
- 自增自减：++  --
- 三目运算符：? :

优先级遵循 C 语言惯例。

    int a = 10;
    int b = a++ + 5;
    bool c = a > 10 && b < 20;
    int max = a > b ? a : b;

### 控制流

if / else if / else（条件不需要括号）：

    if score >= 90 {
        print "A";
    } else if score >= 80 {
        print "B";
    } else if score >= 70 {
        print "C";
    } else {
        print "F";
    }

switch / case（条件不需要括号）：

    switch day {
        case 1: print "Monday"; break;
        case 2: print "Tuesday"; break;
        case 3: print "Wednesday"; break;
        default: print "Unknown";
    }

while 循环（条件不需要括号）：

    while i < n {
        i++;
    }

do-while 循环：

    do {
        sum += n;
        n--;
    } while n > 0;

for 循环（条件不需要括号）：

    for int i = 0; i < 10; i++ {
        print i;
    }

foreach 循环：

    for v : array {
        print v;
    }

    for int v : numbers {
        print v;
    }

break 和 continue 在循环和 switch 中均可用。

### 函数

    int add(int a, int b) {
        return a + b;
    }

    void greet(string name) {
        print "Hello,", name;
    }

    string[] getNames() {
        return {"Alice", "Bob", "Charlie"};
    }

    int main(string[] args) {
        print "Arg count:", args.length;
        for string arg : args {
            print arg;
        }
        return 0;
    }

函数参数和返回值都需显式声明类型。无返回值用 void。main 函数可选接收 string[] args 代表命令行参数。

### 类与面向对象

    class Point {
        int x;
        int y;
        pri int z;

        pub Point(int x, int y) {
            this.x = x;
            this.y = y;
        }

        pub void move(int dx, int dy) {
            x = x + dx;
            y = y + dy;
        }

        pub int getX() { return x; }
        pub int getY() { return y; }
    }

    class ColorPoint : Point {
        int color;

        pub ColorPoint(int x, int y, int color) {
            this.x = x;
            this.y = y;
            this.color = color;
        }

        pub void display() {
            print "ColorPoint(", x, ",", y, ",", color, ")";
        }
    }

    Point p = new Point(10, 20);
    p.move(5, -3);
    ColorPoint cp = new ColorPoint(0, 0, 0xFF0000);
    cp.display();

访问控制有三种：pub（公开）、pro（受保护，子类可访问）、pri（私有，仅类内可访问，自动生成 getter/setter）。this 关键字在方法内部指代当前实例。构造函数与类同名，没有返回类型。

### 枚举

    enum Status {
        Pending = 0,
        Active = 1,
        Completed,
        Failed
    }

    Status s = Status.Pending;
    switch s {
        case Status.Pending:   print "waiting..."; break;
        case Status.Active:    print "running..."; break;
        case Status.Completed: print "done!"; break;
        default:               print "unknown";
    }

枚举成员如未显式赋值则自动递增。枚举本质是 int，可参与整数运算。

### 模块系统

通过 import 语句引入其他文件：

    import yxg.Math;

    Math m = new Math();
    float result = m.sqrt(16.0);

模块路径从当前目录、lib/ 目录和 YXG_PATH 环境变量中搜索。as 关键字设置别名。同一模块只导入一次。

### 标准库

YXG 提供了三个标准库，所有方法都需要先创建实例再调用：

| 库 | 引入方式 | 使用方式 |
|------|----------|----------|
| String | import yxg.String; | String str = new String(); str.len(s); |
| Math | import yxg.Math; | Math m = new Math(); m.sqrt(16.0); |
| System | import yxg.System; | System sys = new System(); sys.time(); |

## 与其他语言的关系

| 特性 | 灵感来源 |
|------|----------|
| 基本语法（分号、花括号） | C |
| 类型声明（int x） | C / Java |
| 类和继承 | Java / C# |
| foreach 循环 | C# / JavaScript |
| this 关键字 | Java / C++ |
| new 操作符 | Java / C++ |
| 字符串拼接（"a" + ch） | JavaScript |
| 访问控制（pub/pro/pri） | C++ |
| 枚举 | C / Java |
| import 模块系统 | Python / Go |
| .length 属性 | C# / JavaScript |

## 限制与取舍

为了保持编译器可实现性，YXG 有意回避：

- 无指针——用引用类型和数组替代
- 无模板/泛型——保持类型系统简单
- 无异常处理——通过返回值传递错误
- 无操作符重载
- 无函数重载——同作用域内函数名唯一
- 仅 Win32 x86——代码生成器面向 Windows + NASM + MinGW
- 无多线程

这些限制使得整个编译器可在约两万行 C 代码内完成，非常适合教学。

## 继续阅读

现在你已经对 YXG 语言有了整体了解。下一章将搭建编译环境，运行你的第一个程序。