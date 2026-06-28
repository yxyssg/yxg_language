# 函数

## 定义函数

函数是组织代码的基本单元。定义函数的语法：

    返回类型 函数名(参数列表) {
        函数体
        return 返回值;
    }

一个简单的例子：

    int add(int a, int b) {
        return a + b;
    }

    int main() {
        int result = add(3, 5);
        print "3 + 5 =", result;
        return 0;
    }

输出：

    3 + 5 = 8

## 参数

函数可以接收零个或多个参数，每个参数需要声明类型和名称：

    // 无参数
    void sayHello() {
        print "Hello!";
    }

    // 一个参数
    void greet(string name) {
        print "Hello,", name;
    }

    // 多个参数
    int max(int a, int b, int c) {
        if a >= b && a >= c { return a; }
        if b >= a && b >= c { return b; }
        return c;
    }

参数之间用逗号分隔。调用时实参按位置顺序传递：

    int main() {
        sayHello();
        greet("Alice");
        int m = max(10, 25, 15);
        print "最大值:", m;
        return 0;
    }

## 返回值

函数通过 return 语句返回值。返回类型必须与声明一致：

    int getNumber() {
        return 42;
    }

    float getPi() {
        return 3.14159;
    }

    string getMessage() {
        return "Hello, World!";
    }

    bool isPositive(int n) {
        return n > 0;
    }

void 函数没有返回值，return 语句可选：

    void printTwice(string s) {
        print s;
        print s;
    }

    void maybeReturn(int x) {
        if x < 0 {
            return;
        }
        print "x is non-negative:", x;
    }

## 数组参数与返回值

函数可以接收和返回数组：

    int sum(int[] arr) {
        int total = 0;
        for int v : arr {
            total += v;
        }
        return total;
    }

    int[] makeArray(int size) {
        int[] arr = new int[size];
        for int i = 0; i < size; i++ {
            arr[i] = i * 2;
        }
        return arr;
    }

    int main() {
        int[] nums = {1, 2, 3, 4, 5};
        int s = sum(nums);
        print "总和:", s;

        int[] evens = makeArray(5);
        for int v : evens {
            print v;
        }

        return 0;
    }

输出：

    总和: 15
    0
    2
    4
    6
    8

多维数组也可以作为参数：

    void printMatrix(int[][] matrix) {
        for int[] row : matrix {
            for int elem : row {
                print elem;
            }
        }
    }

## 函数重载

YXG 不支持函数重载。同一作用域内函数名必须唯一：

    // int add(int a, int b) { return a + b; }
    // float add(float a, float b) { return a + b; }   // 错误！重复定义

如果需要处理不同类型，使用不同的函数名：

    int addInt(int a, int b) { return a + b; }
    float addFloat(float a, float b) { return a + b; }

## 递归

函数可以调用自身：

    int factorial(int n) {
        if n <= 1 {
            return 1;
        }
        return n * factorial(n - 1);
    }

    int fibonacci(int n) {
        if n <= 1 {
            return n;
        }
        return fibonacci(n - 1) + fibonacci(n - 2);
    }

    int main() {
        print "5! =", factorial(5);
        print "fib(7) =", fibonacci(7);
        return 0;
    }

输出：

    5! = 120
    fib(7) = 13

递归深度受限于栈空间。YXG 没有尾递归优化，深度递归可能导致栈溢出。

## main 函数

main 函数是程序的入口点，每个程序必须有且仅有一个。返回类型必须是 int，return 0 表示正常退出。

### 无参数版本

    int main() {
        print "Hello, YXG!";
        return 0;
    }

### 接收命令行参数

main 函数可以声明为接收 string[] 参数来获取命令行参数：

    int main(string[] args) {
        print "参数个数:", args.length;
        for int i = 0; i < args.length; i++ {
            print "args[", i, "] =", args[i];
        }
        return 0;
    }

args[0] 始终是程序自身的路径，后续元素是用户传递的参数。

### 通过 System 库获取命令行参数

除了在 main 函数签名中声明参数外，也可以使用 System 标准库在程序的任何位置获取命令行参数：

    import yxg.System;

    int main() {
        System sys = new System();

        int count = sys.argc();
        print "参数个数:", count;

        for int i = 0; i < count; i++ {
            print "argv[", i, "] =", sys.argv(i);
        }

        sys.printArgs();

        return 0;
    }

System 库还提供了其他系统功能：

    import yxg.System;

    int main() {
        System sys = new System();

        int result = sys.system("dir");
        int now = sys.time();
        print "当前时间戳:", now;
        sys.sleep(1000);
        string path = sys.env("PATH");
        print "PATH:", path;

        return 0;
    }

## 函数声明与定义顺序

YXG 要求函数先定义后使用。如果函数 A 调用函数 B，B 必须在 A 之前定义：

    int add(int a, int b) {
        return a + b;
    }

    int main() {
        int result = add(3, 5);
        print result;
        return 0;
    }

如果顺序反了会报错：

    int main() {
        int result = add(3, 5);    // 错误！add 尚未定义
        print result;
        return 0;
    }

    int add(int a, int b) {
        return a + b;
    }

编译器报错：

    error[E3008]: function 'add' is not declared

YXG 没有前向声明机制。解决循环调用的方法是合理组织代码顺序，或将相互调用的函数放在同一个类中。

## 局部变量与作用域

函数内部声明的变量是局部变量，只在函数内可见：

    void myFunc() {
        int x = 10;
        print x;
    }

    int main() {
        myFunc();
        // print x;             // 错误！x 不在作用域内
        return 0;
    }

函数参数也是局部变量，传递方式是值传递：

    void test(int n) {
        n = n + 1;
        print "函数内:", n;
    }

    int main() {
        int a = 5;
        test(a);
        print "函数外:", a;
        return 0;
    }

输出：

    函数内: 6
    函数外: 5

对于基本类型，传递的是值的副本。对于数组和类实例，传递的是引用的副本（但引用指向同一对象）。

## 数组参数的传递

数组作为参数时传递的是引用，函数内修改数组会影响原数组：

    void doubleElements(int[] arr) {
        for int i = 0; i < arr.length; i++ {
            arr[i] = arr[i] * 2;
        }
    }

    int main() {
        int[] nums = {1, 2, 3, 4, 5};
        doubleElements(nums);

        for int v : nums {
            print v;
        }

        return 0;
    }

输出：

    2
    4
    6
    8
    10

## 常见错误

### 返回类型不匹配

    int getValue() {
        return "hello";              // 错误！
    }

编译器报错：

    error[E3019]: return type mismatch: expected 'int', got 'string'

### 缺少 return 语句

    int getValue() {
        int x = 10;
        // 忘记 return
    }

编译器报错：

    error[E3019]: non-void function must return a value

### 参数数量不匹配

    int add(int a, int b) { return a + b; }

    int main() {
        int x = add(5);              // 错误！
        return 0;
    }

编译器报错：

    error[E3009]: function 'add' expects 2 argument(s), got 1

### 参数类型不匹配

    void printNumber(int n) { print n; }

    int main() {
        printNumber("hello");        // 错误！
        return 0;
    }

编译器报错：

    error[E3003]: type mismatch: expected 'int', got 'string'

## 继续阅读

掌握了函数后，下一章将学习数组的详细用法。