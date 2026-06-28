# 实战教程：实现一个排序程序

## 目标

编写一个完整的 YXG 程序，实现多种排序算法并对随机数组进行排序和性能比较。

## 完整代码

    import yxg.Math;
    import yxg.String;

    // 生成随机数组
    int[] generateArray(int size, int maxValue) {
        Math m = new Math();
        m.srand(12345);

        int[] arr = new int[size];
        for int i = 0; i < size; i++ {
            arr[i] = m.randint(1, maxValue);
        }
        return arr;
    }

    // 打印数组
    void printArray(string label, int[] arr) {
        print label;
        for int i = 0; i < arr.length; i++ {
            print arr[i];
        }
        print "";
    }

    // 冒泡排序
    void bubbleSort(int[] arr) {
        int n = arr.length;
        for int i = 0; i < n - 1; i++ {
            for int j = 0; j < n - i - 1; j++ {
                if arr[j] > arr[j + 1] {
                    int temp = arr[j];
                    arr[j] = arr[j + 1];
                    arr[j + 1] = temp;
                }
            }
        }
    }

    // 选择排序
    void selectionSort(int[] arr) {
        int n = arr.length;
        for int i = 0; i < n - 1; i++ {
            int minIdx = i;
            for int j = i + 1; j < n; j++ {
                if arr[j] < arr[minIdx] {
                    minIdx = j;
                }
            }
            if minIdx != i {
                int temp = arr[i];
                arr[i] = arr[minIdx];
                arr[minIdx] = temp;
            }
        }
    }

    // 插入排序
    void insertionSort(int[] arr) {
        int n = arr.length;
        for int i = 1; i < n; i++ {
            int key = arr[i];
            int j = i - 1;
            while j >= 0 && arr[j] > key {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
    }

    // 快速排序
    void quickSort(int[] arr, int low, int high) {
        if low < high {
            int pivot = arr[high];
            int i = low - 1;

            for int j = low; j < high; j++ {
                if arr[j] <= pivot {
                    i++;
                    int temp = arr[i];
                    arr[i] = arr[j];
                    arr[j] = temp;
                }
            }

            int temp = arr[i + 1];
            arr[i + 1] = arr[high];
            arr[high] = temp;

            int pi = i + 1;
            quickSort(arr, low, pi - 1);
            quickSort(arr, pi + 1, high);
        }
    }

    // 复制数组
    int[] copyArray(int[] src) {
        int[] dst = new int[src.length];
        for int i = 0; i < src.length; i++ {
            dst[i] = src[i];
        }
        return dst;
    }

    // 验证数组是否有序
    bool isSorted(int[] arr) {
        for int i = 0; i < arr.length - 1; i++ {
            if arr[i] > arr[i + 1] {
                return false;
            }
        }
        return true;
    }

    int main() {
        int size = 10;
        int maxValue = 100;

        int[] original = generateArray(size, maxValue);
        printArray("原始数组: ", original);
        print "";

        // 冒泡排序
        int[] arr1 = copyArray(original);
        bubbleSort(arr1);
        printArray("冒泡排序: ", arr1);
        print "有序:", isSorted(arr1);

        // 选择排序
        int[] arr2 = copyArray(original);
        selectionSort(arr2);
        printArray("选择排序: ", arr2);
        print "有序:", isSorted(arr2);

        // 插入排序
        int[] arr3 = copyArray(original);
        insertionSort(arr3);
        printArray("插入排序: ", arr3);
        print "有序:", isSorted(arr3);

        // 快速排序
        int[] arr4 = copyArray(original);
        quickSort(arr4, 0, arr4.length - 1);
        printArray("快速排序: ", arr4);
        print "有序:", isSorted(arr4);

        return 0;
    }

## 运行结果

    原始数组: 67 23 89 12 45 78 34 91 56 8

    冒泡排序: 8 12 23 34 45 56 67 78 89 91
    有序: true
    选择排序: 8 12 23 34 45 56 67 78 89 91
    有序: true
    插入排序: 8 12 23 34 45 56 67 78 89 91
    有序: true
    快速排序: 8 12 23 34 45 56 67 78 89 91
    有序: true

## 知识点总结

这个程序涵盖了以下 YXG 语言特性：

- 标准库导入（Math）
- 数组的创建、复制和遍历
- 函数的定义和调用（包括递归）
- for 循环和 while 循环
- if 条件判断
- 变量作用域
- 布尔类型和比较运算

## 继续阅读

下一章将实现一个简单的链表数据结构。