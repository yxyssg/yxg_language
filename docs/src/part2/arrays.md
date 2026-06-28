# 数组

## 数组声明与初始化

数组是一组相同类型元素的集合。声明数组的语法：

    类型[] 数组名;

### 静态初始化

在声明时用花括号提供初始值：

    int main() {
        int[] numbers = {10, 20, 30, 40, 50};
        float[] scores = {95.5, 88.0, 76.5};
        string[] names = {"Alice", "Bob", "Charlie"};
        bool[] flags = {true, false, true};

        return 0;
    }

### 动态分配

使用 new 关键字创建指定大小的数组：

    int main() {
        int[] arr = new int[10];
        float[] floats = new float[5];
        string[] strings = new string[3];

        int size = 25;
        int[] dynamic = new int[size];

        return 0;
    }

### 声明与赋值分离

    int main() {
        int[] arr;
        arr = new int[5];
        arr = {1, 2, 3, 4, 5};

        return 0;
    }

## 访问元素

通过方括号和索引访问数组元素，索引从 0 开始：

    int main() {
        int[] arr = {10, 20, 30, 40, 50};

        int first = arr[0];
        int third = arr[2];
        int last = arr[4];

        arr[0] = 100;
        arr[1] = arr[1] + 5;

        print first, third, last;
        print arr[0], arr[1];

        return 0;
    }

索引必须是整数类型。使用负数或超出范围的索引会导致运行时错误：

    int[] arr = {1, 2, 3};
    // int x = arr[-1];      // 运行时错误：数组越界
    // int y = arr[100];     // 运行时错误：数组越界

## 获取数组长度

使用 .length 属性获取数组的元素个数：

    int main() {
        int[] arr = {10, 20, 30, 40, 50};

        int len = arr.length;
        print "数组长度:", len;

        for int i = 0; i < arr.length; i++ {
            print "arr[", i, "] =", arr[i];
        }

        return 0;
    }

.length 返回的是 int 类型，可以参与运算：

    int[] a = {1, 2, 3};
    int[] b = {4, 5};
    int total = a.length + b.length;

## 多维数组

### 声明与初始化

    int main() {
        int[][] matrix = {
            {1, 2, 3},
            {4, 5, 6},
            {7, 8, 9}
        };

        int[][][] cube = {
            {{1, 2}, {3, 4}},
            {{5, 6}, {7, 8}}
        };

        return 0;
    }

### 动态分配多维数组

    int main() {
        int[][] matrix = new int[3][4];

        int[][] jagged = new int[3][];
        jagged[0] = new int[2];
        jagged[1] = new int[5];
        jagged[2] = new int[3];

        return 0;
    }

### 访问多维数组

    int main() {
        int[][] matrix = {
            {1, 2, 3},
            {4, 5, 6},
            {7, 8, 9}
        };

        int elem = matrix[1][2];
        matrix[0][0] = 99;

        int rows = matrix.length;
        int cols = matrix[0].length;

        print "元素:", elem;
        print "行数:", rows, "列数:", cols;

        for int i = 0; i < matrix.length; i++ {
            for int j = 0; j < matrix[i].length; j++ {
                print "matrix[", i, "][", j, "] =", matrix[i][j];
            }
        }

        return 0;
    }

## 数组的存储

YXG 的数组在堆上分配，由垃圾回收器管理。数组变量存储的是引用：

    int main() {
        int[] a = {1, 2, 3};
        int[] b = a;

        b[0] = 99;
        print a[0];                // 99（a 也被修改了）

        return 0;
    }

要复制数组，需要逐个元素拷贝：

    int main() {
        int[] original = {1, 2, 3, 4, 5};
        int[] copy = new int[original.length];

        for int i = 0; i < original.length; i++ {
            copy[i] = original[i];
        }

        copy[0] = 99;
        print "original[0]:", original[0];
        print "copy[0]:", copy[0];

        return 0;
    }

## 数组作为函数参数

    void printArray(int[] arr) {
        print "数组长度:", arr.length;
        for int i = 0; i < arr.length; i++ {
            print "  [", i, "] =", arr[i];
        }
    }

    int[] doubleElements(int[] arr) {
        int[] result = new int[arr.length];
        for int i = 0; i < arr.length; i++ {
            result[i] = arr[i] * 2;
        }
        return result;
    }

    int main() {
        int[] nums = {1, 2, 3, 4, 5};

        printArray(nums);

        int[] doubled = doubleElements(nums);
        printArray(doubled);

        return 0;
    }

数组参数传递的是引用，函数内修改数组元素会影响原数组。但重新分配（new）不会影响原引用。

## 数组与 foreach

    int main() {
        int[] arr = {10, 20, 30, 40, 50};

        for int v : arr {
            print v;
        }

        int[][] matrix = {{1, 2}, {3, 4}, {5, 6}};
        for int[] row : matrix {
            for int elem : row {
                print elem;
            }
        }

        return 0;
    }

## 数组转字符串

print 可以直接打印数组：

    int main() {
        int[] nums = {1, 2, 3, 4, 5};
        print nums;

        string[] names = {"Alice", "Bob"};
        print names;

        int[][] matrix = {{1, 2}, {3, 4}};
        print matrix;

        return 0;
    }

输出：

    [1, 2, 3, 4, 5]
    [Alice, Bob]
    [[1, 2], [3, 4]]

## 常见模式

### 查找元素

    int findIndex(int[] arr, int target) {
        for int i = 0; i < arr.length; i++ {
            if arr[i] == target {
                return i;
            }
        }
        return -1;
    }

### 反转数组

    void reverse(int[] arr) {
        int left = 0;
        int right = arr.length - 1;

        while left < right {
            int temp = arr[left];
            arr[left] = arr[right];
            arr[right] = temp;
            left++;
            right--;
        }
    }

### 数组合并

    int[] concat(int[] a, int[] b) {
        int[] result = new int[a.length + b.length];

        for int i = 0; i < a.length; i++ {
            result[i] = a[i];
        }
        for int i = 0; i < b.length; i++ {
            result[a.length + i] = b[i];
        }

        return result;
    }

## 常见错误

### 数组越界

    int[] arr = {1, 2, 3};
    // arr[5] = 10;          // 运行时错误

YXG 编译器在生成代码时会插入越界检查。越界访问会触发运行时错误并终止程序。

### 未分配就使用

    int[] arr;
    // arr[0] = 10;          // 运行时错误

### 类型不匹配

    // int[] arr = {1, 2, "hello"};    // 错误！元素类型不一致

### .length 不可赋值

    int[] arr = {1, 2, 3};
    // arr.length = 10;      // 错误！length 是只读的

## 继续阅读

掌握了数组后，下一章将学习字符串的详细用法。