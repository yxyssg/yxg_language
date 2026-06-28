# 类与面向对象：定义类

## 基本语法

类是 YXG 面向对象编程的核心。定义类使用 class 关键字：

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

类将数据（字段）和行为（方法）封装在一起。

## 字段

字段是类中声明的变量，每个实例拥有自己的一份副本：

    class Person {
        string name;
        int age;
        float height;
    }

    int main() {
        Person p1;
        p1.name = "Alice";
        p1.age = 25;
        p1.height = 165.5;

        Person p2;
        p2.name = "Bob";
        p2.age = 30;
        p2.height = 178.0;

        print p1.name, p1.age, p1.height;
        print p2.name, p2.age, p2.height;
        return 0;
    }

字段的默认值与其类型对应：int 为 0，float 为 0.0，string 为 null，bool 为 false。

## 方法

方法是定义在类内部的函数，可以访问类的字段：

    class Counter {
        int value;

        pub void increment() {
            value = value + 1;
        }

        pub void decrement() {
            value = value - 1;
        }

        pub int getValue() {
            return value;
        }

        pub void reset() {
            value = 0;
        }
    }

    int main() {
        Counter c;
        c.increment();
        c.increment();
        c.increment();
        print c.getValue();

        c.decrement();
        print c.getValue();

        c.reset();
        print c.getValue();

        return 0;
    }

方法使用 pub 标记为公开，外部代码才能调用。没有 pub 的方法默认为私有，仅类内部可用。

## this 关键字

在方法内部，this 指代当前实例：

    class Rectangle {
        int width;
        int height;

        pub void setSize(int width, int height) {
            this.width = width;
            this.height = height;
        }

        pub int area() {
            return this.width * this.height;
        }
    }

当参数名和字段名相同时，必须用 this 区分。如果名字不冲突，this 可以省略。

## 构造函数

构造函数是与类同名的方法，在使用 new 创建实例时自动调用：

    class Point {
        int x;
        int y;

        pub Point(int x, int y) {
            this.x = x;
            this.y = y;
        }

        pub void print() {
            print "Point(", x, ", ", y, ")";
        }
    }

    int main() {
        Point p = new Point(10, 20);
        p.print();

        Point q = new Point(5, 15);
        q.print();

        return 0;
    }

输出：

    Point(10, 20)
    Point(5, 15)

构造函数没有返回类型（连 void 也不写）。使用 new 调用构造函数创建实例。

如果没有定义构造函数，可以使用无参的默认构造：

    class Simple {
        int value;
    }

    int main() {
        Simple s;
        s.value = 42;
        print s.value;
        return 0;
    }

## 访问控制

YXG 提供三种访问级别：

    class Example {
        pub int publicField;
        pro int protectedField;
        pri int privateField;

        pub void publicMethod() {
            print "public";
        }

        pro void protectedMethod() {
            print "protected";
        }

        pri void privateMethod() {
            print "private";
        }
    }

pri 字段的特殊之处：编译器会自动生成 getter 和 setter 方法。例如 pri int value 会生成：

    int getValue() { return this.value; }
    void setValue(int value) { this.value = value; }

外部代码通过 getter/setter 访问私有字段：

    Example e;
    e.setValue(100);
    int v = e.getValue();

## 完整示例：银行账户

    class BankAccount {
        pri string accountNumber;
        pri float balance;
        pub string ownerName;

        pub BankAccount(string owner, string number) {
            this.ownerName = owner;
            this.accountNumber = number;
            this.balance = 0.0;
        }

        pub void deposit(float amount) {
            if amount > 0 {
                balance = balance + amount;
                print "存入:", amount, "余额:", balance;
            }
        }

        pub bool withdraw(float amount) {
            if amount > 0 && amount <= balance {
                balance = balance - amount;
                print "取出:", amount, "余额:", balance;
                return true;
            }
            print "余额不足";
            return false;
        }

        pub float getBalance() {
            return balance;
        }

        pub string getAccountNumber() {
            return accountNumber;
        }
    }

    int main() {
        BankAccount acc = new BankAccount("Alice", "6222021234567890");

        print "账户:", acc.getAccountNumber();
        print "户主:", acc.ownerName;

        acc.deposit(1000.0);
        acc.deposit(500.0);
        acc.withdraw(300.0);
        acc.withdraw(2000.0);

        print "最终余额:", acc.getBalance();
        return 0;
    }

输出：

    账户: 6222021234567890
    户主: Alice
    存入: 1000.000000 余额: 1000.000000
    存入: 500.000000 余额: 1500.000000
    取出: 300.000000 余额: 1200.000000
    余额不足
    最终余额: 1200.000000

## 常见错误

### 忘记 new

    Point p = new Point(1, 2);     // 正确
    // Point p = Point(1, 2);      // 错误！需要 new

### 访问私有成员

    class Foo {
        pri int secret;
    }

    int main() {
        Foo f;
        // f.secret = 42;           // 错误！secret 是私有的
        f.setSecret(42);            // 正确：通过自动生成的 setter
        return 0;
    }

### 构造函数没有返回类型

    class Foo {
        // pub void Foo() { ... }   // 错误！构造函数不能有 void
        pub Foo() { ... }           // 正确
    }

## 继续阅读

掌握了类的基本定义后，下一章将学习类的继承。