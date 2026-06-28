# 类与面向对象：成员访问控制

## 三种访问级别

YXG 提供三种访问控制修饰符，用于控制类成员的可见性：

| 修饰符 | 含义 | 类内部 | 子类 | 外部 |
|--------|------|--------|------|------|
| pub | 公开 | ✓ | ✓ | ✓ |
| pro | 受保护 | ✓ | ✓ | ✗ |
| pri | 私有 | ✓ | ✗ | ✗ |

    class Example {
        pub int publicField;
        pro int protectedField;
        pri int privateField;

        pub void publicMethod() {
            print "任何人都可以调用";
        }

        pro void protectedMethod() {
            print "只有自己和子类可以调用";
        }

        pri void privateMethod() {
            print "只有自己可以调用";
        }
    }

## pub：公开成员

pub 成员在任何地方都可以访问：

    class Person {
        pub string name;
        pub int age;

        pub void greet() {
            print "Hello, I'm", name;
        }
    }

    int main() {
        Person p;
        p.name = "Alice";        // 可以直接访问
        p.age = 25;              // 可以直接访问
        p.greet();               // 可以直接调用
        return 0;
    }

## pro：受保护成员

pro 成员只能在本类和子类中访问，外部代码不可见：

    class Animal {
        pro string species;

        pub Animal(string species) {
            this.species = species;
        }

        pro void breathe() {
            print species, " is breathing";
        }
    }

    class Dog : Animal {
        pub Dog() {
            this.species = "Canine";     // 子类可以访问 pro 字段
        }

        pub void pant() {
            breathe();                    // 子类可以调用 pro 方法
            print "Dog is panting";
        }
    }

    int main() {
        Dog d = new Dog();
        d.pant();

        // d.species = "xxx";            // 错误！外部不能访问 pro 成员
        // d.breathe();                  // 错误！外部不能调用 pro 方法

        return 0;
    }

## pri：私有成员

pri 成员仅在本类内部可访问。编译器会自动为 pri 字段生成 getter 和 setter 方法：

    class BankAccount {
        pri float balance;
        pub string owner;

        pub BankAccount(string owner, float initial) {
            this.owner = owner;
            this.balance = initial;
        }

        pub void deposit(float amount) {
            if amount > 0 {
                balance = balance + amount;
            }
        }

        pub void withdraw(float amount) {
            if amount > 0 && amount <= balance {
                balance = balance - amount;
            }
        }

        // 编译器自动生成：
        // float getBalance() { return this.balance; }
        // void setBalance(float value) { this.balance = value; }
    }

    int main() {
        BankAccount acc = new BankAccount("Alice", 1000.0);

        // acc.balance = 5000.0;         // 错误！不能直接访问 pri 字段

        // 通过自动生成的 getter/setter 访问
        float bal = acc.getBalance();
        print "当前余额:", bal;

        acc.setBalance(2000.0);           // 通过 setter 修改
        print "修改后余额:", acc.getBalance();

        return 0;
    }

### getter 和 setter 的命名规则

对于 pri 字段，编译器自动生成的 getter 和 setter 命名规则如下：

| 字段名 | getter | setter |
|--------|--------|--------|
| balance | getBalance() | setBalance(value) |
| name | getName() | setName(value) |
| maxSize | getMaxSize() | setMaxSize(value) |

规则是：get/set + 首字母大写的字段名。

## 方法的访问控制

方法同样可以使用三种修饰符：

    class Database {
        pri string connectionString;

        pub Database(string conn) {
            this.connectionString = conn;
        }

        pub void query(string sql) {
            if validateSQL(sql) {
                executeQuery(sql);
            } else {
                print "Invalid SQL";
            }
        }

        pro bool validateSQL(string sql) {
            // 子类可以重写验证逻辑
            return sql != "";
        }

        pri void executeQuery(string sql) {
            // 只有本类可以执行
            print "Executing:", sql;
        }
    }

## 访问控制在继承中的行为

    class Base {
        pub int a = 1;
        pro int b = 2;
        pri int c = 3;
    }

    class Derived : Base {
        pub void test() {
            print a;                 // 可以：pub 继承后仍然是 pub
            print b;                 // 可以：pro 对子类可见
            // print c;              // 错误！pri 对子类不可见
            print getC();            // 正确：通过自动生成的 getter
        }
    }

    int main() {
        Derived d;
        d.test();

        print d.a;                   // 可以：pub
        // print d.b;                // 错误：pro 对外部不可见
        // print d.c;                // 错误：pri 对外部不可见
        print d.getC();              // 正确：通过 getter

        return 0;
    }

## 完整示例：封装良好的计数器

    class Counter {
        pri int value;
        pri int maxValue;
        pub string label;

        pub Counter(string label, int max) {
            this.label = label;
            this.maxValue = max;
            this.value = 0;
        }

        pub void increment() {
            if value < maxValue {
                value = value + 1;
            } else {
                print label, ": 已达到最大值";
            }
        }

        pub void decrement() {
            if value > 0 {
                value = value - 1;
            } else {
                print label, ": 已经是最小值";
            }
        }

        pub int getValue() {
            return value;
        }

        pub void reset() {
            value = 0;
        }

        pro bool isAtLimit() {
            return value == 0 || value == maxValue;
        }
    }

    int main() {
        Counter c = new Counter("点击次数", 5);

        c.increment();
        c.increment();
        c.increment();
        print c.label, ":", c.getValue();

        c.increment();
        c.increment();
        c.increment();               // 超过最大值

        c.reset();
        print "重置后:", c.getValue();

        return 0;
    }

输出：

    点击次数: 3
    点击次数: 已达到最大值
    重置后: 0

## 常见错误

### 从外部访问 pro 成员

    class Base {
        pro int value;
    }

    int main() {
        Base b;
        // b.value = 10;            // 错误！pro 对外部不可见
        return 0;
    }

### 从外部直接访问 pri 字段

    class Foo {
        pri int secret;
    }

    int main() {
        Foo f;
        // f.secret = 42;           // 错误！
        f.setSecret(42);            // 正确：通过 setter
        return 0;
    }

### 子类直接访问 pri 字段

    class Base {
        pri int data;
    }

    class Derived : Base {
        pub void test() {
            // data = 100;           // 错误！pri 对子类不可见
            setData(100);            // 正确：通过 setter
        }
    }

## 继续阅读

掌握了访问控制后，下一章将学习类的继承。