# 控制流：while / do-while 循环

## while 循环

while 循环在条件为真时重复执行代码块。条件直接跟在 while 后面，不需要括号：

    int main() {
        int count = 0;

        while count < 5 {
            print "count =", count;
            count++;
        }

        print "循环结束";
        return 0;
    }

输出：

    count = 0
    count = 1
    count = 2
    count = 3
    count = 4
    循环结束

如果条件一开始就是 false，循环体一次也不执行：

    int main() {
        int x = 10;

        while x < 5 {
            print "这行永远不会执行";
        }

        print "直接到这里";
        return 0;
    }

## 循环中的 break

break 语句立即终止循环：

    int main() {
        int i = 0;

        while true {
            i++;
            print i;

            if i >= 5 {
                break;
            }
        }

        print "循环结束, i =", i;
        return 0;
    }

输出：

    1
    2
    3
    4
    5
    循环结束, i = 5

## 循环中的 continue

continue 跳过本次循环的剩余部分，直接进入下一次条件检查：

    int main() {
        int i = 0;

        while i < 10 {
            i++;

            if i % 2 == 0 {
                continue;
            }

            print i;
        }

        return 0;
    }

输出：

    1
    3
    5
    7
    9

## do-while 循环

do-while 先执行循环体，再检查条件。循环体至少执行一次：

    int main() {
        int count = 0;

        do {
            print "count =", count;
            count++;
        } while count < 5;

        print "循环结束";
        return 0;
    }

即使条件一开始就是 false，循环体也会执行一次：

    int main() {
        int x = 10;

        do {
            print "至少执行一次, x =", x;
        } while x < 5;

        return 0;
    }

输出：

    至少执行一次, x = 10

## while 与 do-while 对比

    int main() {
        int a = 0;
        while a > 0 {
            print "while: 不会执行";
        }

        int b = 0;
        do {
            print "do-while: 会执行一次";
        } while b > 0;

        return 0;
    }

输出：

    do-while: 会执行一次

选择建议：
- 循环体可能一次都不执行，用 while
- 循环体至少执行一次（如输入验证），用 do-while

## 嵌套循环

循环可以嵌套，内层 break 只跳出最内层循环：

    int main() {
        int i = 0;

        while i < 3 {
            int j = 0;
            print "i =", i;

            while j < 3 {
                if i == 1 && j == 1 {
                    print "  提前退出内层循环";
                    break;
                }
                print "  j =", j;
                j++;
            }

            i++;
        }

        return 0;
    }

输出：

    i = 0
      j = 0
      j = 1
      j = 2
    i = 1
      j = 0
      提前退出内层循环
    i = 2
      j = 0
      j = 1
      j = 2

## 常见模式

### 求和

    int main() {
        int sum = 0;
        int i = 1;

        while i <= 100 {
            sum += i;
            i++;
        }

        print "1 到 100 的和:", sum;
        return 0;
    }

### 查找

    int main() {
        int[] arr = {3, 7, 2, 9, 5};
        int target = 9;
        int i = 0;
        bool found = false;

        while i < arr.length {
            if arr[i] == target {
                found = true;
                break;
            }
            i++;
        }

        if found {
            print "找到", target, "在索引", i;
        } else {
            print "未找到", target;
        }

        return 0;
    }

### 最大公约数（辗转相除法）

    int main() {
        int a = 48;
        int b = 18;

        while b != 0 {
            int temp = b;
            b = a % b;
            a = temp;
        }

        print "最大公约数:", a;
        return 0;
    }

## 无限循环

    int main() {
        int count = 0;

        while true {
            count++;
            print count;
            if count >= 3 { break; }
        }

        return 0;
    }

## 常见错误

### 忘记更新循环变量

    int i = 0;
    while i < 5 {
        print i;
        // 忘记 i++，导致无限循环
    }

### 分号误用

    int i = 0;
    while i < 5; {          // 分号导致循环体为空
        print i;
        i++;
    }

### break 在嵌套循环中

break 只跳出直接包含它的那一层循环。

## 继续阅读

掌握了 while 和 do-while 后，下一章将学习 for 循环。