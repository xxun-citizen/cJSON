#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>

int main()
{
    // 初始化 cJSON hooks
    cJSON_InitHooks(NULL);

    // 1. 构建测试 JSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "cJSON");
    cJSON_AddNumberToObject(root, "version", 1.7);
    
    cJSON *features = cJSON_CreateArray();
    cJSON_AddItemToArray(features, cJSON_CreateString("lightweight"));
    cJSON_AddItemToArray(features, cJSON_CreateString("easy to extend"));
    cJSON_AddItemToObject(root, "features", features);
    
    cJSON *author = cJSON_CreateObject();
    cJSON_AddStringToObject(author, "name", "Dave Gamble");
    cJSON_AddNumberToObject(author, "year", 2014);
    cJSON_AddItemToObject(root, "author", author);

    // 2. 测试默认美化配置
    char *pretty_str = cJSON_PrintPretty(root);
    printf("=== Default Pretty Output ===\n%s\n", pretty_str);
    free(pretty_str);

    // 3. 测试自定义配置（制表符缩进 + 键名对齐）
    cJSON_PrintConfig custom_config = CJSON_PRINT_CONFIG_DEFAULT;
    custom_config.indent_char = "\t";    // 制表符缩进
    custom_config.indent_step = 1;       // 每层1个制表符
    custom_config.align_key = 1;         // 键名对齐
    custom_config.add_blank_line = 1;    // 对象间加空行

    char *custom_str = cJSON_PrintWithConfig(root, &custom_config);
    printf("=== Custom Pretty Output ===\n%s\n", custom_str);
    free(custom_str);

    // 4. 释放资源
    cJSON_Delete(root);
    return 0;
}