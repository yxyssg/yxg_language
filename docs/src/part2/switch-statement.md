# 控制流：switch / case

## 基本语法

switch 语句根据一个整数值选择执行不同的分支。条件直接跟在 switch 后面，不需要括号：

    int main() {
        int day = 3;

        switch day {
            case 1: print "Monday"; break;
            case 2: print "Tuesday"; break;
            case 3: print "Wednesday"; break;
            case 4: print "Thursday"; break;
            case 5: print "Friday"; break;
            case 6: print "Saturday"; break;
            case 7: print "Sunday"; break;
            default: print "Invalid day";
        }

        return 0;
    }

输出：

    Wednesday

## switch 详解

### 条件表达式

switch 的条件必须是整数类型（int、uint、char、bool）：

    int main() {
        char grade = 'B';

        switch grade {
            case 'A': print "优秀"; break;
            case 'B': print "良好"; break;
            case 'C': print "中等"; break;
            case 'D': print "及格"; break;
            case 'F': print "不及格"; break;
            default: print "无效等级";
        }

        return 0;
    }

float 和 string 不能用于 switch。

### case 值

case 的值必须是编译时常量（字面量或枚举成员）：

    int main() {
        int x = 2;

        switch x {
            case 1:
                print "one";
                break;
            case 2:
                print "two";
                break;
            // case x:        // 错误！x 是变量，不是常量
            //     break;
        }

        return 0;
    }

### break 语句

每个 case 必须显式使用 break 退出，YXG 不会自动穿透：

    int main() {
        int x = 1;

        switch x {
            case 1:
                print "one";
                break;
            case 2:
                print "two";
                break;
        }

        return 0;
    }

多个 case 共享同一段代码时，让它们连续排列：

    int main() {
        int day = 6;

        switch day {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                print "工作日";
                break;
            case 6:
            case 7:
                print "周末";
                break;
            default:
                print "无效";
        }

        return 0;
    }

输出：

    周末

### default 分支

default 分支在所有 case 都不匹配时执行，它是可选的：

    int main() {
        int code = 999;

        switch code {
            case 200: print "成功"; break;
            case 404: print "未找到"; break;
            case 500: print "服务器错误"; break;
            default: print "未知状态码:", code;
        }

        return 0;
    }

输出：

    未知状态码: 999

## 与枚举配合

    enum Color { Red, Green, Blue, Yellow }

    int main() {
        Color c = Color.Green;

        switch c {
            case Color.Red:    print "红色"; break;
            case Color.Green:  print "绿色"; break;
            case Color.Blue:   print "蓝色"; break;
            case Color.Yellow: print "黄色"; break;
            default:           print "未知颜色";
        }

        return 0;
    }

## 嵌套 switch

switch 可以嵌套，内层的 break 只跳出内层 switch：

    int main() {
        int category = 1;
        int item = 2;

        switch category {
            case 1:
                print "电子产品";
                switch item {
                    case 1: print "手机"; break;
                    case 2: print "电脑"; break;
                    default: print "其他电子产品";
                }
                break;
            case 2:
                print "食品";
                switch item {
                    case 1: print "水果"; break;
                    case 2: print "蔬菜"; break;
                    default: print "其他食品";
                }
                break;
            default:
                print "未知分类";
        }

        return 0;
    }

输出：

    电子产品
    电脑

## switch 与 if 的选择

| 场景 | 推荐 |
|------|------|
| 判断范围（>、<） | if / else if |
| 判断相等（多个离散值） | switch |
| 复杂逻辑条件 | if / else if |
| 枚举类型匹配 | switch |

## 常见错误

### 忘记 break

    switch x {
        case 1:
            print "one";
            // 忘记 break，编译器会报错
        case 2:
            print "two";
            break;
    }

### 重复的 case 值

    switch x {
        case 1: print "one"; break;
        case 1: print "also one"; break;    // 错误！
    }

编译器报错：

    error[E3018]: duplicate case value '1'

### 使用非整数类型

    string s = "hello";
    // switch s { ... }         // 错误！string 不能用于 switch

## 继续阅读

掌握了 switch 语句后，下一章将学习 while 和 do-while 循环。