# 附录：编译错误码速查

## 词法分析错误（E1xxx）

| 错误码 | 错误名 | 说明 |
|--------|--------|------|
| E1001 | ERR_LEX_UNTERMINATED_STRING | 字符串字面量缺少闭合引号 |
| E1002 | ERR_LEX_UNTERMINATED_CHAR | 字符字面量缺少闭合单引号 |
| E1003 | ERR_LEX_UNTERMINATED_COMMENT | 多行注释缺少闭合标记 |
| E1004 | ERR_LEX_UNKNOWN_CHAR | 无法识别的字符 |
| E1005 | ERR_LEX_MULTICHAR_LITERAL | 字符字面量包含多个字符 |

## 语法分析错误（E2xxx）

| 错误码 | 错误名 | 说明 |
|--------|--------|------|
| E2001 | ERR_PARSE_UNEXPECTED_TOKEN | 意外的 Token |
| E2002 | ERR_PARSE_EXPECTED_TOKEN | 缺少预期的 Token |
| E2003 | ERR_PARSE_UNEXPECTED_EOF | 文件意外结束 |
| E2004 | ERR_PARSE_ILLEGAL_OPERAND | 非法操作数 |
| E2005 | ERR_PARSE_INVALID_TYPE | 无效的类型关键字 |
| E2006 | ERR_PARSE_MODULE_NOT_FOUND | 模块未找到 |

## 语义分析错误（E3xxx）

| 错误码 | 错误名 | 说明 |
|--------|--------|------|
| E3001 | ERR_SEM_UNDECLARED_VAR | 变量未声明 |
| E3002 | ERR_SEM_DUPLICATE_VAR | 变量重复声明 |
| E3003 | ERR_SEM_TYPE_MISMATCH | 类型不匹配 |
| E3004 | ERR_SEM_INCOMPATIBLE_TYPES | 类型不兼容 |
| E3005 | ERR_SEM_DIV_BY_ZERO | 除零错误 |
| E3006 | ERR_SEM_NOT_ARRAY | 对非数组类型使用数组操作 |
| E3007 | ERR_SEM_INVALID_INDEX | 无效的数组索引 |
| E3008 | ERR_SEM_NOT_FUNCTION | 调用了非函数类型 |
| E3009 | ERR_SEM_ARG_COUNT_MISMATCH | 函数参数数量不匹配 |
| E3010 | ERR_SEM_VOID_VALUE | void 函数返回了值 |
| E3011 | ERR_SEM_NO_MAIN | 缺少 main 函数 |
| E3012 | ERR_SEM_INVALID_BREAK | break 不在循环或 switch 中 |
| E3013 | ERR_SEM_INVALID_CONTINUE | continue 不在循环中 |
| E3014 | ERR_SEM_FIELD_NOT_FOUND | 类中未找到指定字段 |
| E3015 | ERR_SEM_METHOD_NOT_FOUND | 类中未找到指定方法 |
| E3016 | ERR_SEM_CLASS_NOT_FOUND | 类未定义 |
| E3017 | ERR_SEM_CANNOT_ASSIGN_NEG | 不能将负数赋值给无符号类型 |
| E3018 | ERR_SEM_DUPLICATE_CASE | switch 中有重复的 case 值 |
| E3019 | ERR_SEM_RETURN_TYPE_MISMATCH | 返回类型不匹配 |
| E3020 | ERR_SEM_CONDITION_NOT_BOOL | 条件表达式不是布尔类型 |

## IR 生成错误（E4xxx）

| 错误码 | 错误名 | 说明 |
|--------|--------|------|
| E4001 | ERR_IR_NULL_AST | AST 节点为空 |

## 代码生成错误（E5xxx）

| 错误码 | 错误名 | 说明 |
|--------|--------|------|
| E5001 | ERR_CG_UNKNOWN_VAR | 代码生成时遇到未知变量 |

## 内部错误（E9xxx）

| 错误码 | 错误名 | 说明 |
|--------|--------|------|
| E9001 | ERR_INTERNAL | 编译器内部错误 |
| E9002 | ERR_OUT_OF_MEMORY | 内存不足 |
| E9003 | ERR_UNREACHABLE | 执行到不应到达的代码 |
| E9004 | ERR_UNHANDLED_AST_TYPE | 未处理的 AST 节点类型 |