# 控制流：if / else if / else

## 基本 if 语句

if 语句根据条件决定是否执行代码块。条件直接跟在 if 后面，不需要括号：

    int main() {
        int score = 85;

        if score >= 60 {
            print "及格";
        }

        return 0;
    }

条件必须是一个 bool 类型的表达式。int 不能直接作为条件：

    int x = 10;
    // if x { ... }            // 错误！x 是 int，不是 bool
    if x != 0 { ... }          // 正确

## if / else

    int main() {
        int score = 55;

        if score >= 60 {
            print "及格";
        } else {
            print "不及格";
        }

        return 0;
    }

else 部分是可选的。每个 if 最多有一个 else。

## if / else if / else

多个条件分支使用 else if：

    int main() {
        int score = 85;

        if score >= 90 {
            print "优秀";
        } else if score >= 80 {
            print "良好";
        } else if score >= 70 {
            print "中等";
        } else if score >= 60 {
            print "及格";
        } else {
            print "不及格";
        }

        return 0;
    }

else if 可以有任意多个。条件按顺序检查，第一个为真的分支被执行，后续分支被跳过。

## 嵌套 if

if 语句可以嵌套：

    int main() {
        int age = 25;
        bool hasTicket = true;

        if age >= 18 {
            if hasTicket {
                print "欢迎入场";
            } else {
                print "请先购票";
            }
        } else {
            print "未成年人不得入场";
        }

        return 0;
    }

## 代码块与单条语句

如果分支只有一条语句，花括号可以省略：

    int main() {
        int x = 10;

        if x > 5
            print "大于 5";
        else
            print "小于等于 5";

        return 0;
    }

但建议始终使用花括号，避免悬挂 else 问题。YXG 中 else 始终与最近的未匹配 if 配对。

## 条件表达式

条件可以是任意返回 bool 的表达式：

    int main() {
        int a = 10;
        int b = 20;

        if a < b { print "a < b"; }
        if a > 0 && b > 0 { print "both positive"; }
        if a < 0 || b < 0 { print "at least one negative"; }
        if !(a > b) { print "a <= b"; }

        bool isEven(int n) {
            return n % 2 == 0;
        }

        if isEven(a) { print "a is even"; }

        return 0;
    }

## 三目运算符与 if/else

对于简单的二选一赋值，三目运算符更简洁：

    int a = 10;
    int b = 20;

    int max = a > b ? a : b;

    int max2;
    if a > b {
        max2 = a;
    } else {
        max2 = b;
    }

需要执行多条语句时，必须使用 if/else。

## 常见模式

### 提前返回

    int getGradeLevel(int score) {
        if score < 0 || score > 100 {
            return -1;
        }

        if score >= 90 { return 5; }
        if score >= 80 { return 4; }
        if score >= 70 { return 3; }
        if score >= 60 { return 2; }
        return 1;
    }

### 范围检查

    int main() {
        int age = 25;

        if age >= 0 && age <= 12 {
            print "儿童";
        } else if age >= 13 && age <= 17 {
            print "青少年";
        } else if age >= 18 && age <= 64 {
            print "成人";
        } else {
            print "老年";
        }

        return 0;
    }

### 输入验证

    import yxg.String;

    int main() {
        String str = new String();
        string input = "admin";

        if input == "" {
            print "输入不能为空";
        } else if str.len(input) < 3 {
            print "输入太短";
        } else if str.len(input) > 20 {
            print "输入太长";
        } else {
            print "输入有效:", input;
        }

        return 0;
    }

## 常见错误

### 条件不是布尔类型

    int x = 10;
    // if x { ... }            // 错误！
    // error[E3020]: condition must be boolean, got 'int'

### 忘记花括号导致逻辑错误

    if score >= 60
        print "及格";
        print "恭喜！";         // 这行不受 if 控制，总会执行

缩进不决定代码块，只有花括号决定。建议始终使用花括号。

## 继续阅读

掌握了 if 语句后，下一章将学习 switch / case 多分支选择。