# 第一个程序：Hello World

## 最小的 YXG 程序

创建一个文件 hello.yxg，输入以下内容：

    int main() {
        print "Hello, YXG!";
        return 0;
    }

编译并运行：

    yxg hello.yxg -r

输出：

    Hello, YXG!

## 逐行解析

### 第 1 行：int main()

    int main() {

- int 是返回值类型，表示这个函数返回一个整数
- main 是函数名，它是程序的入口点，每个 YXG 程序必须有且仅有一个 main 函数
- () 是参数列表，这里为空表示不接收命令行参数
- { 表示函数体的开始

### 第 2 行：print "Hello, YXG!"

    print "Hello, YXG!";

- print 是 YXG 的内置语句，用于向控制台输出内容
- "Hello, YXG!" 是一个字符串字面量
- print 不需要括号，直接跟要打印的内容
- ; 分号表示语句结束

### 第 3 行：return 0

    return 0;

- return 语句将值返回给调用者
- 0 表示程序正常退出

### 第 4 行：}

函数体的结束。

## 带命令行参数的版本

    int main(string[] args) {
        print "Program name:", args[0];
        print "Argument count:", args.length;

        for int i = 0; i < args.length; i++ {
            print "Arg", i, ":", args[i];
        }

        return 0;
    }

编译运行：

    yxg hello_args.yxg -r

输出示例：

    Program name: output.exe
    Argument count: 1
    Arg 0: output.exe

如果带参数运行：

    output.exe foo bar

输出：

    Program name: output.exe
    Argument count: 3
    Arg 0: output.exe
    Arg 1: foo
    Arg 2: bar

- args 是一个 string[] 类型，即字符串数组
- args[0] 始终是程序自身的路径
- args.length 返回数组的元素个数

## print 的多种用法

print 可以接受任意数量和类型的参数，自动格式化输出：

    int main() {
        print 42;
        print 3.14159;
        print true;
        print false;
        print 'A';
        print "The answer is", 42;
        print "3 + 5 =", 3 + 5;

        int[] nums = {1, 2, 3, 4, 5};
        print nums;

        return 0;
    }

输出：

    42
    3.141590
    true
    false
    A
    The answer is 42
    3 + 5 = 8
    [1, 2, 3, 4, 5]

## 注释

YXG 支持两种注释：

    // 这是单行注释，从 // 到行尾都被忽略

    int main() {
        print "Hello";  // 行尾注释

        /*
           这是多行注释
           可以跨越多行
           编译器完全忽略
        */

        return 0;
    }

## 变量与基本运算

    int main() {
        int a = 10;
        int b = 20;
        int sum = a + b;

        print "a =", a;
        print "b =", b;
        print "sum =", sum;

        float pi = 3.14159;
        float radius = 5.0;
        float area = pi * radius * radius;

        print "radius =", radius;
        print "area =", area;

        string greeting = "Hello";
        string name = "YXG";
        string message = greeting + ", " + name + "!";

        print message;

        bool isGreater = a > b;
        print "a > b ?", isGreater;

        return 0;
    }

输出：

    a = 10
    b = 20
    sum = 30
    radius = 5.000000
    area = 78.539750
    Hello, YXG!
    a > b ? false

## 程序结构规范

一个典型的 YXG 程序遵循以下结构：

    // 1. import 语句（可选）
    import "math.yxg";

    // 2. 枚举定义（可选）
    enum Status { Active, Inactive }

    // 3. 类定义（可选）
    class MyClass {
        int value;
        pub void show() { print value; }
    }

    // 4. 函数定义（可选）
    int helper(int x) {
        return x * 2;
    }

    // 5. main 函数（必须）
    int main() {
        return 0;
    }

其中只有 main 函数是必须的，其他部分都是可选的。

## 常见错误

### 忘记分号

    int main() {
        print "Hello"       // 错误：缺少分号
        return 0;
    }

编译器报错：

    error[E2001]: expected ';' before 'return'

### 忘记 return

    int main() {
        print "Hello";
        // 错误：int 类型的 main 函数必须有 return 语句
    }

编译器报错：

    error[E3019]: non-void function must return a value

### 字符串用单引号

    int main() {
        print 'Hello';      // 错误：单引号只能用于单个字符
        return 0;
    }

### main 函数写错名字

    int Main() {             // 错误：大小写敏感
        print "Hello";
        return 0;
    }

编译器报错：

    error[E3011]: no main function found

## 继续阅读

掌握了基本的程序结构后，下一章将详细介绍 YXG 的命令行参数和编译选项。