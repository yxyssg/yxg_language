# 控制流：for 循环

## 基本语法

for 循环将初始化、条件判断和更新操作集中在一行。各部分用分号分隔，条件不需要括号：

    int main() {
        for int i = 0; i < 5; i++ {
            print "i =", i;
        }

        print "循环结束";
        return 0;
    }

输出：

    i = 0
    i = 1
    i = 2
    i = 3
    i = 4
    循环结束

执行流程：
1. 执行初始化（只执行一次）
2. 检查条件，如果为 false 则退出
3. 执行循环体
4. 执行更新
5. 回到步骤 2

## 各部分详解

### 初始化

初始化部分在循环开始前执行一次。通常用于声明和初始化循环变量：

    for int i = 0; ...       // i 的作用域仅限于 for 循环内
    for int i = 10; ...      // 可以从任意值开始
    for int i = n - 1; ...   // 可以使用表达式

初始化部分声明的变量作用域仅限于 for 循环内：

    int main() {
        for int i = 0; i < 3; i++ {
            print i;
        }
        // print i;            // 错误！i 已超出作用域
        return 0;
    }

也可以使用已存在的变量：

    int main() {
        int i;
        for i = 0; i < 5; i++ {
            print i;
        }
        print "最终 i =", i;
        return 0;
    }

初始化部分可以留空：

    int main() {
        int i = 0;
        for ; i < 5; i++ {
            print i;
        }
        return 0;
    }

### 条件

条件在每次迭代开始前检查。如果为 false，循环终止：

    for int i = 0; i < 10; i++
    for int i = 10; i > 0; i--
    for int i = 0; i < n; i += 2

条件可以留空，表示始终为 true（无限循环）：

    for ;; {
        print "loop";
        break;
    }

### 更新

更新在每次循环体执行完后执行：

    for int i = 0; i < 10; i++
    for int i = 0; i < 10; i += 2
    for int i = 10; i > 0; i--

更新部分可以留空，在循环体中手动更新：

    for int i = 0; i < 10; {
        print i;
        i += 2;
    }

## 循环中的 break 和 continue

break 立即终止整个循环：

    int main() {
        for int i = 0; i < 10; i++ {
            if i == 5 {
                break;
            }
            print i;
        }
        return 0;
    }

输出：

    0
    1
    2
    3
    4

continue 跳过本次迭代的剩余部分：

    int main() {
        for int i = 0; i < 10; i++ {
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

## 嵌套 for 循环

    int main() {
        for int i = 1; i <= 9; i++ {
            for int j = 1; j <= i; j++ {
                print j, "*", i, "=", i * j, "  ";
            }
            print "";
        }
        return 0;
    }

输出：

    1 * 1 = 1
    1 * 2 = 2   2 * 2 = 4
    1 * 3 = 3   2 * 3 = 6   3 * 3 = 9
    ...

内层 break 只跳出内层循环。

## 常见模式

### 遍历数组

    int main() {
        int[] arr = {10, 20, 30, 40, 50};

        for int i = 0; i < arr.length; i++ {
            print "arr[", i, "] =", arr[i];
        }

        return 0;
    }

### 倒序遍历

    int main() {
        int[] arr = {10, 20, 30, 40, 50};

        for int i = arr.length - 1; i >= 0; i-- {
            print "arr[", i, "] =", arr[i];
        }

        return 0;
    }

### 步长大于 1

    int main() {
        for int i = 0; i <= 20; i += 2 {
            print i;
        }
        return 0;
    }

### 多个变量

    int main() {
        for int i = 0, j = 10; i < j; i++, j-- {
            print "i =", i, ", j =", j;
        }
        return 0;
    }

输出：

    i = 0, j = 10
    i = 1, j = 9
    i = 2, j = 8
    i = 3, j = 7
    i = 4, j = 6

## for 与 while 对比

for 循环：

    for int i = 0; i < 10; i++ {
        print i;
    }

等价的 while 循环：

    {
        int i = 0;
        while i < 10 {
            print i;
            i++;
        }
    }

选择建议：
- 循环次数已知或使用索引变量时，用 for
- 循环次数未知、条件复杂时，用 while
- 循环体至少执行一次时，用 do-while

## 常见错误

### 循环变量作用域

    for int i = 0; i < 5; i++ {
        print i;
    }
    // print i;                   // 错误！i 已超出作用域

如果需要在循环外使用，在循环前声明：

    int i;
    for i = 0; i < 5; i++ {
        print i;
    }
    print "最终 i =", i;

### 分号误用

    for int i = 0; i < 5; i++; {   // 注意分号！
        print i;                    // 错误！i 不在作用域内
    }

### 修改循环变量

    for int i = 0; i < 10; i++ {
        i += 2;                      // 在循环体内修改 i，可能导致意外
    }

## 继续阅读

掌握了 for 循环后，下一章将学习 foreach 循环。