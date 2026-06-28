# 控制流

## 概述

控制流语句决定程序的执行路径。YXG 提供了以下控制流结构：

| 语句 | 用途 |
|------|------|
| if / else if / else | 条件分支 |
| switch / case | 多分支选择 |
| while | 前置条件循环 |
| do-while | 后置条件循环 |
| for | 计数循环 |
| foreach | 数组遍历循环 |
| break | 跳出循环或 switch |
| continue | 跳过本次迭代 |
| return | 从函数返回 |

## 条件语句的选择

根据不同场景选择合适的条件语句：

    int score = 85;

    // 简单的是/否判断 → if/else
    if score >= 60 {
        print "及格";
    } else {
        print "不及格";
    }

    // 多个范围判断 → if/else if/else
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

    // 单个值的多个匹配 → switch/case
    int day = 3;
    switch day {
        case 1: print "Monday"; break;
        case 2: print "Tuesday"; break;
        case 3: print "Wednesday"; break;
        default: print "Unknown";
    }

## 循环语句的选择

    int[] arr = {1, 2, 3, 4, 5};

    // 已知循环次数 → for
    for int i = 0; i < 5; i++ {
        print i;
    }

    // 遍历数组所有元素 → foreach
    for int v : arr {
        print v;
    }

    // 条件未知，可能零次 → while
    int i = 0;
    while i < arr.length && arr[i] < 10 {
        print arr[i];
        i++;
    }

    // 至少执行一次 → do-while
    int input;
    do {
        input = 5;
    } while input < 0 || input > 100;

## break 与 continue

break 立即终止当前循环或 switch：

    for int i = 0; i < 10; i++ {
        if i == 5 {
            break;
        }
        print i;
    }

continue 跳过本次迭代的剩余部分：

    for int i = 0; i < 10; i++ {
        if i % 2 == 0 {
            continue;
        }
        print i;
    }

在嵌套结构中，break 和 continue 只影响最内层的循环或 switch。

## 条件表达式

所有条件语句的条件必须是 bool 类型：

    int x = 10;
    // if x { ... }            // 错误！int 不能作为条件
    if x != 0 { ... }          // 正确

    string s = "hello";
    if s != "" { ... }         // 正确

    bool flag = true;
    if flag { ... }            // 正确

    if x > 5 && y < 20 { ... } // 正确

## 代码块与作用域

控制流语句创建新的作用域：

    int x = 10;

    if x > 5 {
        int y = 20;
        print x, y;
    }
    // print y;                 // 错误！y 已超出作用域

    for int i = 0; i < 5; i++ {
        print i;
    }
    // print i;                 // 错误！i 已超出作用域

## 继续阅读

接下来各章节将详细介绍每种控制流语句的用法。