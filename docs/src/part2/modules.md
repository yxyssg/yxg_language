# 模块系统（import）

## 基本语法

YXG 通过 import 语句引入其他源文件，实现代码复用和模块化：

    import yxg.Math;
    import yxg.String;

    int main() {
        Math m = new Math();
        float x = m.sqrt(16.0);
        print x;
        return 0;
    }

import 语句必须放在文件顶部，在所有其他声明之前。

## 导入方式

### 点号路径导入

使用点号分隔的模块路径，编译器会自动转换为目录分隔符并加上 .yxg 后缀：

    import yxg.String;
    import yxg.Math;
    import yxg.System;

    int main() {
        String str = new String();
        Math m = new Math();
        System sys = new System();

        print str.len("hello");
        print m.sqrt(25.0);
        print sys.time();

        return 0;
    }

import yxg.String 等价于查找 yxg/String.yxg。

### 字符串路径导入

也可以使用字符串指定文件路径：

    import "mylib/custom.yxg";

    int main() {
        // 使用 custom.yxg 中定义的类
        return 0;
    }

### 使用别名

当模块名较长或可能冲突时，使用 as 关键字设置别名：

    import utils.string_utils as StrUtil;

    int main() {
        StrUtil.StringHelper helper = new StrUtil.StringHelper();
        print helper.toUpper("hello");
        return 0;
    }

## 模块搜索路径

编译器按以下顺序搜索模块：

1. 当前工作目录
2. 编译器所在目录下的 lib/ 子目录
3. YXG_PATH 环境变量指定的目录（多个路径用分号分隔）

示例：

    // 假设目录结构：
    // ./
    //   main.yxg
    //   lib/
    //     yxg/
    //       String.yxg
    //       Math.yxg
    //       System.yxg
    //   mylib/
    //     custom.yxg

    import yxg.String;              // 从 lib/yxg/String.yxg 找到
    import "mylib/custom.yxg";      // 从 ./mylib/custom.yxg 找到

## 设置模块搜索路径

Windows 命令提示符：

    set YXG_PATH=C:\my_yxg_libs;D:\shared_yxg

PowerShell：

    $env:YXG_PATH = "C:\my_yxg_libs;D:\shared_yxg"

## 重复导入

同一个模块只会被导入一次。编译器内部维护了已导入模块的列表，后续对同一模块的 import 会被忽略：

    import yxg.String;
    import yxg.String;              // 被忽略，不会重复导入

    int main() {
        String str = new String();
        print str.len("hello");
        return 0;
    }

## 循环导入

YXG 不支持循环导入。如果 A.yxg import B.yxg，而 B.yxg 又 import A.yxg，编译器会检测到并报错。

## 完整示例

假设有以下项目结构：

    project/
    ├── main.yxg
    └── lib/
        └── yxg/
            ├── String.yxg
            ├── Math.yxg
            └── System.yxg

main.yxg：

    import yxg.String;
    import yxg.Math;
    import yxg.System;

    int main() {
        String str = new String();
        Math m = new Math();
        System sys = new System();

        string greeting = "Hello, YXG!";
        print "字符串长度:", str.len(greeting);
        print "转大写:", str.toUpperStr(greeting);

        print "sqrt(144) =", m.sqrt(144.0);
        print "随机数:", m.random();

        print "当前时间戳:", sys.time();

        return 0;
    }

编译：

    yxg main.yxg -r

## 常见错误

### 模块未找到

    import nonexistent.lib;

编译器报错：

    error[E2006]: module 'nonexistent.lib' not found

检查文件路径是否正确，以及是否在搜索路径中。

### 忘记 import

    int main() {
        String str = new String();    // 错误！String 未定义
        return 0;
    }

需要先 import：

    import yxg.String;

    int main() {
        String str = new String();
        return 0;
    }

### 循环导入

    // a.yxg
    import b;

    // b.yxg
    import a;                         // 错误！循环导入

## 继续阅读

掌握了模块系统后，下一章将介绍 YXG 的内置函数参考。