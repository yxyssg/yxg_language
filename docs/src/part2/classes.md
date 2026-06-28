# 类与面向对象

## 概述

类是 YXG 面向对象编程的核心。类将数据（字段）和行为（方法）封装在一起，支持构造函数、访问控制、继承和多态。

## 定义一个简单的类

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

## 类的基本组成

一个完整的类可以包含以下成员：

    class MyClass {
        // 字段
        pub int publicField;
        pro int protectedField;
        pri int privateField;

        // 构造函数
        pub MyClass(int value) {
            this.publicField = value;
        }

        // 方法
        pub void doSomething() {
            print "doing something";
        }

        pro int calculate() {
            return publicField * 2;
        }

        pri void internalWork() {
            print "internal";
        }
    }

## 创建和使用对象

YXG 中有两种创建对象的方式：

### 无构造函数时直接声明

    class Simple {
        int value;
        string name;
    }

    int main() {
        Simple s;
        s.value = 42;
        s.name = "test";
        print s.value, s.name;
        return 0;
    }

### 有构造函数时使用 new

    class Point {
        int x;
        int y;

        pub Point(int x, int y) {
            this.x = x;
            this.y = y;
        }
    }

    int main() {
        Point p = new Point(10, 20);
        print p.x, p.y;
        return 0;
    }

## 类中的 this

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

当参数名和字段名相同时，必须用 this 区分。名字不冲突时 this 可以省略。

## 访问控制

YXG 提供三种访问级别：

| 修饰符 | 类内部 | 子类 | 外部 |
|--------|--------|------|------|
| pub | ✓ | ✓ | ✓ |
| pro | ✓ | ✓ | ✗ |
| pri | ✓ | ✗ | ✗ |

    class Example {
        pub int a;        // 任何地方可访问
        pro int b;        // 本类和子类可访问
        pri int c;        // 仅本类可访问，自动生成 getter/setter
    }

pri 字段会自动生成 getter 和 setter，例如 pri int value 会生成 getValue() 和 setValue(int v)。

## 继承

YXG 支持多继承，子类用冒号指定父类，多个父类用逗号分隔：

    class Animal {
        string name;

        pub void speak() {
            print name, " makes a sound";
        }
    }

    class Flyable {
        pub void fly() {
            print "Flying...";
        }
    }

    class Bird : Animal, Flyable {
        pub void chirp() {
            print name, " chirps!";
        }
    }

    int main() {
        Bird b;
        b.name = "Tweety";
        b.speak();
        b.fly();
        b.chirp();
        return 0;
    }

子类继承父类所有 pub 和 pro 成员。子类可以重写父类方法。

## 类与结构体

YXG 没有单独的 struct 关键字。没有方法的类本质上就是结构体：

    // 这就像一个 C 结构体
    class Point3D {
        float x;
        float y;
        float z;
    }

    int main() {
        Point3D p;
        p.x = 1.0;
        p.y = 2.0;
        p.z = 3.0;
        return 0;
    }

## 完整示例：学生管理系统

    class Student {
        pri int id;
        pub string name;
        pri float score;

        pub Student(int id, string name, float score) {
            this.id = id;
            this.name = name;
            this.score = score;
        }

        pub int getId() {
            return id;
        }

        pub float getScore() {
            return score;
        }

        pub void setScore(float s) {
            if s >= 0 && s <= 100 {
                score = s;
            }
        }

        pub string getGrade() {
            if score >= 90 { return "A"; }
            if score >= 80 { return "B"; }
            if score >= 70 { return "C"; }
            if score >= 60 { return "D"; }
            return "F";
        }

        pub void printInfo() {
            print "ID:", id, "姓名:", name, "分数:", score, "等级:", getGrade();
        }
    }

    int main() {
        Student s1 = new Student(1001, "Alice", 92.5);
        Student s2 = new Student(1002, "Bob", 78.0);
        Student s3 = new Student(1003, "Charlie", 55.5);

        s1.printInfo();
        s2.printInfo();
        s3.printInfo();

        s3.setScore(68.0);
        print "Charlie 补考后:";
        s3.printInfo();

        return 0;
    }

输出：

    ID: 1001 姓名: Alice 分数: 92.500000 等级: A
    ID: 1002 姓名: Bob 分数: 78.000000 等级: C
    ID: 1003 姓名: Charlie 分数: 55.500000 等级: F
    Charlie 补考后:
    ID: 1003 姓名: Charlie 分数: 68.000000 等级: D

## 章节导航

| 章节 | 内容 |
|------|------|
| 定义类 | 类的声明、字段、方法、构造函数、this |
| 成员访问控制 | pub/pro/pri 详解、getter/setter |
| 继承 | 单继承、多继承、方法重写、父类调用 |

## 继续阅读

下一章将详细介绍类的定义语法。