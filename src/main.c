#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "Codegen.h"
#include "IR.h"
#include "Lexer.h"
#include "parser.h"
#include "Semantics.h"
#include "CFG.h"


#ifdef _WIN32
#define TokenType  Win32_TokenType_avoid_conflict
#include <windows.h>
#undef TokenType
#undef ERROR
#endif

#include "Error.h"
#include "SSA.h"

bool checkFileSuffix(const char *filename, const char *suffix) {
    if (filename == NULL || suffix == NULL || *filename == '\0' || *suffix == '\0') {
        ERROR(ERR_PARSE_MODULE_NOT_FOUND, LOC_NONE, "file name cannot be empty");
        return false;
    }

    const char *dot = strrchr(filename, '.');
    if (dot == NULL) {
        return false;
    }

    if (strcmp(dot + 1, suffix) == 0) {
        return true;
    } else {
        return false;
    }
}

char* read_source(char* file_name)
{
    if (file_name == NULL) {
        ERROR(ERR_PARSE_MODULE_NOT_FOUND, LOC_NONE, "file name cannot be empty");
        return NULL;
    }
    if (!checkFileSuffix(file_name, "yxg")) {
        ERROR(ERR_PARSE_MODULE_NOT_FOUND, LOC(file_name, 0, 0), "file type must be .yxg");
        return NULL;
    }
    String* string = string_read_from_file(file_name);
    if (!string || string_is_empty(string)) {
        ERROR(ERR_PARSE_MODULE_NOT_FOUND, LOC(file_name, 0, 0), "file is empty");
        if (string) string_destroy(string);
        return NULL;
    }
    char* temp = malloc(string_length(string) + 1);
    strcpy(temp, string_cstr(string));
    string_destroy(string);
    return temp;
}

void print_help()
{
    printf("Usage: yxg file_name [Parameter]\n");
    printf("Parameter:\n");
    printf("-o: Change output name\n");
    printf("-r: Compile and run\n");
    printf("-d: Debug mode(No real use)\n");
    printf("-a: Compile to assembly only\n");
    printf("-time: Show current time\n");
    printf("-version: Show current version\n");
    printf("-help: Show this help information\n");
}

int main(int argc, char** argv)
{
    error_init();

    clock_t start_time = clock();
    char* file_name = NULL;
    char* output_file_name = NULL;
    bool is_executable = false;
    bool is_debug = false;
    bool only_asm = false;

    if (argc < 2)
    {
        print_help();
        return 0;
    }
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0)
        {
            output_file_name = argv[++i];
        } else if (strcmp(argv[i], "-help") == 0)
        {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-version") == 0)
        {
            printf("version: 1.0.0-alpha.1");
            return 0;
        } else if (strcmp(argv[i], "-time") == 0)
        {
            time_t current_timestamp;
            char time_str[100];
            time(&current_timestamp);
            strftime(time_str, sizeof(time_str), "Current time: %Y-%m-%d %H:%M:%S", localtime(&current_timestamp));
            puts(time_str);
            return 0;
        } else if (strcmp(argv[i], "-r") == 0)
        {
            is_executable = true;
        } else if (strcmp(argv[i],"-d") == 0)
        {
            is_debug = true;
        } else if (strcmp(argv[i], "-a") == 0)
        {
            only_asm = true;
        } else
        {
            file_name = argv[i];
        }
    }

    if (output_file_name == NULL)
    {
        output_file_name = "output.exe";
    }
    {
        char exe_dir[MAX_PATH] = "";
#ifdef _WIN32
        GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
        char* last_sep = strrchr(exe_dir, '\\');
        if (last_sep) *last_sep = '\0';
#else
        ssize_t len = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
        if (len != -1) {
            exe_dir[len] = '\0';
            char* last_sep = strrchr(exe_dir, '/');
            if (last_sep) *last_sep = '\0';
        }
#endif
        module_path_add_defaults(exe_dir);
    }

    Lexer*  lexer_instances[256] = {NULL};
    int     lexer_count = 0;

    ASTNode* ast_node = NULL;
    ASTNode* ast_tail = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-o") == 0) i++;
            continue;
        }
        char* src = read_source(argv[i]);
        if (!src) {
            ERROR(ERR_PARSE_MODULE_NOT_FOUND, LOC(argv[i], 0, 0), "cannot read '%s'", argv[i]);
            continue;
        }
        Lexer* lex = lexer(src, argv[i]);
        if (lexer_count < 256) {
            lexer_instances[lexer_count++] = lex;
        }
        Parser* p = parser_init(lex->tokens, argv[i]);
        ASTNode* file_ast = parser_parse(p);
        if (!file_ast) continue;

        if (!ast_node) {
            ast_node = file_ast;
            ast_tail = file_ast;
        } else {
            ast_tail->next = file_ast;
        }
        while (ast_tail->next) ast_tail = ast_tail->next;
    }
    if (!ast_node) {
        FATAL(ERR_INTERNAL, LOC_NONE, "no input files");
    }

    // Print_AST(ast_node,0,false);
    // if (has_error)
    // {
    //     clock_t end_time = clock();
    //     double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    //     fprintf(stderr,"BUILD FAILED in %.6fs\n",elapsed_time);
    //     AST_Free(ast_node);
    //     for (int i = 0; i < lexer_count; i++) lexer_free(lexer_instances[i]);
    //     module_system_cleanup();
    //     semantics_cleanup();
    //     codegen_cleanup();
    //     optimizer_cleanup();
    //     exit(EXIT_FAILURE);
    // }

    Semantics(ast_node);
    // Print_AST(ast_node,0,true);

    if (has_error)
    {
        clock_t end_time = clock();
        double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
        fprintf(stderr,"BUILD FAILED in %.6fs\n",elapsed_time);
        // AST_Free(ast_node);
        for (int i = 0; i < lexer_count; i++) lexer_free(lexer_instances[i]);
        module_system_cleanup();
        semantics_cleanup();
        codegen_cleanup();
        return 1;
    }

    IR* ir = parse_IR(ast_node);
    // print_IR(ir);

    IR* full_ir_head = NULL;
    IR* full_ir_tail = NULL;

    regmap_init();
    FuncIR* func_list = ir_split_by_function(ir);
    for (FuncIR* fi = func_list; fi; fi = fi->next) {
        if (fi->func_name && strcmp(fi->func_name, "__preamble__") == 0 ||
            strcmp(fi->func_name, "__interlude__") == 0) {
            if (!full_ir_head) {
                full_ir_head = fi->first_ir;
            } else {
                full_ir_tail->next = fi->first_ir;
            }
            full_ir_tail = fi->last_ir;
            while (full_ir_tail->next) full_ir_tail = full_ir_tail->next;
            continue;
        }
        CFG* cfg = cfg_build(fi->first_ir, fi->last_ir);
        if (!cfg) continue;

        cfg_remove_unreachable(cfg);
        if (cfg->block_count == 0) {
            cfg_destroy(cfg);
            continue;
        }

        cfg_assign_linear_ids(cfg);

        cfg_compute_dominators(cfg);
        cfg_compute_dominance_frontiers(cfg);

        ssa_construct(cfg);
        // ssa_print_cfg(cfg);

        for (int round = 0; round < 4; round++) {
            int changes = 0;
            changes += ssa_constant_propagation(cfg);
            changes += ssa_constant_folding(cfg);
            changes += ssa_copy_propagation(cfg);
            changes += ssa_dead_code_elimination(cfg);
            if (changes == 0) break;
        }

        IR* func_ir = ssa_destruct(cfg);

        cfg_liveness_analysis(cfg);

        regmap_allocate_from_cfg(cfg, 6);

        // cfg_print(cfg);
        cfg_destroy(cfg);
        if (func_ir) {
            if (!full_ir_head) {
                full_ir_head = func_ir;
            } else {
                full_ir_tail->next = func_ir;
            }
            full_ir_tail = func_ir;
            while (full_ir_tail->next) full_ir_tail = full_ir_tail->next;
        }
    }
    func_ir_list_free(func_list);

    // printf("\n=== Final IR (after SSA) ===\n");
    // print_IR(full_ir_head);
    // printf("=== End Final IR ===\n\n");

    FILE* output = fopen("output.asm", "w");
    codegen_win32_init();
    codegen_win32(output, full_ir_head);
    codegen_win32_end(output);
    fclose(output);

    free_IR_chain(full_ir_head);

    if (only_asm)
    {
        clock_t end_time = clock();
        double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
        printf("Compilation complete, output file is: %s\n",output_file_name);
        printf("Total compilation time: %.6f second\n", elapsed_time);
        return 0;
    }

    if (has_print)
    {
        if (system("nasm -g -f win32 -O0 runtime/runtime_print.asm -o runtime_print.obj") != 0)
        {
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
            return 1;
        }
    }
    if (has_gc)
    {
        if (system("nasm -g -f win32 -O0 runtime/runtime_gc.asm -o runtime_gc.obj") != 0)
        {
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
            return 1;
        }
    }
    if (has_string_lib)
    {
        if (system("nasm -g -f win32 -O0 runtime/runtime_string.asm -o runtime_string.obj") != 0)
        {
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
            return 1;
        }
    }
    if (has_memory_lib)
    {
        if (system("nasm -g -f win32 -O0 runtime/runtime_memory.asm -o runtime_memory.obj"))
        {
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
            return 1;
        }
    }
    if (has_float_lib)
    {
        if (system("nasm -g -f win32 -O0 runtime/runtime_float.asm -o runtime_float.obj"))
        {
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
            return 1;
        }
    }
    if (has_array_lib)
    {
        if (system("nasm -g -f win32 -O0 runtime/runtime_array.asm -o runtime_array.obj"))
        {
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
            return 1;
        }
    }
    if (main_has_args || has_input || has_format || has_system_lib)
    {
        if (system("nasm -g -f win32 -O0 runtime/runtime_system.asm -o runtime_system.obj"))
        {
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
            return 1;
        }
    }
    if (has_bounds_check)
    {
        if (system("nasm -g -f win32 -O0 runtime/runtime_bounds.asm -o runtime_bounds.obj"))
        {
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
            fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
            return 1;
        }
    }

    if (system("nasm -g output.asm -O0 -f win32 -o output.obj") != 0)
    {
        clock_t end_time = clock();
        double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
        fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
        return 1;
    }

    char* command = "gcc -m32 -g";
    if (has_print)
    {
        int len = snprintf(NULL, 0, "%s runtime_print.obj",command);
        char* temp = malloc(len + 1);
        if (temp) {
            snprintf(temp, len + 1, "%s runtime_print.obj",command);
        }
        command = strdup(temp);
        free(temp);
    }
    if (has_gc)
    {
        int len = snprintf(NULL, 0, "%s runtime_gc.obj",command);
        char* temp = malloc(len + 1);
        if (temp) {
            snprintf(temp, len + 1, "%s runtime_gc.obj",command);
        }
        command = strdup(temp);
        free(temp);
    }
    if (has_string_lib)
    {
        int len = snprintf(NULL, 0, "%s runtime_string.obj",command);
        char* temp = malloc(len + 1);
        if (temp) {
            snprintf(temp, len + 1, "%s runtime_string.obj",command);
        }
        command = strdup(temp);
        free(temp);
    }
    if (has_memory_lib)
    {
        int len = snprintf(NULL, 0, "%s runtime_memory.obj",command);
        char* temp = malloc(len + 1);
        if (temp) {
            snprintf(temp, len + 1, "%s runtime_memory.obj",command);
        }
        command = strdup(temp);
        free(temp);
    }
    if (has_float_lib)
    {
        int len = snprintf(NULL, 0, "%s runtime_float.obj",command);
        char* temp = malloc(len + 1);
        if (temp) {
            snprintf(temp, len + 1, "%s runtime_float.obj",command);
        }
        command = strdup(temp);
        free(temp);
    }
    if (has_array_lib)
    {
        int len = snprintf(NULL, 0, "%s runtime_array.obj",command);
        char* temp = malloc(len + 1);
        if (temp) {
            snprintf(temp, len + 1, "%s runtime_array.obj",command);
        }
        command = strdup(temp);
        free(temp);
    }
    if (main_has_args || has_input || has_format || has_system_lib)
    {
        int len = snprintf(NULL, 0, "%s runtime_system.obj",command);
        char* temp = malloc(len + 1);
        if (temp) {
            snprintf(temp, len + 1, "%s runtime_system.obj",command);
        }
        command = strdup(temp);
        free(temp);
    }
    if (has_bounds_check)
    {
        int len = snprintf(NULL, 0, "%s runtime_bounds.obj",command);
        char* temp = malloc(len + 1);
        if (temp) {
            snprintf(temp, len + 1, "%s runtime_bounds.obj",command);
        }
        command = strdup(temp);
        free(temp);
    }

    int len = snprintf(NULL, 0, "%s output.obj -o %s",command,output_file_name);
    char* temp = malloc(len + 1);
    if (temp) {
        snprintf(temp, len + 1, "%s output.obj -o %s",command,output_file_name);
    }
    command = strdup(temp);
    free(temp);

    if (system(command) != 0)
    {
        clock_t end_time = clock();
        double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
        fprintf(stderr, "Build Failed in %.6fs!\n",elapsed_time);
        return 1;
    }

    AST_Free(ast_node);
    for (int i = 0; i < lexer_count; i++) {
        lexer_free(lexer_instances[i]);
    }
    module_system_cleanup();
    semantics_cleanup();
    codegen_cleanup();
    clock_t end_time = clock();

    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Compilation complete, output file is: %s\n",output_file_name);
    printf("Total compilation time: %.6f second\n", elapsed_time);
    if (is_executable)
    {
        system(output_file_name);
    }
    
    remove("output.asm");
    remove("output.obj");
    remove("runtime_print.obj");
    remove("runtime_string.obj");
    remove("runtime_array.obj");
    remove("runtime_bounds.obj");
    remove("runtime_float.obj");
    remove("runtime_system.obj");
    remove("runtime_gc.obj");
    remove("runtime_memory.obj");

    if (error_occurred()) {
        fprintf(stderr, "\n%d error(s), %d warning(s)\n",
                error_count(), warning_count());
    }
    error_cleanup();
    return error_occurred() ? 1 : 0;
}
