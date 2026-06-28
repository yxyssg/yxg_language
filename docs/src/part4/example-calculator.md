# 实战教程：实现一个命令行计算器

## 目标

使用 YXG 的类、枚举、String 库和 System 库，实现一个支持四则运算的命令行计算器。

## 完整代码

    import yxg.String;
    import yxg.Math;
    import yxg.System;

    // 操作类型枚举
    enum Operation {
        ADD = 1,
        SUB = 2,
        MUL = 3,
        DIV = 4
    }

    // 计算器类
    class Calculator {
        pri float memory;
        pri int opCount;

        pub Calculator() {
            this.memory = 0.0;
            this.opCount = 0;
        }

        pub float add(float a, float b) {
            opCount++;
            return a + b;
        }

        pub float sub(float a, float b) {
            opCount++;
            return a - b;
        }

        pub float mul(float a, float b) {
            opCount++;
            return a * b;
        }

        pub float div(float a, float b) {
            if b == 0.0 {
                print "错误: 除数不能为零";
                return 0.0;
            }
            opCount++;
            return a / b;
        }

        pub float power(int base, int exp) {
            Math m = new Math();
            opCount++;
            return m.pow(base, exp);
        }

        pub float sqrt(float x) {
            Math m = new Math();
            opCount++;
            return m.sqrt(x);
        }

        pub void store(float value) {
            memory = value;
            print "已存储:", memory;
        }

        pub float recall() {
            return memory;
        }

        pub void clearMemory() {
            memory = 0.0;
            print "存储器已清零";
        }

        pub int getOpCount() {
            return opCount;
        }

        pub void resetCount() {
            opCount = 0;
        }
    }

    // 显示菜单
    void showMenu() {
        print "";
        print "========== YXG 计算器 ==========";
        print " 1. 加法";
        print " 2. 减法";
        print " 3. 乘法";
        print " 4. 除法";
        print " 5. 幂运算";
        print " 6. 平方根";
        print " 7. 存储结果";
        print " 8. 读取存储";
        print " 9. 清除存储";
        print "10. 显示统计";
        print " 0. 退出";
        print "================================";
        print "请选择操作:";
    }

    // 读取两个操作数
    void readTwoOperands(float a, float b) {
        print "请输入第一个数:";
        input "%f", &a;
        print "请输入第二个数:";
        input "%f", &b;
    }

    // 读取一个操作数
    void readOneOperand(float x) {
        print "请输入数值:";
        input "%f", &x;
    }

    int main() {
        Calculator calc = new Calculator();
        String str = new String();
        System sys = new System();

        bool running = true;
        float a, b, result;
        int choice;

        while running {
            showMenu();
            input "%d", &choice;

            switch choice {
                case 1:
                    readTwoOperands(a, b);
                    result = calc.add(a, b);
                    print a, " + ", b, " = ", result;
                    break;

                case 2:
                    readTwoOperands(a, b);
                    result = calc.sub(a, b);
                    print a, " - ", b, " = ", result;
                    break;

                case 3:
                    readTwoOperands(a, b);
                    result = calc.mul(a, b);
                    print a, " * ", b, " = ", result;
                    break;

                case 4:
                    readTwoOperands(a, b);
                    result = calc.div(a, b);
                    if b != 0.0 {
                        print a, " / ", b, " = ", result;
                    }
                    break;

                case 5:
                    int base, exp;
                    print "请输入底数:";
                    input "%d", &base;
                    print "请输入指数:";
                    input "%d", &exp;
                    result = calc.power(base, exp);
                    print base, " ^ ", exp, " = ", result;
                    break;

                case 6:
                    readOneOperand(a);
                    if a >= 0.0 {
                        result = calc.sqrt(a);
                        print "sqrt(", a, ") = ", result;
                    } else {
                        print "错误: 不能对负数开平方根";
                    }
                    break;

                case 7:
                    print "请输入要存储的值:";
                    input "%f", &a;
                    calc.store(a);
                    break;

                case 8:
                    print "存储的值:", calc.recall();
                    break;

                case 9:
                    calc.clearMemory();
                    break;

                case 10:
                    print "已执行运算次数:", calc.getOpCount();
                    break;

                case 0:
                    print "感谢使用 YXG 计算器！";
                    print "共执行", calc.getOpCount(), "次运算";
                    running = false;
                    break;

                default:
                    print "无效选择，请重新输入";
            }
        }

        return 0;
    }

## 运行示例

    ========== YXG 计算器 ==========
     1. 加法
     2. 减法
     3. 乘法
     4. 除法
     5. 幂运算
     6. 平方根
     7. 存储结果
     8. 读取存储
     9. 清除存储
    10. 显示统计
     0. 退出
    ================================
    请选择操作: 1
    请输入第一个数: 15.5
    请输入第二个数: 4.5
    15.500000 + 4.500000 = 20.000000

    ========== YXG 计算器 ==========
    请选择操作: 6
    请输入数值: 144
    sqrt(144.000000) = 12.000000

    ========== YXG 计算器 ==========
    请选择操作: 7
    请输入要存储的值: 100
    已存储: 100.000000

    ========== YXG 计算器 ==========
    请选择操作: 8
    存储的值: 100.000000

    ========== YXG 计算器 ==========
    请选择操作: 10
    已执行运算次数: 2

    ========== YXG 计算器 ==========
    请选择操作: 0
    感谢使用 YXG 计算器！
    共执行 2 次运算

## 知识点总结

这个程序涵盖了以下 YXG 语言特性：

- 三个标准库的导入和使用（String、Math、System）
- 枚举定义和 switch 配合使用
- 类的封装（私有字段、公开方法）
- 构造函数初始化
- input 读取用户输入
- while 循环实现菜单交互
- switch 多分支选择
- 浮点运算和错误处理
- 存储器功能（状态保持）

## 继续阅读

恭喜你完成了全部实战教程！接下来可以查阅附录中的错误码参考等。