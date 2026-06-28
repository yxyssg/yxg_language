# 控制流：foreach 循环

## 基本语法

foreach 循环用于遍历数组的每个元素，无需手动管理索引：

    int main() {
        int[] numbers = {10, 20, 30, 40, 50};

        for int v : numbers {
            print v;
        }

        return 0;
    }

输出：

    10
    20
    30
    40
    50

语法格式：

    for 元素类型 变量名 : 数组 {
        循环体
    }

## 类型声明

### 显式类型

    int main() {
        float[] scores = {95.5, 88.0, 76.5, 92.0};

        for float s : scores {
            print s;
        }

        return 0;
    }

### 省略类型（自动推断）

    int main() {
        string[] names = {"Alice", "Bob", "Charlie"};

        for name : names {
            print name;
        }

        return 0;
    }

当省略类型时，编译器从数组的元素类型自动推断。两种写法完全等价。

## 遍历多维数组

    int main() {
        int[][] matrix = {
            {1, 2, 3},
            {4, 5, 6},
            {7, 8, 9}
        };

        for int[] row : matrix {
            for int elem : row {
                print elem;
            }
        }

        return 0;
    }

输出：

    1
    2
    3
    4
    5
    6
    7
    8
    9

## 遍历字符串

    int main() {
        string text = "Hello";

        for char ch : text {
            print ch;
        }

        return 0;
    }

输出：

    H
    e
    l
    l
    o

## foreach 与 for 对比

    int main() {
        int[] arr = {10, 20, 30, 40, 50};

        // 传统 for：需要索引时使用
        for int i = 0; i < arr.length; i++ {
            print "arr[", i, "] =", arr[i];
        }

        // foreach：只读遍历时更简洁
        for int v : arr {
            print v;
        }

        return 0;
    }

选择建议：
- 需要索引时用 for（如修改数组元素、需要知道位置）
- 只读遍历时用 foreach（更简洁，不会越界）
- 倒序遍历用 for
- 跳跃遍历用 for

## 循环中的 break 和 continue

    int main() {
        int[] arr = {1, 3, 5, 8, 10, 12};

        // 找到第一个偶数
        for int v : arr {
            if v % 2 == 0 {
                print "第一个偶数:", v;
                break;
            }
        }

        // 只打印奇数
        for int v : arr {
            if v % 2 == 0 {
                continue;
            }
            print "奇数:", v;
        }

        return 0;
    }

输出：

    第一个偶数: 8
    奇数: 1
    奇数: 3
    奇数: 5

## 常见模式

### 求和

    int main() {
        int[] arr = {1, 2, 3, 4, 5};
        int sum = 0;

        for int v : arr {
            sum += v;
        }

        print "总和:", sum;
        return 0;
    }

### 查找最大值

    int main() {
        int[] arr = {42, 17, 89, 5, 73};
        int max = arr[0];

        for int v : arr {
            if v > max {
                max = v;
            }
        }

        print "最大值:", max;
        return 0;
    }

### 统计

    int main() {
        int[] scores = {85, 92, 78, 95, 88, 72, 90};
        int count = 0;

        for int s : scores {
            if s >= 90 {
                count++;
            }
        }

        print "90 分以上的人数:", count;
        return 0;
    }

## 注意事项

### 不能修改数组元素

foreach 中的循环变量是数组元素的副本，修改它不会影响原数组：

    int main() {
        int[] arr = {1, 2, 3, 4, 5};

        for int v : arr {
            v = v * 2;               // 只修改了副本，不影响 arr
        }

        for int v : arr {
            print v;                 // 仍然是 1, 2, 3, 4, 5
        }

        return 0;
    }

需要修改数组元素时使用传统 for 循环：

    for int i = 0; i < arr.length; i++ {
        arr[i] = arr[i] * 2;
    }

### 不能获取索引

foreach 不提供索引信息。如果需要索引，使用传统 for。

### 遍历时不能修改数组长度

数组长度在循环开始时确定，遍历期间不能增减元素。

## 常见错误

### 类型不匹配

    float[] arr = {1.5, 2.5, 3.5};
    // for int v : arr { ... }       // 错误！类型不匹配
    for float v : arr { ... }        // 正确

### 遍历非数组类型

    int x = 42;
    // for v : x { ... }             // 错误！x 不是数组

foreach 只能用于数组和字符串。

## 继续阅读

掌握了所有循环结构后，下一章将学习函数的定义与使用。