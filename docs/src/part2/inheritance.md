# 类与面向对象：继承

## 基本语法

YXG 支持多继承。子类通过冒号指定父类，多个父类用逗号分隔：

    class Animal {
        string name;

        pub void speak() {
            print name, " makes a sound";
        }
    }

    class Dog : Animal {
        pub void bark() {
            print name, " barks: Woof!";
        }
    }

    int main() {
        Dog d;
        d.name = "Buddy";
        d.speak();
        d.bark();
        return 0;
    }

输出：

    Buddy makes a sound
    Buddy barks: Woof!

子类继承父类的所有公开（pub）和受保护（pro）字段与方法。私有（pri）成员不参与继承，但通过自动生成的 getter/setter 仍可间接访问。

## 多继承

YXG 支持多继承，用逗号分隔多个父类：

    class Flyable {
        pub void fly() {
            print "Flying...";
        }
    }

    class Swimmable {
        pub void swim() {
            print "Swimming...";
        }
    }

    class Duck : Animal, Flyable, Swimmable {
        pub void quack() {
            print name, " quacks: Quack!";
        }
    }

    int main() {
        Duck d;
        d.name = "Donald";
        d.speak();
        d.fly();
        d.swim();
        d.quack();
        return 0;
    }

输出：

    Donald makes a sound
    Flying...
    Swimming...
    Donald quacks: Quack!

多继承时如果多个父类有同名方法，子类必须自己定义该方法来消除歧义。

## 方法重写

子类可以重写父类的方法：

    class Animal {
        string name;

        pub void speak() {
            print name, " makes a sound";
        }
    }

    class Cat : Animal {
        pub void speak() {
            print name, " meows: Meow!";
        }
    }

    int main() {
        Cat c;
        c.name = "Kitty";
        c.speak();
        return 0;
    }

输出：

    Kitty meows: Meow!

重写时方法签名（名称和参数类型）必须与父类一致。

## 调用父类方法

在子类方法中，可以通过父类名调用被重写的父类方法：

    class Animal {
        string name;

        pub void speak() {
            print name, " makes a sound";
        }
    }

    class Dog : Animal {
        pub void speak() {
            Animal.speak();
            print name, " barks: Woof!";
        }
    }

    int main() {
        Dog d;
        d.name = "Buddy";
        d.speak();
        return 0;
    }

输出：

    Buddy makes a sound
    Buddy barks: Woof!

## 构造函数与继承

子类的构造函数需要负责初始化从父类继承的字段：

    class Point {
        int x;
        int y;

        pub Point(int x, int y) {
            this.x = x;
            this.y = y;
        }
    }

    class Point3D : Point {
        int z;

        pub Point3D(int x, int y, int z) {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        pub void display() {
            print "Point3D(", x, ",", y, ",", z, ")";
        }
    }

    int main() {
        Point3D p = new Point3D(10, 20, 30);
        p.display();
        return 0;
    }

输出：

    Point3D(10, 20, 30)

## 访问控制与继承

三种访问级别在继承中的行为：

    class Base {
        pub int a;
        pro int b;
        pri int c;
    }

    class Derived : Base {
        pub void test() {
            a = 1;
            b = 2;
            // c = 3;          // 错误！pri 不可直接访问
            setC(3);           // 正确：通过自动生成的 setter
        }
    }

    int main() {
        Derived d;
        d.test();
        d.a = 10;
        // d.b = 20;           // 错误（pro 对外部不可见）
        // d.c = 30;           // 错误（pri 对外部不可见）
        d.setC(30);            // 正确：通过 setter
        return 0;
    }

## 完整示例：图形类层次结构

    class Shape {
        pub string name;

        pub Shape(string name) {
            this.name = name;
        }

        pub float area() {
            return 0.0;
        }

        pub void describe() {
            print name, " area:", area();
        }
    }

    class Circle : Shape {
        float radius;

        pub Circle(float radius) {
            this.name = "Circle";
            this.radius = radius;
        }

        pub float area() {
            return 3.14159 * radius * radius;
        }
    }

    class Rectangle : Shape {
        float width;
        float height;

        pub Rectangle(float width, float height) {
            this.name = "Rectangle";
            this.width = width;
            this.height = height;
        }

        pub float area() {
            return width * height;
        }
    }

    class Square : Rectangle {
        pub Square(float side) {
            this.name = "Square";
            this.width = side;
            this.height = side;
        }
    }

    int main() {
        Circle c = new Circle(5.0);
        Rectangle r = new Rectangle(4.0, 6.0);
        Square s = new Square(3.0);

        c.describe();
        r.describe();
        s.describe();

        return 0;
    }

输出：

    Circle area: 78.539750
    Rectangle area: 24.000000
    Square area: 9.000000

## 继承与字段布局

子类实例在内存中包含父类的所有字段。字段按继承顺序排列：

    class A { int x; }
    class B : A { int y; }
    class C : B { int z; }

多继承时，字段按父类声明顺序依次排列。

## 常见错误

### 访问私有字段

    class Base {
        pri int secret;
    }

    class Derived : Base {
        pub void test() {
            // secret = 42;    // 错误！pri 不可直接访问
            setSecret(42);     // 正确
        }
    }

### 方法签名不匹配

    class Base {
        pub void greet(string name) { }
    }

    class Derived : Base {
        // pub void greet(int x) { }  // 这不是重写，是新方法
    }

重写时参数类型必须完全一致。

### 循环继承

    // class A : B { }
    // class B : A { }          // 错误！循环继承

编译器会检测循环继承并报错。

## 继续阅读

掌握了继承后，下一章将学习枚举类型。