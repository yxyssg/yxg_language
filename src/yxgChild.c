// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <ctype.h>
// #include <stdarg.h>
// #include <assert.h>
//
// /* ---------- 动态字符串 ---------- */
// typedef struct {
//     char *data;
//     size_t len, cap;
// } StringBuilder;
//
// void sb_init(StringBuilder *sb) {
//     sb->data = malloc(64);
//     sb->data[0] = '\0';
//     sb->len = 0;
//     sb->cap = 64;
// }
//
// void sb_append(StringBuilder *sb, const char *fmt, ...) {
//     va_list ap;
//     va_start(ap, fmt);
//     int need = vsnprintf(NULL, 0, fmt, ap);
//     va_end(ap);
//     if (need < 0) return;
//     while (sb->len + need + 1 > sb->cap) {
//         sb->cap *= 2;
//         sb->data = realloc(sb->data, sb->cap);
//     }
//     va_start(ap, fmt);
//     vsnprintf(sb->data + sb->len, need + 1, fmt, ap);
//     va_end(ap);
//     sb->len += need;
// }
//
// void sb_free(StringBuilder *sb) { free(sb->data); }
//
// /* ---------- 符号表（用于类名上下文）---------- */
// #define MAX_SYMBOLS 64
// typedef struct {
//     char name[64];
//     char kind;        // 'C' 类, 'V' 变量, 'F' 函数
// } Symbol;
//
// Symbol symtab[MAX_SYMBOLS];
// int symcnt = 0;
//
// void add_symbol(const char *name, char kind) {
//     if (symcnt < MAX_SYMBOLS) {
//         strncpy(symtab[symcnt].name, name, 63);
//         symtab[symcnt].kind = kind;
//         symcnt++;
//     }
// }
//
// char lookup_kind(const char *name) {
//     for (int i = 0; i < symcnt; i++)
//         if (strcmp(symtab[i].name, name) == 0) return symtab[i].kind;
//     return '?';
// }
//
// /* ---------- 缩进上下文栈 ---------- */
// #define MAX_STACK 256
// typedef struct {
//     int indent;
//     char kind;          // 'I' if, 'E' else if/else, 'L' 循环, 'F' 函数, 'C' 类, 'B' 普通块, 'A' 代码数组块
//     char classname[64]; // 当前类名（仅当 kind=='C' 有效）
//     char listname[64];  // 代码数组变量名（kind=='A'）
//     int chain_id;
// } Context;
//
// Context ctx_stack[MAX_STACK];
// int ctx_top = 0;
// int chain_counter = 0;
//
// void push_ctx(int indent, char kind, const char *cname, const char *lname, int chain) {
//     if (ctx_top < MAX_STACK) {
//         ctx_stack[ctx_top].indent = indent;
//         ctx_stack[ctx_top].kind = kind;
//         if (cname) strncpy(ctx_stack[ctx_top].classname, cname, 63);
//         else ctx_stack[ctx_top].classname[0] = '\0';
//         if (lname) strncpy(ctx_stack[ctx_top].listname, lname, 63);
//         else ctx_stack[ctx_top].listname[0] = '\0';
//         ctx_stack[ctx_top].chain_id = chain;
//         ctx_top++;
//     }
// }
//
// void pop_ctx(void) {
//     if (ctx_top > 0) ctx_top--;
// }
//
// Context top_ctx(void) {
//     if (ctx_top == 0) {
//         Context def = {0, '?', "", "", 0};
//         return def;
//     }
//     return ctx_stack[ctx_top - 1];
// }
//
// int current_indent(void) {
//     return top_ctx().indent;
// }
//
// const char *current_class(void) {
//     return top_ctx().classname;
// }
//
// const char *current_codearray(void) {
//     return top_ctx().listname;
// }
//
// int current_chain(void) {
//     return top_ctx().chain_id;
// }
//
// /* ---------- 输出对象 ---------- */
// StringBuilder out;
//
// /* ---------- 字符串辅助 ---------- */
// char *trim_right(char *s) {
//     char *e = s + strlen(s) - 1;
//     while (e >= s && isspace((unsigned char)*e)) e--;
//     *(e + 1) = '\0';
//     if (e >= s && *e == ':') *e = '\0';
//     return s;
// }
//
// int count_leading_spaces(const char *s) {
//     int n = 0;
//     while (*s == ' ') { n++; s++; }
//     return n;
// }
//
// char *skip_spaces(char *s) {
//     while (*s == ' ') s++;
//     return s;
// }
//
// /* 简单类型推导（增强） */
// const char *infer_value_type(const char *tok) {
//     if (strcmp(tok, "true") == 0 || strcmp(tok, "false") == 0) return "bool";
//     char *end;
//     strtod(tok, &end);
//     if (*end == '\0') {
//         return strchr(tok, '.') ? "float" : "int";
//     }
//     if (tok[0] == '"') return "string";
//     if (tok[0] == '\'' && tok[2] == '\'' && tok[3] == '\0') return "char";
//     return "var";
// }
//
// /* 从赋值值推导变量类型声明 */
// const char *decl_type(const char *val) {
//     if (val[0] == '&') {
//         // 指针类型根据星号数量简单添加
//         int stars = 0;
//         const char *p = val;
//         while (*p == '&') { p++; stars++; }
//         // 假设指向 int
//         static char buf[32];
//         strcpy(buf, "int");
//         for (int i = 0; i < stars; i++) strcat(buf, "*");
//         return buf;
//     }
//     if (strstr(val, "new ")) {
//         const char *p = val + 4;
//         while (*p == ' ') p++;
//         char cls[64];
//         sscanf(p, "%s", cls);
//         static char buf[128];
//         snprintf(buf, sizeof(buf), "%s", cls);
//         return buf;
//     }
//     return infer_value_type(val);
// }
//
// /* 判断一个标记是否可能是函数调用表达式（以 ) 结尾） */
// int is_func_call(const char *expr) {
//     // 简单处理：如果表达式以 ) 结尾且不包含 '='，则认为可能是函数调用
//     const char *end = expr + strlen(expr) - 1;
//     while (end > expr && isspace(*end)) end--;
//     if (*end == ')') {
//         // 但不能是数组访问 ] 或 }，这里只检查 )
//         return 1;
//     }
//     return 0;
// }
//
// /* ---------- 语句解析 ---------- */
// void parse_stmt(char *line, int indent) {
//     char *s = skip_spaces(line);
//     if (*s == '\0' || strchr(";/", *s)) return;
//
//     /* P( ... ) 打印 */
//     if (strncmp(s, "P(", 2) == 0) {
//         char *arg = s + 2;
//         char *end = strrchr(arg, ')');
//         if (end) *end = '\0';
//         sb_append(&out, "print %s;\n", arg);
//         return;
//     }
//
//     /* E 条件 (else if / else) */
//     if (strncmp(s, "E", 1) == 0 && (s[1] == ' ' || s[1] == '\0')) {
//         if (current_indent() == indent &&
//             (top_ctx().kind == 'I' || top_ctx().kind == 'E')) {
//             sb_append(&out, "} ");
//         }
//         char *cond = s + 1;
//         while (*cond == ' ') cond++;
//         if (*cond == '\0') {
//             sb_append(&out, "else {\n");
//         } else {
//             sb_append(&out, "else if (%s) {\n", cond);
//         }
//         pop_ctx();
//         push_ctx(indent, 'E', NULL, NULL, current_chain());
//         return;
//     }
//
//     /* if 条件 (表达式 :) - 必须排除赋值、循环、函数定义等 */
//     {
//         char *colon = strrchr(s, ':');
//         if (colon && !strchr(s, '=') && !strstr(s, " L:") && !strstr(s, "=I(") &&
//             !strstr(s, "=O.") && strncmp(s, "L", 1) != 0 && colon[1] == '\0') {
//             *colon = '\0';
//             // 进一步检查是否包含 ( 或 new (它们会形成调用或构造，不是条件)
//             if (!strstr(s, "(") && !strstr(s, "new")) {
//                 chain_counter++;
//                 sb_append(&out, "if (%s) {\n", s);
//                 push_ctx(indent, 'I', NULL, NULL, chain_counter);
//                 return;
//             }
//             *colon = ':';
//         }
//     }
//
//     /* while 循环 (表达式 L:) */
//     char *L = strstr(s, " L:");
//     if (L) {
//         *L = '\0';
//         sb_append(&out, "while (%s) {\n", s);
//         push_ctx(indent, 'L', NULL, NULL, 0);
//         return;
//     }
//
//     /* for 循环 L<num>: */
//     if (strncmp(s, "L", 1) == 0 && isdigit((unsigned char)s[1])) {
//         int num = atoi(s + 1);
//         sb_append(&out, "for (int _i = 0; _i < %d; _i++) {\n", num);
//         push_ctx(indent, 'L', NULL, NULL, 0);
//         return;
//     }
//
//     /* return 语句 R ... */
//     if (strncmp(s, "R ", 2) == 0) {
//         sb_append(&out, "return (%s);\n", s + 2);
//         return;
//     }
//
//     /* 输入 I(CONSOLE) / I(FILE,"...") */
//     if (strstr(s, "=I(CONSOLE)")) {
//         char var[64];
//         sscanf(s, "%s =I(CONSOLE)", var);
//         sb_append(&out, "string %s;\ninput(\"{}\", &%s);\n", var, var);
//         return;
//     }
//     if (strstr(s, "=I(FILE,")) {
//         char var[64], file[256];
//         sscanf(s, "%s =I(FILE,\"%[^\"]\")", var, file);
//         sb_append(&out, "String %s = file.read(\"%s\");\n", var, file);
//         return;
//     }
//
//     /* 外部变量引用 O.fun(PUB,INT,"name") */
//     if (strstr(s, "= O.fun(")) {
//         char var[64], name[64];
//         sscanf(s, "%s = O.fun(PUB,INT,\"%[^\"]\")", var, name);
//         sb_append(&out, "int %s = %s;\n", var, name);
//         return;
//     }
//
//     /* 多返回值获取 / 表获取 - 统一处理 .G( 和 .[ 语法 */
//     {
//         // 先处理 .G( 形式
//         char *Gcall = strstr(s, ".G(");
//         if (Gcall) {
//             // 切割出点前部分
//             char before[256] = {0};
//             strncpy(before, s, Gcall - s);
//             before[Gcall - s] = '\0';
//             char idx[16];
//             sscanf(Gcall + 3, "%[^)]", idx);
//             // 判断点前是否是函数调用表达式（以 ) 结尾）
//             if (is_func_call(before)) {
//                 sb_append(&out, "%sreturn(%s);\n", before, idx);
//             } else {
//                 sb_append(&out, "%sget(%s);\n", before, idx);
//             }
//             return;
//         }
//         // 再处理 .[ 语法 (如 l.[0])
//         char *bracket = strstr(s, ".[");
//         if (bracket) {
//             char before[256] = {0};
//             strncpy(before, s, bracket - s);
//             before[bracket - s] = '\0';
//             char idx[16];
//             sscanf(bracket + 2, "%[^]]", idx);
//             sb_append(&out, "%sget(%s);\n", before, idx);
//             return;
//         }
//     }
//
//     /* 类定义  name = :  (可能带继承) */
//     {
//         char *colon_eq = strstr(s, "=:");
//         if (colon_eq) {
//             char name[64] = {0};
//             strncpy(name, s, colon_eq - s);
//             name[colon_eq - s] = '\0';
//             // 去除尾部空格
//             char *p = name + strlen(name) - 1;
//             while (p >= name && isspace(*p)) *p-- = '\0';
//             sb_append(&out, "pub class %s", name);
//             // 检查继承 :base
//             char *colon = strchr(name, ':');
//             if (colon) {
//                 *colon = '\0';
//                 char *base = colon + 1;
//                 while (*base == ' ') base++;
//                 sb_append(&out, " : %s", base);
//             }
//             sb_append(&out, " {\n");
//             add_symbol(name, 'C');
//             push_ctx(indent, 'C', name, NULL, 0);
//             return;
//         }
//     }
//
//     /* 继承定义 r = i :  或 多继承 t = s,i : */
//     {
//         char *eq = strchr(s, '=');
//         char *cln = strrchr(s, ':');
//         if (eq && cln && cln > eq) {
//             // 检查冒号后面是否为空（语句结束）
//             if (cln[1] == '\0' || (cln[1] == ' ' && cln[2] == '\0')) {
//                 *eq = '\0';
//                 char *name = s;
//                 char *bases = eq + 1;
//                 *cln = '\0';
//                 // 移除 bases 前后空格
//                 while (*bases == ' ') bases++;
//                 char *endb = bases + strlen(bases) - 1;
//                 while (endb >= bases && isspace(*endb)) *endb-- = '\0';
//                 sb_append(&out, "pub class %s : %s {\n", skip_spaces(name), bases);
//                 push_ctx(indent, 'C', skip_spaces(name), NULL, 0);
//                 return;
//             }
//         }
//     }
//
//     /* 函数定义 name(params):  (可能带修饰符) */
//     {
//         char *paren = strchr(s, '(');
//         char *colon = strrchr(s, ':');
//         if (paren && colon && colon > paren && colon[1] == '\0') {
//             *colon = '\0';
//             // 提取修饰符和函数名
//             char *start = s;
//             char mod[4] = "pub";
//             if (strncmp(s, "PUB ", 4) == 0) { start += 4; while (*start == ' ') start++; }
//             else if (strncmp(s, "PRI ", 4) == 0) { strcpy(mod, "pri"); start += 4; while (*start == ' ') start++; }
//             else if (strncmp(s, "PRO ", 4) == 0) { strcpy(mod, "pro"); start += 4; while (*start == ' ') start++; }
//
//             char *name_end = paren;
//             while (name_end > start && isspace((unsigned char)*(name_end - 1))) name_end--;
//             *name_end = '\0';
//             char *name = start;
//             char *params = paren + 1;
//             char *close = strrchr(params, ')');
//             if (close) *close = '\0';
//
//             // 判断是否为构造函数（在类内部，函数名等于类名）
//             if (top_ctx().kind == 'C' && strcmp(name, current_class()) == 0) {
//                 // 构造函数，无返回值
//                 sb_append(&out, "pub %s(%s) {\n", name, params);
//             } else {
//                 sb_append(&out, "%s void %s(%s) {\n", mod, name, params);
//             }
//             push_ctx(indent, 'F', NULL, NULL, 0);
//             return;
//         }
//     }
//
//     /* 修饰符 + 变量/常量定义 PUB j = 10 */
//     if (strncmp(s, "PUB ", 4) == 0 || strncmp(s, "PRI ", 4) == 0 || strncmp(s, "PRO ", 4) == 0) {
//         char mod[4];
//         strncpy(mod, s, 3); mod[3] = '\0';
//         char *rest = s + 4;
//         while (*rest == ' ') rest++;
//         char *eq = strchr(rest, '=');
//         if (eq) {
//             *eq = '\0';
//             char *var = rest;
//             char *val = eq + 1;
//             const char *type = decl_type(val);
//             sb_append(&out, "%s %s %s = %s;\n", mod, type, var, val);
//             return;
//         }
//     }
//
//     /* 指针定义 *n = &m 或 **o = &n */
//     if (s[0] == '*') {
//         char *eq = strchr(s, '=');
//         if (eq) {
//             *eq = '\0';
//             char *var = s;
//             char *val = eq + 1;
//             int stars = 0;
//             char *p = var;
//             while (*p == '*') { stars++; p++; }
//             char type[32] = "int";
//             for (int i = 0; i < stars; i++) strcat(type, "*");
//             sb_append(&out, "%s %s = %s;\n", type, var, val);
//             return;
//         }
//     }
//
//     /* 对象创建 q = new i() */
//     if (strstr(s, "=new ")) {
//         char var[64], cls[64];
//         sscanf(s, "%s = new %s()", var, cls);
//         sb_append(&out, "%s %s = new %s();\n", cls, var, cls);
//         return;
//     }
//
//     /* 字典 / 数组 / 代码数组 定义 */
//     {
//         char *eq = strchr(s, '=');
//         if (eq) {
//             *eq = '\0';
//             char *var = s;
//             char *val = eq + 1;
//             // 去除 var 尾部空格
//             char *v_end = var + strlen(var) - 1;
//             while (v_end >= var && isspace(*v_end)) *v_end-- = '\0';
//             // 去除 val 前导空格
//             while (*val == ' ') val++;
//
//             /* 代码数组 var = :  后续缩进块生成 add */
//             if (strcmp(val, ":") == 0) {
//                 sb_append(&out, "List<yxg.System.code> %s = new List()<>;\n", var);
//                 push_ctx(indent, 'A', NULL, var, 0);
//                 return;
//             }
//
//             /* 字典形式（包含 ':' 分隔的键值对） */
//             if (strchr(val, ':') && !strchr(val, '(')) {
//                 sb_append(&out, "List<int,string> %s = new List()<>;\n", var);
//                 // 解析键值对，生成 add 语句
//                 char *tok = strtok(val, ",");
//                 while (tok) {
//                     while (*tok == ' ') tok++;
//                     char *colon = strchr(tok, ':');
//                     if (colon) {
//                         *colon = '\0';
//                         char *key = tok;
//                         char *value = colon + 1;
//                         // 去除值前后空格
//                         while (*value == ' ') value++;
//                         sb_append(&out, "%s.add(%s = %s);\n", var, key, value);
//                     }
//                     tok = strtok(NULL, ",");
//                 }
//                 return;
//             }
//
//             /* 普通数组 */
//             const char *type = infer_value_type(val);
//             sb_append(&out, "%s %s[] = {%s};\n", type, var, val);
//             return;
//         }
//     }
//
//     /* 指针修改 *n = ... 或普通赋值 */
//     if (s[0] == '*') {
//         sb_append(&out, "%s;\n", s);
//         return;
//     }
//
//     /* 普通表达式/调用，直接输出 */
//     sb_append(&out, "%s;\n", s);
// }
//
// /* ---------- 块处理 ---------- */
// void process_lines(FILE *in) {
//     char line[1024];
//     while (fgets(line, sizeof(line), in)) {
//         // 移除注释
//         char *sc = strchr(line, ';');
//         if (sc) *sc = '\0';
//         char *dc = strstr(line, "//");
//         if (dc) *dc = '\0';
//         trim_right(line);
//         if (strlen(line) == 0) continue;
//
//         int indent = count_leading_spaces(line);
//         char *content = skip_spaces(line);
//
//         // 缩进减少，出块
//         while (ctx_top > 0 && indent < current_indent()) {
//             Context c = top_ctx();
//             if (c.kind == 'I' || c.kind == 'E' || c.kind == 'L' ||
//                 c.kind == 'F' || c.kind == 'C' || c.kind == 'B') {
//                 sb_append(&out, "}\n");
//             } else if (c.kind == 'A') {
//                 // 代码数组块结束，无需关闭括号
//             }
//             pop_ctx();
//         }
//
//         // 缩进增加，进入新块
//         if (indent > current_indent()) {
//             // 判断父块类型以决定是否输出 {
//             Context parent = top_ctx();
//             if (parent.kind == 'A') {
//                 // 代码数组内的每一行生成一个 add（lambda 包装）
//                 // 此处我们生成一个简单的 lambda 包裹该行翻译
//                 // 需要捕获这一行的输出，暂时简化为直接输出代码行作为字符串 add
//                 // 真实实现需要递归解析，这里做占位处理
//                 sb_append(&out, "%s.add(() -> { ", parent.listname);
//                 // 直接解析这一行并内嵌
//                 parse_stmt(content, indent);
//                 sb_append(&out, "});\n");
//                 // 不改变上下文缩进（保持同一级）
//                 // 跳过后面的常规解析
//                 continue;
//             } else if (parent.kind != 'I' && parent.kind != 'E' && parent.kind != 'L' &&
//                        parent.kind != 'F' && parent.kind != 'C' && parent.kind != 'B') {
//                 sb_append(&out, "{\n");
//             }
//             push_ctx(indent, 'B', NULL, NULL, 0);
//             // 继续解析本条语句
//         }
//
//         parse_stmt(content, indent);
//     }
//
//     // 关闭所有剩余块
//     while (ctx_top > 0) {
//         Context c = top_ctx();
//         if (c.kind == 'I' || c.kind == 'E' || c.kind == 'L' ||
//             c.kind == 'F' || c.kind == 'C' || c.kind == 'B')
//             sb_append(&out, "}\n");
//         pop_ctx();
//     }
// }
//
// /* ---------- 主程序 ---------- */
// int main(int argc, char **argv) {
//     if (argc < 2) {
//         fprintf(stderr, "Usage: yxgcc <input.yxgch>\n");
//         return 1;
//     }
//     FILE *in = fopen(argv[1], "r");
//     if (!in) { perror(argv[1]); return 1; }
//
//     sb_init(&out);
//     process_lines(in);
//     fclose(in);
//
//     printf("%s", out.data);
//     sb_free(&out);
//     return 0;
// }