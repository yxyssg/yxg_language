# 枚举

## 基本语法

枚举是一组具名整型常量的集合。使用 enum 关键字定义：

    enum Color {
        Red,
        Green,
        Blue
    }

    int main() {
        Color c = Color.Red;
        print(c);                    // 0（枚举值本质是 int）

        if (c == Color.Red) {
            print("红色");
        }

        return 0;
    }

枚举成员默认从 0 开始自动递增。上面 Red = 0，Green = 1，Blue = 2。

## 指定枚举值

可以手动指定枚举成员的值：

    enum Status {
        Pending = 10,
        Active = 20,
        Completed = 30,
        Failed = 40
    }

也可以部分指定，未指定的成员在前一个值的基础上递增：

    enum ErrorCode {
        Success = 0,
        NotFound,                    // 1
        PermissionDenied,            // 2
        InternalError = 100,
        ServiceUnavailable,          // 101
        Timeout                      // 102
    }

    int main() {
        print(ErrorCode.Success);             // 0
        print(ErrorCode.NotFound);            // 1
        print(ErrorCode.PermissionDenied);    // 2
        print(ErrorCode.InternalError);       // 100
        print(ErrorCode.ServiceUnavailable);  // 101
        print(ErrorCode.Timeout);             // 102
        return 0;
    }

## 枚举与 switch

枚举和 switch 配合使用非常自然：

    enum Day {
        Monday = 1,
        Tuesday,
        Wednesday,
        Thursday,
        Friday,
        Saturday,
        Sunday
    }

    int main() {
        Day today = Day.Wednesday;

        switch (today) {
            case Day.Monday:    print("星期一"); break;
            case Day.Tuesday:   print("星期二"); break;
            case Day.Wednesday: print("星期三"); break;
            case Day.Thursday:  print("星期四"); break;
            case Day.Friday:    print("星期五"); break;
            case Day.Saturday:  print("星期六"); break;
            case Day.Sunday:    print("星期日"); break;
            default:            print("无效日期");
        }

        return 0;
    }

输出：

    星期三

## 枚举本质

枚举成员本质是 int 类型的编译时常量，可以参与整数运算：

    int main() {
        int x = Color.Red + 10;      // 10
        int y = Color.Blue * 2;      // 4

        // 遍历枚举（利用递增特性）
        for (int i = Color.Red; i <= Color.Blue; i++) {
            print(i);                // 0, 1, 2
        }

        return 0;
    }

## 枚举作为函数参数

    enum LogLevel {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    }

    void log(LogLevel level, string message) {
        switch (level) {
            case LogLevel.Debug:   print("[DEBUG]", message); break;
            case LogLevel.Info:    print("[INFO]", message); break;
            case LogLevel.Warning: print("[WARN]", message); break;
            case LogLevel.Error:   print("[ERROR]", message); break;
        }
    }

    int main() {
        log(LogLevel.Info, "程序启动");
        log(LogLevel.Warning, "磁盘空间不足");
        log(LogLevel.Error, "连接失败");
        return 0;
    }

输出：

    [INFO] 程序启动
    [WARN] 磁盘空间不足
    [ERROR] 连接失败

## 完整示例：HTTP 状态码

    enum HttpStatus {
        OK = 200,
        Created = 201,
        NoContent = 204,
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        InternalServerError = 500,
        BadGateway = 502,
        ServiceUnavailable = 503
    }

    string getStatusMessage(HttpStatus status) {
        switch (status) {
            case HttpStatus.OK:                  return "OK";
            case HttpStatus.Created:             return "Created";
            case HttpStatus.NoContent:           return "No Content";
            case HttpStatus.BadRequest:          return "Bad Request";
            case HttpStatus.Unauthorized:        return "Unauthorized";
            case HttpStatus.Forbidden:           return "Forbidden";
            case HttpStatus.NotFound:            return "Not Found";
            case HttpStatus.InternalServerError: return "Internal Server Error";
            case HttpStatus.BadGateway:          return "Bad Gateway";
            case HttpStatus.ServiceUnavailable:  return "Service Unavailable";
            default:                             return "Unknown";
        }
    }

    int main() {
        HttpStatus s1 = HttpStatus.OK;
        HttpStatus s2 = HttpStatus.NotFound;
        HttpStatus s3 = HttpStatus.InternalServerError;

        print(s1, getStatusMessage(s1));   // 200 OK
        print(s2, getStatusMessage(s2));   // 404 Not Found
        print(s3, getStatusMessage(s3));   // 500 Internal Server Error

        return 0;
    }

## 常见错误

### 重复的枚举值

    enum Bad {
        A = 1,
        B = 1           // 错误！重复的值
    }

编译器会检测重复的显式枚举值并报错。

### 枚举名冲突

    enum Color { Red, Green, Blue }
    // enum Traffic { Red, Yellow, Green }   // 错误！Red 和 Green 重复

枚举成员名在同一作用域内必须唯一。如果确实需要同名，可以将枚举放在不同类中。

### 将枚举当作类使用

    Color c = Color.Red;
    // c.someMethod();     // 错误！枚举不是类，没有方法

枚举只是 int 的别名集合，不具备类的特性。

## 继续阅读

掌握了枚举后，下一章将学习模块系统与 import 语句。