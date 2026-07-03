# 内置函数参考

## 概述

YXG 编译器预注册了一系列运行时函数，无需 import 即可直接调用。这些函数提供了基本的 I/O、字符串操作、系统调用等功能。

## 输入输出

### print

向控制台输出内容，自动识别类型并格式化。print 不需要括号：

    int main() {
        print "Hello, World!";
        print 42;
        print 3.14159;
        print true;
        print 'A';

        // 多个参数，自动用空格分隔
        print "The answer is", 42;

        // 打印数组
        int[] nums = {1, 2, 3};
        print nums;

        return 0;
    }

输出：

    Hello, World!
    42
    3.141590
    true
    A
    The answer is 42
    [1, 2, 3]

### input

从控制台读取输入。使用 {} 作为占位符，变量需用 & 取地址：

    int main() {
        int age;
        string name;

        print "请输入姓名和年龄:";
        input("{}{}", &name, &age);

        print "姓名:", name;
        print "年龄:", age;

        return 0;
    }

input 的第一个参数是格式字符串，{} 按顺序对应后面的变量地址。支持的类型包括 int、float、char、string、bool。

## 字符串格式化

### format

格式化字符串并返回结果。使用 {} 作为占位符，按顺序填充参数：

    int main() {
        string result = format("姓名:{}, 年龄:{}", "Alice", 25);
        print result;

        return 0;
    }

输出：

    姓名:Alice, 年龄:25

任何类型均可自动填入 {}：

    print format("金币:{}", 100.5);
    print format("等级:{}", "A");
    print format("在线:{}", true);

格式控制（在 {} 内用 : 后接格式说明符）：

指定进制输出（参数需为整数）：

    print format("{:x}", 255);      // ff（十六进制小写）
    print format("{:X}", 255);      // FF（十六进制大写）
    print format("{:o}", 255);      // 377（八进制）
    print format("{:b}", 255);      // 11111111（二进制）
    print format("{:d}", 255);      // 255（十进制）

浮点数精度控制：

    print format("{:.2f}", 3.14159);   // 3.14
    print format("{:.0f}", 3.14159);   // 3
    print format("{:.6f}", 3.0);       // 3.000000

定宽零填充整数：

    print format("{:04d}", 7);         // 0007
    print format("{:08x}", 255);       // 000000ff
    print format("{:08X}", 255);       // 000000FF
    print format("{:08b}", 5);         // 00000101

转义花括号（连续两个花括号表示字面花括号）：

    print format("{{}}是占位符");       // {}是占位符
    print format("A{}B", 1);           // A1B

综合示例：

    print format("价格:{:.1f} 编号:{:07d}", 99.95, 42);
    // 输出: 价格:100.0 编号:0000042

## 字符串操作函数

这些函数可以直接调用，不过推荐使用 String 标准库的封装方法。

### _str_length

返回字符串长度：

    int main() {
        string s = "Hello";
        int len = _str_length(s);
        print len;                   // 5
        return 0;
    }

### _str_char_at

返回字符串指定位置的字符（ASCII 码）：

    int main() {
        string s = "Hello";
        int ch = _str_char_at(s, 0);
        print ch;                    // 72（'H'）
        return 0;
    }

### _str_substring

提取子串：

    int main() {
        string s = "Hello, World!";
        string sub = _str_substring(s, 0, 5);
        print sub;                   // Hello
        return 0;
    }

参数依次为：原字符串、起始索引、长度。

### _str_concat

拼接两个字符串：

    int main() {
        string result = _str_concat("Hello, ", "World!");
        print result;                // Hello, World!
        return 0;
    }

### _str_equal / _str_not_equal

比较两个字符串是否相等：

    int main() {
        bool eq = _str_equal("abc", "abc");
        bool ne = _str_not_equal("abc", "def");

        print eq;                    // true
        print ne;                    // true
        return 0;
    }

### _char_to_str

将字符转换为字符串：

    int main() {
        string s = _char_to_str('A');
        print s;                     // A
        return 0;
    }

### _str_plus_char

字符串拼接字符：

    int main() {
        string s = _str_plus_char("Hello", '!');
        print s;                     // Hello!
        return 0;
    }

### _char_plus_str

字符拼接字符串：

    int main() {
        string s = _char_plus_str('A', "BC");
        print s;                     // ABC
        return 0;
    }

## 数组操作

### _array_to_string

将数组转换为字符串表示：

    int main() {
        int[] nums = {1, 2, 3, 4, 5};
        string s = _array_to_string(nums);
        print s;                     // [1, 2, 3, 4, 5]
        return 0;
    }

## 系统函数

### _system_exit

退出程序并返回退出码：

    int main() {
        print "程序即将退出";
        _system_exit(0);
        print "这行不会执行";
        return 0;
    }

### _system_sleep

暂停程序执行指定毫秒数：

    int main() {
        print "等待中...";
        _system_sleep(1000);         // 暂停 1 秒
        print "完成";
        return 0;
    }

### _system_time

返回当前 Unix 时间戳（秒）：

    int main() {
        int now = _system_time();
        print "当前时间戳:", now;
        return 0;
    }

### _system_getenv

获取环境变量的值：

    int main() {
        string path = _system_getenv("PATH");
        print "PATH:", path;
        return 0;
    }

### _system_call

执行系统命令并返回退出码：

    int main() {
        int result = _system_call("dir");
        print "命令退出码:", result;
        return 0;
    }

### _GetCommandLineArgc

返回命令行参数个数（包括程序名）：

    int main() {
        int count = _GetCommandLineArgc();
        print "参数个数:", count;
        return 0;
    }

### _GetCommandLineArgv

返回指定索引的命令行参数：

    int main() {
        string arg0 = _GetCommandLineArgv(0);
        print "程序名:", arg0;
        return 0;
    }

### _sys_argv_at

返回指定索引的命令行参数（与 _GetCommandLineArgv 功能相同）：

    int main() {
        for int i = 0; i < _GetCommandLineArgc(); i++ {
            print "argv[", i, "] =", _sys_argv_at(i);
        }
        return 0;
    }

## 内存管理

### gc

手动触发一次垃圾回收：

    int main() {
        print "执行垃圾回收";
        gc();
        print "完成";
        return 0;
    }

### _gc_status

输出 GC 状态信息（调试用）：

    int main() {
        _gc_status();
        return 0;
    }

## 浮点运算辅助函数

这些函数用于浮点数的比较和取模，通常由编译器自动生成调用，用户一般不需要直接使用：

| 函数 | 说明 |
|------|------|
| _fgt(a, b) | 浮点大于比较 |
| _fge(a, b) | 浮点大于等于比较 |
| _feq(a, b) | 浮点等于比较 |
| _fne(a, b) | 浮点不等于比较 |
| _fle(a, b) | 浮点小于等于比较 |
| _flt(a, b) | 浮点小于比较 |
| _fmod_fpu(a, b) | 浮点取模 |

## 内置函数与标准库对比

很多内置函数在标准库中有更友好的封装：

| 内置函数 | 标准库方法 |
|----------|-----------|
| _str_length(s) | String str = new String(); str.len(s) |
| _str_char_at(s, i) | str.charAt(s, i) |
| _str_substring(s, start, len) | str.sub(s, start, len) |
| _str_concat(a, b) | a + b（使用运算符） |
| _system_exit(code) | System sys = new System(); sys.exit(code) |
| _system_time() | sys.time() |
| _system_sleep(ms) | sys.sleep(ms) |
| _GetCommandLineArgc() | sys.argc() |
| _sys_argv_at(i) | sys.argv(i) |

建议优先使用标准库，代码更清晰易读。

## 继续阅读

至此，YXG 语言参考部分全部结束。下一部分将深入编译器内部，从整体架构开始讲解。