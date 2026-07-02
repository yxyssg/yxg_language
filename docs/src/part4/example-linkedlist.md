# 实战教程：实现一个简单链表

## 目标

使用 YXG 的面向对象特性实现一个单向链表，支持添加、删除、查找和遍历操作。

## 完整代码

    import yxg.String;

    // 链表节点类
    class Node {
        pub int data;
        pub Node next;

        pub Node(int data) {
            this.data = data;
            this.next = null;
        }
    }

    // 链表类
    class LinkedList {
        pri Node head;
        pri int size;

        pub LinkedList() {
            this.head = null;
            this.size = 0;
        }

        // 在尾部添加节点
        pub void append(int data) {
            Node newNode = new Node(data);

            if head == null {
                head = newNode;
            } else {
                Node current = head;
                while current.next != null {
                    current = current.next;
                }
                current.next = newNode;
            }

            size = size + 1;
        }

        // 在头部添加节点
        pub void prepend(int data) {
            Node newNode = new Node(data);
            newNode.next = head;
            head = newNode;
            size = size + 1;
        }

        // 在指定位置插入
        pub bool insert(int index, int data) {
            if index < 0 || index > size {
                print "插入位置无效:", index;
                return false;
            }

            if index == 0 {
                prepend(data);
                return true;
            }

            Node newNode = new Node(data);
            Node current = head;

            for int i = 0; i < index - 1; i++ {
                current = current.next;
            }

            newNode.next = current.next;
            current.next = newNode;
            size = size + 1;
            return true;
        }

        // 删除指定位置的节点
        pub bool removeAt(int index) {
            if index < 0 || index >= size {
                print "删除位置无效:", index;
                return false;
            }

            if index == 0 {
                head = head.next;
                size = size - 1;
                return true;
            }

            Node current = head;
            for int i = 0; i < index - 1; i++ {
                current = current.next;
            }

            current.next = current.next.next;
            size = size - 1;
            return true;
        }

        // 删除第一个值为 data 的节点
        pub bool remove(int data) {
            if head == null {
                return false;
            }

            if head.data == data {
                head = head.next;
                size = size - 1;
                return true;
            }

            Node current = head;
            while current.next != null {
                if current.next.data == data {
                    current.next = current.next.next;
                    size = size - 1;
                    return true;
                }
                current = current.next;
            }

            return false;
        }

        // 查找节点位置
        pub int indexOf(int data) {
            Node current = head;
            int index = 0;

            while current != null {
                if current.data == data {
                    return index;
                }
                current = current.next;
                index++;
            }

            return -1;
        }

        // 获取指定位置的节点值
        pub int get(int index) {
            if index < 0 || index >= size {
                print "索引越界:", index;
                return -1;
            }

            Node current = head;
            for int i = 0; i < index; i++ {
                current = current.next;
            }

            return current.data;
        }

        // 获取链表长度
        pub int getSize() {
            return size;
        }

        // 判断链表是否为空
        pub bool isEmpty() {
            return size == 0;
        }

        // 反转链表
        pub void reverse() {
            Node prev = null;
            Node current = head;
            Node next = null;

            while current != null {
                next = current.next;
                current.next = prev;
                prev = current;
                current = next;
            }

            head = prev;
        }

        // 打印链表
        pub void printList() {
            if head == null {
                print "链表为空";
                return;
            }

            Node current = head;
            while current != null {
                print current.data;
                current = current.next;
                if current != null {
                    print " -> ";
                }
            }
            print "";
        }
    }

    int main() {
        LinkedList list = new LinkedList();

        print "=== 添加元素 ===";
        list.append(10);
        list.append(20);
        list.append(30);
        list.prepend(5);
        list.insert(2, 15);
        list.printList();
        print "链表长度:", list.getSize();

        print "";
        print "=== 查找元素 ===";
        print "索引 0 的值:", list.get(0);
        print "索引 3 的值:", list.get(3);
        print "值 20 的位置:", list.indexOf(20);
        print "值 100 的位置:", list.indexOf(100);

        print "";
        print "=== 删除元素 ===";
        list.removeAt(2);
        print "删除索引 2 后:";
        list.printList();

        list.remove(20);
        print "删除值 20 后:";
        list.printList();

        print "";
        print "=== 反转链表 ===";
        list.reverse();
        print "反转后:";
        list.printList();

        print "";
        print "=== 最终状态 ===";
        print "链表长度:", list.getSize();
        print "是否为空:", list.isEmpty();

        return 0;
    }

## 运行结果

    === 添加元素 ===
    5 -> 10 -> 15 -> 20 -> 30
    链表长度: 5

    === 查找元素 ===
    索引 0 的值: 5
    索引 3 的值: 20
    值 20 的位置: 3
    值 100 的位置: -1

    === 删除元素 ===
    删除索引 2 后:
    5 -> 10 -> 20 -> 30
    删除值 20 后:
    5 -> 10 -> 30

    === 反转链表 ===
    反转后:
    30 -> 10 -> 5

    === 最终状态 ===
    链表长度: 3
    是否为空: false

## 知识点总结

这个程序涵盖了以下 YXG 语言特性：

- 类的定义与实例化
- 构造函数（Node 和 LinkedList）
- 访问控制（pri 和 pub）
- 方法的定义与调用
- this 关键字的使用
- null 值的判断
- while 循环和 for 循环
- if 条件判断
- 布尔返回值和逻辑判断

## 继续阅读

下一章将实现一个命令行计算器。