#include "cJSON.h" 
#include <stdio.h>  
int main() {     // 1. 序列化：创建JSON对象并转为字符串     cJSON *root = cJSON_CreateObject();     cJSON_AddStringToObject(root, "name", "Tom");     cJSON_AddNumberToObject(root, "age", 20);     char *json_str = cJSON_Print(root);     printf("原版序列化结果：\n%s\n\n", json_str);      // 2. 反序列化：JSON字符串转回cJSON对象     cJSON *parsed_root = cJSON_Parse(json_str);     if (parsed_root == NULL) {         printf("解析失败：%s\n", cJSON_GetErrorPtr());         return 1;     }     cJSON *name = cJSON_GetObjectItem(parsed_root, "name");     printf("反序列化结果：name = %s\n", name->valuestring);      // 3. 释放内存（避免内存泄漏）     cJSON_Delete(root);     cJSON_Delete(parsed_root);     free(json_str);     return 0; }

}