# 字符串

## 字符串的创建

字符串用双引号包裹：

    import yxg.String;

    int main() {
        string s1 = "Hello, World!";
        string s2 = "";
        string s3 = "包含中文和日本語";

        print s1;
        print s2;
        print s3;
        return 0;
    }

转义序列：

    string s = "第一行\n第二行";
    string t = "列1\t列2\t列3";
    string q = "他说：\"你好\"";
    string b = "路径：C:\\Users\\Admin";

## 字符串拼接

使用 + 运算符拼接字符串：

    string hello = "Hello";
    string world = "World";
    string s1 = hello + " " + world;
    string s2 = hello + "!";

    print s1;
    print s2;

与字符拼接：

    string s = "ABC";
    char ch = 'D';
    string result = s + ch;

与数字拼接：

    string s1 = "Value: " + 42;
    string s2 = "Pi ≈ " + 3.14;
    string s3 = "Flag: " + true;

## 使用 String 库

String 库中的方法需要通过实例调用。首先创建 String 对象：

    import yxg.String;

    int main() {
        String str = new String();

        // 之后通过 str 调用方法
        int len = str.len("Hello");
        print len;

        return 0;
    }

## 字符串长度

    import yxg.String;

    int main() {
        String str = new String();

        int len = str.len("Hello, World!");
        print len;                          // 13

        bool empty = str.isEmpty("");
        print empty;                        // true

        return 0;
    }

## 访问单个字符

    import yxg.String;

    int main() {
        String str = new String();
        string s = "Hello";

        int c0 = str.charAt(s, 0);         // 72（'H' 的 ASCII 码）
        int c1 = str.charAt(s, 1);         // 101（'e'）
        int c4 = str.charAt(s, 4);         // 111（'o'）

        print c0, c1, c4;

        // 遍历
        for int i = 0; i < str.len(s); i++ {
            print "s[", i, "] =", str.charAt(s, i);
        }

        return 0;
    }

首尾字符快捷方法：

    int first = str.first(s);              // 72
    int last = str.last(s);                // 111

## 子串操作

    import yxg.String;

    int main() {
        String str = new String();
        string s = "Hello, World!";

        string sub1 = str.sub(s, 0, 5);    // "Hello"
        string sub2 = str.sub(s, 7, 5);    // "World"
        string left = str.left(s, 5);      // "Hello"
        string right = str.right(s, 6);    // "World!"

        print sub1;
        print sub2;
        print left;
        print right;

        return 0;
    }

## 字符串比较

    string s1 = "apple";
    string s2 = "apple";
    string s3 = "banana";

    if s1 == s2 { print "相同"; }
    if s1 != s3 { print "不同"; }
    if s1 < s3  { print "apple 在 banana 之前"; }

== 比较的是字符串内容，不是内存地址。比较区分大小写。

## 前后缀判断

    import yxg.String;

    int main() {
        String str = new String();
        string s = "Hello, World!";

        bool b1 = str.startsWith(s, "Hello");    // true
        bool b2 = str.startsWith(s, "World");    // false
        bool b3 = str.endsWith(s, "World!");     // true
        bool b4 = str.endsWith(s, "Hello");      // false

        print b1, b2, b3, b4;

        return 0;
    }

## 字符与子串查找

    import yxg.String;

    int main() {
        String str = new String();
        string s = "Hello, World!";

        int pos1 = str.indexOf(s, 'o');          // 4
        int pos2 = str.lastIndexOf(s, 'o');      // 8
        bool has = str.contains(s, 'W');         // true

        int pos3 = str.indexOfStr(s, "World");   // 7
        bool has2 = str.containsStr(s, "llo");   // true

        print pos1, pos2, has, pos3, has2;

        return 0;
    }

## 大小写转换

    import yxg.String;

    int main() {
        String str = new String();

        int upper = str.toUpper('a');            // 65（'A'）
        int lower = str.toLower('Z');            // 122（'z'）

        string s = "Hello, World!";
        string up = str.toUpperStr(s);           // "HELLO, WORLD!"
        string lo = str.toLowerStr(s);           // "hello, world!"

        print upper, lower;
        print up;
        print lo;

        return 0;
    }

## 空白处理

    import yxg.String;

    int main() {
        String str = new String();
        string s = "   Hello, World!   ";

        string t1 = str.trimL(s);
        string t2 = str.trimR(s);
        string t3 = str.trim(s);

        print "'", t1, "'";
        print "'", t2, "'";
        print "'", t3, "'";

        bool space = str.isSpace(' ');
        bool tab   = str.isSpace('\t');

        print space, tab;

        return 0;
    }

输出：

    'Hello, World!   '
    '   Hello, World!'
    'Hello, World!'
    true true

## 重复与填充

    import yxg.String;

    int main() {
        String str = new String();

        string r1 = str.repeat("Hi", 3);         // "HiHiHi"
        string r2 = str.repeat("AB", 2);         // "ABAB"

        string p1 = str.padL("42", 5, '0');      // "00042"
        string p2 = str.padR("YXG", 6, '.');     // "YXG..."

        print r1;
        print r2;
        print p1;
        print p2;

        return 0;
    }

## 替换

    import yxg.String;

    int main() {
        String str = new String();

        string r = str.replace("Hello, World!", "World", "YXG");
        string r2 = str.replace("one two one two", "one", "1");

        print r;                                 // Hello, YXG!
        print r2;                                // 1 two 1 two

        return 0;
    }

## 统计

    import yxg.String;

    int main() {
        String str = new String();
        string s = "Hello, World!";

        int n1 = str.count(s, 'l');              // 3
        int n2 = str.countStr(s, "lo");          // 1

        print n1;
        print n2;

        return 0;
    }

## 字符串不可变性

YXG 中字符串是不可变的。每次操作都创建新字符串：

    string s = "Hello";
    string t = s;
    s = s + " World";
    print "s:", s;
    print "t:", t;

输出：

    s: Hello World
    t: Hello

旧字符串由 GC 自动回收。

## 字符串与数组

字符串可以像字符数组一样遍历：

    import yxg.String;

    int main() {
        String str = new String();
        string s = "YXG";

        for int i = 0; i < str.len(s); i++ {
            print str.charAt(s, i);
        }

        for char ch : s {
            print ch;
        }

        return 0;
    }

但字符串不是数组，不能赋值给 string[] 类型。

## 常见模式

### 统计字符出现次数

    import yxg.String;

    int countChar(string s, char target) {
        String str = new String();
        int count = 0;
        for int i = 0; i < str.len(s); i++ {
            if str.charAt(s, i) == target {
                count++;
            }
        }
        return count;
    }

### 反转字符串

    import yxg.String;

    string reverse(string s) {
        String str = new String();
        string result = "";
        for int i = str.len(s) - 1; i >= 0; i-- {
            int ch = str.charAt(s, i);
            result = result + ch;
        }
        return result;
    }

### 判断是否以某子串开头

    import yxg.String;

    bool startsWith(string s, string prefix) {
        String str = new String();
        if str.len(prefix) > str.len(s) { return false; }

        for int i = 0; i < str.len(prefix); i++ {
            if str.charAt(s, i) != str.charAt(prefix, i) {
                return false;
            }
        }
        return true;
    }

## 常见错误

### 混淆字符和字符串

    string s = "A";
    char ch = 'A';
    // string wrong = 'A';     // 错误
    // char wrong2 = "A";      // 错误

### 空字符串与 null

    import yxg.String;

    int main() {
        String str = new String();

        string s;
        // int len = str.len(s);    // 运行时错误：s 是 null

        string t = "";
        int len = str.len(t);       // 正确：0

        return 0;
    }

未初始化的字符串默认是 null，访问其属性或方法会出错。建议始终初始化字符串。

## 继续阅读

掌握了字符串后，下一章将学习类的定义与面向对象编程。