/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/* cJSON */
/* JSON parser in C. */

/* disable warnings about old C89 functions in MSVC */
#if !defined(_CRT_SECURE_NO_DEPRECATE) && defined(_MSC_VER)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif
#if defined(_MSC_VER)
#pragma warning (push)
/* disable warning about single line comments in system headers */
#pragma warning (disable : 4001)
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <float.h>

#ifdef ENABLE_LOCALES
#include <locale.h>
#endif

#if defined(_MSC_VER)
#pragma warning (pop)
#endif
#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#include "cJSON.h"

/* define our own boolean type */
#ifdef true
#undef true
#endif
#define true ((cJSON_bool)1)

#ifdef false
#undef false
#endif
#define false ((cJSON_bool)0)

/* define isnan and isinf for ANSI C, if in C99 or above, isnan and isinf has been defined in math.h */
#ifndef isinf
#define isinf(d) (isnan((d - d)) && !isnan(d))
#endif
#ifndef isnan
#define isnan(d) (d != d)
#endif

#ifndef NAN
#ifdef _WIN32
#define NAN sqrt(-1.0)
#else
#define NAN 0.0/0.0
#endif
#endif

typedef struct {
    const unsigned char *json;
    size_t position;
} error;
static error global_error = { NULL, 0 };

CJSON_PUBLIC(const char *) cJSON_GetErrorPtr(void)
{
    return (const char*) (global_error.json + global_error.position);
}

CJSON_PUBLIC(char *) cJSON_GetStringValue(const cJSON * const item)
{
    if (!cJSON_IsString(item))
    {
        return NULL;
    }

    return item->valuestring;
}

CJSON_PUBLIC(double) cJSON_GetNumberValue(const cJSON * const item)
{
    if (!cJSON_IsNumber(item))
    {
        return (double) NAN;
    }

    return item->valuedouble;
}

/* This is a safeguard to prevent copy-pasters from using incompatible C and header files */
#if (CJSON_VERSION_MAJOR != 1) || (CJSON_VERSION_MINOR != 7) || (CJSON_VERSION_PATCH != 19)
    #error cJSON.h and cJSON.c have different versions. Make sure that both have the same.
#endif

CJSON_PUBLIC(const char*) cJSON_Version(void)
{
    static char version[15];
    sprintf(version, "%i.%i.%i", CJSON_VERSION_MAJOR, CJSON_VERSION_MINOR, CJSON_VERSION_PATCH);

    return version;
}

/* Case insensitive string comparison, doesn't consider two NULL pointers equal though */
static int case_insensitive_strcmp(const unsigned char *string1, const unsigned char *string2)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        return 1;
    }

    if (string1 == string2)
    {
        return 0;
    }

    for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0')
        {
            return 0;
        }
    }

    return tolower(*string1) - tolower(*string2);
}

typedef struct internal_hooks
{
    void *(CJSON_CDECL *allocate)(size_t size);
    void (CJSON_CDECL *deallocate)(void *pointer);
    void *(CJSON_CDECL *reallocate)(void *pointer, size_t size);
} internal_hooks;

#if defined(_MSC_VER)
/* work around MSVC error C2322: '...' address of dllimport '...' is not static */
static void * CJSON_CDECL internal_malloc(size_t size)
{
    return malloc(size);
}
static void CJSON_CDECL internal_free(void *pointer)
{
    free(pointer);
}
static void * CJSON_CDECL internal_realloc(void *pointer, size_t size)
{
    return realloc(pointer, size);
}
#else
#define internal_malloc malloc
#define internal_free free
#define internal_realloc realloc
#endif

/* strlen of character literals resolved at compile time */
#define static_strlen(string_literal) (sizeof(string_literal) - sizeof(""))

static internal_hooks global_hooks = { internal_malloc, internal_free, internal_realloc };

static unsigned char* cJSON_strdup(const unsigned char* string, const internal_hooks * const hooks)
{
    size_t length = 0;
    unsigned char *copy = NULL;

    if (string == NULL)
    {
        return NULL;
    }

    length = strlen((const char*)string) + sizeof("");
    copy = (unsigned char*)hooks->allocate(length);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, string, length);

    return copy;
}

CJSON_PUBLIC(void) cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (hooks == NULL)  //如果hooks为空，那么将使用默认的内存管理，然后返回.
    {
        /* Reset hooks */
        global_hooks.allocate = malloc;
        global_hooks.deallocate = free;
        global_hooks.reallocate = realloc;
        return;
    }
    //也可以自定义内存管理函数
    global_hooks.allocate = malloc;
    if (hooks->malloc_fn != NULL)
    {
        global_hooks.allocate = hooks->malloc_fn;
    }
    //如果自定义了内存分配函数，那么就必须提供一个对应的内存释放函数，否则会导致内存泄漏
    global_hooks.deallocate = free;
    if (hooks->free_fn != NULL)
    {
        global_hooks.deallocate = hooks->free_fn;
    }

    
    global_hooks.reallocate = NULL;
    if ((global_hooks.allocate == malloc) && (global_hooks.deallocate == free))
    {
        global_hooks.reallocate = realloc;
    }
}

//初始化内存，返回了一个cJSON节点
static cJSON *cJSON_New_Item(const internal_hooks * const hooks)
{
    //定义一个node作为返回值，node指向一个新的cJSON节点.
    cJSON* node = (cJSON*)hooks->allocate(sizeof(cJSON));
    //f语句，如果成功malloc出节点，则调用memset函数，将内存初始化为0，大小为cJSON结构体的大小.
    if (node)
    {
        memset(node, '\0', sizeof(cJSON));
    }
    //返回node，返回类型是（cJSON*）型
    return node;
}

//删除一整个的JSON数据，同时将所有的节点全部释放内存.
CJSON_PUBLIC(void) cJSON_Delete(cJSON *item)
{
    //传入一整个cJSON类型数据，指向指针item，item指向cJSON结构体的首地址.

    cJSON *next = NULL; //定义一个cJSON类的next指针，用来递归删除整个JSON数据.

    //如果当前的指针item指向的cJSON节点不为空，进入循环体进行删除操作
    while (item != NULL)
    {
        //先定义next保存当前指针item的下一个指针指向位置，用来后面的递归遍历c=next
        next = item->next;
        //如果传入的是cJSON结构，并且item->child，则调用cJSON_Delete函数删除嵌套的孩子链表
        if (!(item->type & cJSON_IsReference) && (item->child != NULL))
        {
            cJSON_Delete(item->child);
        }
        //如果传入的是cJSON结构，并且item->valuestring，则调用全局内存管理函数global_hooks.deallocate函数删除字符串，并将item->valuestring置空
        {
            global_hooks.deallocate(item->valuestring);
            item->valuestring = NULL;
        }
        //如果传入的是cJSON结构，并且item->string，则调用global_hooks.deallocate函数释放该节点内存
        if (!(item->type & cJSON_StringIsConst) && (item->string != NULL))
        {
            global_hooks.deallocate(item->string);
            item->string = NULL;
        }
    
        global_hooks.deallocate(item);
        item = next;
    }
}

/* get the decimal point character of the current locale */
static unsigned char get_decimal_point(void)
{
#ifdef ENABLE_LOCALES
    struct lconv *lconv = localeconv();
    return (unsigned char) lconv->decimal_point[0];
#else
    return '.';
#endif
}

typedef struct
{
    const unsigned char *content;
    size_t length;
    size_t offset;
    size_t depth; /* How deeply nested (in arrays/objects) is the input at the current offset. */
    internal_hooks hooks;
} parse_buffer;

/* check if the given size is left to read in a given parse buffer (starting with 1) */
#define can_read(buffer, size) ((buffer != NULL) && (((buffer)->offset + size) <= (buffer)->length))
/* check if the buffer can be accessed at the given index (starting with 0) */
#define can_access_at_index(buffer, index) ((buffer != NULL) && (((buffer)->offset + index) < (buffer)->length))
#define cannot_access_at_index(buffer, index) (!can_access_at_index(buffer, index))
/* get a pointer to the buffer at the position */
#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

//打印数字函数:
static cJSON_bool parse_number(cJSON * const item, parse_buffer * const input_buffer)
{
    double number = 0;// 初始化变量 number，用于存储解析后的双精度浮点数。
    unsigned char *after_end = NULL;// 指向 strtod 函数解析结束后的位置，用于检查解析是否成功。
    unsigned char *number_c_string;// 指向临时字符串缓冲区，用于存储数字字符串
    unsigned char decimal_point = get_decimal_point();  // 获取当前区域设置的小数点字符（例如 '.' 或 ','）。
    size_t i = 0;// 循环计数器
    size_t number_string_length = 0;// 数字字符串的长度。
    cJSON_bool has_decimal_point = false;// 标志位，表示数字字符串是否包含小数点。


    if ((input_buffer == NULL) || (input_buffer->content == NULL)) // 检查输入缓冲区是否有效，如果无效则返回 false。
    {
        return false;
    }

    /* copy the number into a temporary buffer and replace '.' with the decimal point
     * of the current locale (for strtod)
     * This also takes care of '\0' not necessarily being available for marking the end of the input */
    for (i = 0; can_access_at_index(input_buffer, i); i++)// 循环遍历缓冲区中的字符，直到无法访问为止。
    { 
        switch (buffer_at_offset(input_buffer)[i]) // 根据当前字符进行分支处理。
        {
            case '0':// 数字字符 '0'。
            case '1':// 数字字符 '1'。
            case '2':// 数字字符 '2'。
            case '3':// 数字字符 '3'。
            case '4':// 数字字符 '4'。
            case '5':// 数字字符 '5'。
            case '6':// 数字字符 '6'。
            case '7':// 数字字符 '7'。
            case '8':// 数字字符 '8'。
            case '9':// 数字字符 '9'。
            case '+':// 数字字符 '+'。
            case '-':// 数字字符 '-'。
            case 'e':// 数字字符 'e'。
            case 'E':// 数字字符 'E'。
                number_string_length++;// 增加数字字符串长度计数。
                break;

            case '.':// 小数点 '.'。
                number_string_length++; // 增加数字字符串长度计数。
                has_decimal_point = true;// 设置标志位，表示有小数点。
                break;

            default: // 其他字符，结束循环。
                goto loop_end;
        }
    }
loop_end: // 循环结束标签。
    // 注释：为临时缓冲区分配内存，额外加 1 用于 '\0' 终止符。
    number_c_string = (unsigned char *) input_buffer->hooks.allocate(number_string_length + 1);
    if (number_c_string == NULL)// 如果分配失败，返回 false。
    {
        return false; /* allocation failure */
    }

    memcpy(number_c_string, buffer_at_offset(input_buffer), number_string_length);// 将数字字符串从缓冲区复制到临时缓冲区。
    number_c_string[number_string_length] = '\0';// 添加字符串终止符。

    if (has_decimal_point)// 如果有小数点，进行替换。
    {
        for (i = 0; i < number_string_length; i++) // 遍历临时缓冲区。
        {
            if (number_c_string[i] == '.')// 如果是 '.'，替换为区域小数点。
            {
                /* replace '.' with the decimal point of the current locale (for strtod) */
                number_c_string[i] = decimal_point;
            }
        }
    }

    number = strtod((const char*)number_c_string, (char**)&after_end);// 使用 strtod 将字符串转换为双精度浮点数。
    if (number_c_string == after_end)// 如果解析失败（after_end 指向开始位置），释放内存并返回 false。
    {
        
        input_buffer->hooks.deallocate(number_c_string); // 注释：释放临时缓冲区。
        return false; /* parse_error */
    }

    item->valuedouble = number; // 将解析后的双精度数存储到 cJSON 对象中。


    // 注释：在溢出情况下使用饱和处理。
    if (number >= INT_MAX)// 如果数字大于等于 INT_MAX，设置为 INT_MAX。
    {
        item->valueint = INT_MAX;
    }
    else if (number <= (double)INT_MIN) // 如果数字小于等于 INT_MIN，设置为 INT_MIN。
    {
        item->valueint = INT_MIN;
    }
    else // 否则，将数字转换为 int 并存储。
    {
        item->valueint = (int)number;
    }

    item->type = cJSON_Number;// 设置 cJSON 对象的类型为数字。

    input_buffer->offset += (size_t)(after_end - number_c_string);// 更新缓冲区偏移量，跳过已解析的部分。

    /* free the temporary buffer */
    input_buffer->hooks.deallocate(number_c_string);
    return true;// 解析成功，返回 true。
}

/* don't ask me, but the original cJSON_SetNumberValue returns an integer or double */
CJSON_PUBLIC(double) cJSON_SetNumberHelper(cJSON *object, double number)
{
    if (number >= INT_MAX)
    {
        object->valueint = INT_MAX;
    }
    else if (number <= (double)INT_MIN)
    {
        object->valueint = INT_MIN;
    }
    else
    {
        object->valueint = (int)number;
    }

    return object->valuedouble = number;
}

/* Note: when passing a NULL valuestring, cJSON_SetValuestring treats this as an error and return NULL */
CJSON_PUBLIC(char*) cJSON_SetValuestring(cJSON *object, const char *valuestring)
{
    char *copy = NULL;
    size_t v1_len;
    size_t v2_len;
    /* if object's type is not cJSON_String or is cJSON_IsReference, it should not set valuestring */
    if ((object == NULL) || !(object->type & cJSON_String) || (object->type & cJSON_IsReference))
    {
        return NULL;
    }
    /* return NULL if the object is corrupted or valuestring is NULL */
    if (object->valuestring == NULL || valuestring == NULL)
    {
        return NULL;
    }

    v1_len = strlen(valuestring);
    v2_len = strlen(object->valuestring);

    if (v1_len <= v2_len)
    {
        /* strcpy does not handle overlapping string: [X1, X2] [Y1, Y2] => X2 < Y1 or Y2 < X1 */
        if (!( valuestring + v1_len < object->valuestring || object->valuestring + v2_len < valuestring ))
        {
            return NULL;
        }
        strcpy(object->valuestring, valuestring);
        return object->valuestring;
    }
    copy = (char*) cJSON_strdup((const unsigned char*)valuestring, &global_hooks);
    if (copy == NULL)
    {
        return NULL;
    }
    if (object->valuestring != NULL)
    {
        cJSON_free(object->valuestring);
    }
    object->valuestring = copy;

    return copy;
}

typedef struct
{
    unsigned char *buffer;
    size_t length;
    size_t offset;
    size_t depth; /* current nesting depth (for formatted printing) */
    cJSON_bool noalloc;
    cJSON_bool format; /* is this print a formatted print */
    internal_hooks hooks;
} printbuffer;

/* realloc printbuffer if necessary to have at least "needed" bytes more */
static unsigned char* ensure(printbuffer * const p, size_t needed)
{
    unsigned char *newbuffer = NULL;
    size_t newsize = 0;

    if ((p == NULL) || (p->buffer == NULL))
    {
        return NULL;
    }

    if ((p->length > 0) && (p->offset >= p->length))
    {
        /* make sure that offset is valid */
        return NULL;
    }

    if (needed > INT_MAX)
    {
        /* sizes bigger than INT_MAX are currently not supported */
        return NULL;
    }

    needed += p->offset + 1;
    if (needed <= p->length)
    {
        return p->buffer + p->offset;
    }

    if (p->noalloc) {
        return NULL;
    }

    /* calculate new buffer size */
    if (needed > (INT_MAX / 2))
    {
        /* overflow of int, use INT_MAX if possible */
        if (needed <= INT_MAX)
        {
            newsize = INT_MAX;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        newsize = needed * 2;
    }

    if (p->hooks.reallocate != NULL)
    {
        /* reallocate with realloc if available */
        newbuffer = (unsigned char*)p->hooks.reallocate(p->buffer, newsize);
        if (newbuffer == NULL)
        {
            p->hooks.deallocate(p->buffer);
            p->length = 0;
            p->buffer = NULL;

            return NULL;
        }
    }
    else
    {
        /* otherwise reallocate manually */
        newbuffer = (unsigned char*)p->hooks.allocate(newsize);
        if (!newbuffer)
        {
            p->hooks.deallocate(p->buffer);
            p->length = 0;
            p->buffer = NULL;

            return NULL;
        }

        memcpy(newbuffer, p->buffer, p->offset + 1);
        p->hooks.deallocate(p->buffer);
    }
    p->length = newsize;
    p->buffer = newbuffer;  

    return newbuffer + p->offset;
}

/* calculate the new length of the string in a printbuffer and update the offset */
static void update_offset(printbuffer * const buffer)
{
    const unsigned char *buffer_pointer = NULL;
    if ((buffer == NULL) || (buffer->buffer == NULL))
    {
        return;
    }
    buffer_pointer = buffer->buffer + buffer->offset;

    buffer->offset += strlen((const char*)buffer_pointer);
}

/* securely comparison of floating-point variables */
static cJSON_bool compare_double(double a, double b)
{
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= maxVal * DBL_EPSILON);
}

/* Render the number nicely from the given item into a string. */
static cJSON_bool print_number(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    double d = item->valuedouble;
    int length = 0;
    size_t i = 0;
    unsigned char number_buffer[26] = {0}; /* temporary buffer to print the number into */
    unsigned char decimal_point = get_decimal_point();
    double test = 0.0;

    if (output_buffer == NULL)
    {
        return false;  
    }

    /* This checks for NaN and Infinity */
    if (isnan(d) || isinf(d))
    {
        length = sprintf((char*)number_buffer, "null");
    }
    else if(d == (double)item->valueint)
    {
        length = sprintf((char*)number_buffer, "%d", item->valueint);
    }
    else
    {
        /* Try 15 decimal places of precision to avoid nonsignificant nonzero digits */
        length = sprintf((char*)number_buffer, "%1.15g", d);

        /* Check whether the original double can be recovered */
        if ((sscanf((char*)number_buffer, "%lg", &test) != 1) || !compare_double((double)test, d))
        {
            /* If not, print with 17 decimal places of precision */
            length = sprintf((char*)number_buffer, "%1.17g", d);
        }
    }

    /* sprintf failed or buffer overrun occurred */
    if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
    {
        return false;
    }

    /* reserve appropriate space in the output */
    output_pointer = ensure(output_buffer, (size_t)length + sizeof(""));
    if (output_pointer == NULL)
    {
        return false;
    }

    /* copy the printed number to the output and replace locale
     * dependent decimal point with '.' */
    for (i = 0; i < ((size_t)length); i++)
    {
        if (number_buffer[i] == decimal_point)
        {
            output_pointer[i] = '.';
            continue;
        }

        output_pointer[i] = number_buffer[i];
    }
    output_pointer[i] = '\0';

    output_buffer->offset += (size_t)length;

    return true;
}

/* parse 4 digit hexadecimal number */
static unsigned parse_hex4(const unsigned char * const input)
{
    unsigned int h = 0;
    size_t i = 0;

    for (i = 0; i < 4; i++)
    {
        /* parse digit */
        if ((input[i] >= '0') && (input[i] <= '9'))
        {
            h += (unsigned int) input[i] - '0';
        }
        // 检查字符是否为大写字母A-F
        else if ((input[i] >= 'A') && (input[i] <= 'F'))
        {
            h += (unsigned int) 10 + input[i] - 'A';
        }
        // 检查字符是否为小写字母a-f
        else if ((input[i] >= 'a') && (input[i] <= 'f'))
        {
            h += (unsigned int) 10 + input[i] - 'a'; // 转换为10-15并累加
        }
        else /* invalid */
        {
            return 0;
        }

        if (i < 3)
        {
            /* shift left to make place for the next nibble */
            h = h << 4;
        }
    }

    return h;
}

/* converts a UTF-16 literal to UTF-8
 * A literal can be one or two sequences of the form \uXXXX */
static unsigned char utf16_literal_to_utf8(const unsigned char * const input_pointer, const unsigned char * const input_end, unsigned char **output_pointer)
{
    long unsigned int codepoint = 0;
    unsigned int first_code = 0;
    const unsigned char *first_sequence = input_pointer;
    unsigned char utf8_length = 0;
    unsigned char utf8_position = 0;
    unsigned char sequence_length = 0;
    unsigned char first_byte_mark = 0;

    if ((input_end - first_sequence) < 6)
    {
        /* input ends unexpectedly */
        goto fail;
    }

    /* get the first utf16 sequence */
    first_code = parse_hex4(first_sequence + 2);

    /* check that the code is valid */
    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF)))
    {
        goto fail;
    }

    /* UTF16 surrogate pair */
    if ((first_code >= 0xD800) && (first_code <= 0xDBFF))
    {
        const unsigned char *second_sequence = first_sequence + 6;
        unsigned int second_code = 0;
        sequence_length = 12; /* \uXXXX\uXXXX */

        if ((input_end - second_sequence) < 6)
        {
            /* input ends unexpectedly */
            goto fail;
        }

        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u'))
        {
            /* missing second half of the surrogate pair */
            goto fail;
        }

        /* get the second utf16 sequence */
        second_code = parse_hex4(second_sequence + 2);
        /* check that the code is valid */
        if ((second_code < 0xDC00) || (second_code > 0xDFFF))
        {
            /* invalid second half of the surrogate pair */
            goto fail;
        }


        /* calculate the unicode codepoint from the surrogate pair */
        codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
    }
    else
    {
        sequence_length = 6; /* \uXXXX */
        codepoint = first_code;
    }

    /* encode as UTF-8
     * takes at maximum 4 bytes to encode:
     * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (codepoint < 0x80)
    {
        /* normal ascii, encoding 0xxxxxxx */
        utf8_length = 1;
    }
    else if (codepoint < 0x800)
    {
        /* two bytes, encoding 110xxxxx 10xxxxxx */
        utf8_length = 2;
        first_byte_mark = 0xC0; /* 11000000 */
    }
    else if (codepoint < 0x10000)
    {
        /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        utf8_length = 3;
        first_byte_mark = 0xE0; /* 11100000 */
    }
    else if (codepoint <= 0x10FFFF)
    {
        /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        utf8_length = 4;
        first_byte_mark = 0xF0; /* 11110000 */
    }
    else
    {
        /* invalid unicode codepoint */
        goto fail;
    }

    /* encode as utf8 */
    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0; utf8_position--)
    {
        /* 10xxxxxx */
        (*output_pointer)[utf8_position] = (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    /* encode first byte */
    if (utf8_length > 1)
    {
        (*output_pointer)[0] = (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    }
    else
    {
        (*output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    }

    *output_pointer += utf8_length;

    return sequence_length;

fail:
    return 0;
}

/*
 * 解析输入文本中的字符串常量，解除转义并将结果存入 item。
 *
 * 参数:
 *   item           - 目标 cJSON 结构，解析后类型将设置为 cJSON_String。
 *   input_buffer   - 包含待解析 JSON 文本和当前偏移的缓冲区。
 *
 * 成功返回 true；失败（格式错误或内存不足）返回 false。
 */
static cJSON_bool parse_string(cJSON * const item, parse_buffer * const input_buffer)
{
    /* 指向开头引号之后的当前输入字符 */
    const unsigned char *input_pointer = buffer_at_offset(input_buffer) + 1;
    /* 用于查找结束引号的扫描指针，初始同 input_pointer */
    const unsigned char *input_end = buffer_at_offset(input_buffer) + 1;
    /* 输出缓冲的写入位置 */
    unsigned char *output_pointer = NULL;
    /* 分配给取消转义后字符串的缓冲起始地址 */
    unsigned char *output = NULL;

    /* 检查当前偏移处是否是双引号，非字符串则跳转失败 */
    if (buffer_at_offset(input_buffer)[0] != '\"')
    {
        goto fail;
    }

    {
        /* 估算输出长度：未计算转义字符 */
        size_t allocation_length = 0;
        size_t skipped_bytes = 0; /* 转义序列中需要跳过的额外字符 */
        while (((size_t)(input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"'))
        {
            /* 当前字符是否为反斜杠，开始转义序列 */
            if (input_end[0] == '\\')
            {
                /* 如果反斜杠是最后一个字符，则后续无转义内容，视为不完整 */
                if ((size_t)(input_end + 1 - input_buffer->content) >= input_buffer->length)
                {
                    /* prevent buffer overflow when last input character is a backslash */
                    goto fail;
                }
                skipped_bytes++;
                input_end++;
            }
            input_end++;
        }
        /* 如果到达缓冲末尾或者当前字符不是结束引号，则字符串无效 */
        if (((size_t)(input_end - input_buffer->content) >= input_buffer->length) || (*input_end != '\"'))
        {
            goto fail; /* string ended unexpectedly */
        }

        /* This is at most how much we need for the output */
        allocation_length = (size_t) (input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
        output = (unsigned char*)input_buffer->hooks.allocate(allocation_length + sizeof(""));
        if (output == NULL)
        {
            goto fail; /* allocation failure */
        }
    }

    /* 准备输出指针，写入开始 */
    output_pointer = output;
    /* 遍历字符串字面量并处理每个字符 */
    while (input_pointer < input_end)
    {
        /* 如果不是反斜杠，直接复制普通字符 */
        if (*input_pointer != '\\')
        {
            *output_pointer++ = *input_pointer++;
        }
        /* 否则处理转义序列 */
        else
        {
            /* 默认跳过两个字符：反斜杠和后续编码 */
            unsigned char sequence_length = 2;
            /* 若剩余长度不足以包含转义码，失败 */
            if ((input_end - input_pointer) < 1)
            {
                goto fail;
            }

            switch (input_pointer[1])
            {
                case 'b': /* backspace (\b) */
                    *output_pointer++ = '\b';
                    break;
                case 'f': /* formfeed (\f) */
                    *output_pointer++ = '\f';
                    break;
                case 'n': /* newline (\n) */
                    *output_pointer++ = '\n';
                    break;
                case 'r':
                    *output_pointer++ = '\r';
                    break;
                case 't':
                    *output_pointer++ = '\t';
                    break;
                case '\"':
                case '\\':
                case '/':
                    *output_pointer++ = input_pointer[1];
                    break;

                /* UTF-16 literal */
                case 'u':
                    sequence_length = utf16_literal_to_utf8(input_pointer, input_end, &output_pointer);
                    if (sequence_length == 0)
                    {
                        /* failed to convert UTF16-literal to UTF-8 */
                        goto fail;
                    }
                    break;

                default:
                    goto fail;
            }
            input_pointer += sequence_length;
        }
    }

    /* zero terminate the output */
    *output_pointer = '\0';

    item->type = cJSON_String;
    item->valuestring = (char*)output;

    input_buffer->offset = (size_t) (input_end - input_buffer->content);
    input_buffer->offset++;

    return true;

fail:
    if (output != NULL)
    {
        input_buffer->hooks.deallocate(output);
        output = NULL;
    }

    if (input_pointer != NULL)
    {
        input_buffer->offset = (size_t)(input_pointer - input_buffer->content);
    }

    return false;
}

/* Render the cstring provided to an escaped version that can be printed. */
static cJSON_bool print_string_ptr(const unsigned char * const input, printbuffer * const output_buffer)
{
    const unsigned char *input_pointer = NULL;
    unsigned char *output = NULL;
    unsigned char *output_pointer = NULL;
    size_t output_length = 0;
    /* numbers of additional characters needed for escaping */
    size_t escape_characters = 0;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* empty string */
    if (input == NULL)
    {
        output = ensure(output_buffer, sizeof("\"\""));
        if (output == NULL)
        {
            return false;
        }
        strcpy((char*)output, "\"\"");

        return true;
    }

    /* set "flag" to 1 if something needs to be escaped */
    for (input_pointer = input; *input_pointer; input_pointer++)
    {
        switch (*input_pointer)
        {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                /* one character escape sequence */
                escape_characters++;
                break;
            default:
                if (*input_pointer < 32)
                {
                    /* UTF-16 escape sequence uXXXX */
                    escape_characters += 5;
                }
                break;
        }
    }
    output_length = (size_t)(input_pointer - input) + escape_characters;

    output = ensure(output_buffer, output_length + sizeof("\"\""));
    if (output == NULL)
    {
        return false;
    }

    /* no characters have to be escaped */
    if (escape_characters == 0)
    {
        output[0] = '\"';
        memcpy(output + 1, input, output_length);
        output[output_length + 1] = '\"';
        output[output_length + 2] = '\0';

        return true;
    }

    output[0] = '\"';
    output_pointer = output + 1;
    /* copy the string */
    for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
        {
            /* normal character, copy */
            *output_pointer = *input_pointer;
        }
        else
        {
            /* character needs to be escaped */
            *output_pointer++ = '\\';
            switch (*input_pointer)
            {
                case '\\':
                    *output_pointer = '\\';
                    break;
                case '\"':
                    *output_pointer = '\"';
                    break;
                case '\b':
                    *output_pointer = 'b';
                    break;
                case '\f':
                    *output_pointer = 'f';
                    break;
                case '\n':
                    *output_pointer = 'n';
                    break;
                case '\r':
                    *output_pointer = 'r';
                    break;
                case '\t':
                    *output_pointer = 't';
                    break;
                default:
                    /* escape and print as unicode codepoint */
                    sprintf((char*)output_pointer, "u%04x", *input_pointer);
                    output_pointer += 4;
                    break;
            }
        }
    }
    output[output_length + 1] = '\"';
    output[output_length + 2] = '\0';

    return true;
}

/* Invoke print_string_ptr (which is useful) on an item. */
static cJSON_bool print_string(const cJSON * const item, printbuffer * const p)
{
    return print_string_ptr((unsigned char*)item->valuestring, p);
}

/* Predeclare these prototypes. */
static cJSON_bool parse_value(cJSON * const item, parse_buffer * const input_buffer);
static cJSON_bool print_value(const cJSON * const item, printbuffer * const output_buffer);
static cJSON_bool parse_array(cJSON * const item, parse_buffer * const input_buffer);
static cJSON_bool print_array(const cJSON * const item, printbuffer * const output_buffer);
static cJSON_bool parse_object(cJSON * const item, parse_buffer * const input_buffer);
static cJSON_bool print_object(const cJSON * const item, printbuffer * const output_buffer);
static int print_value_custom(const cJSON*item,printbuffer*p,const cJSON_PrintConfig*config,int depth);

/* Utility to jump whitespace and cr/lf */
static parse_buffer *buffer_skip_whitespace(parse_buffer * const buffer)
{
    if ((buffer == NULL) || (buffer->content == NULL))
    {
        return NULL;
    }

    if (cannot_access_at_index(buffer, 0))
    {
        return buffer;
    }

    while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] <= 32))
    {
       buffer->offset++;
    }

    if (buffer->offset == buffer->length)
    {
        buffer->offset--;
    }

    return buffer;
}

/* skip the UTF-8 BOM (byte order mark) if it is at the beginning of a buffer */
static parse_buffer *skip_utf8_bom(parse_buffer * const buffer)
{
    if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0))
    {
        return NULL;
    }

    if (can_access_at_index(buffer, 4) && (strncmp((const char*)buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0))
    {
        buffer->offset += 3;
    }

    return buffer;
}

CJSON_PUBLIC(cJSON *) cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated)
{
    size_t buffer_length;

    if (NULL == value)
    {
        return NULL;
    }

    /* Adding null character size due to require_null_terminated. */
    buffer_length = strlen(value) + sizeof("");

    return cJSON_ParseWithLengthOpts(value, buffer_length, return_parse_end, require_null_terminated);
}

/* Parse an object - create a new root, and populate. */
CJSON_PUBLIC(cJSON *) cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, cJSON_bool require_null_terminated)
{
    parse_buffer buffer = { 0, 0, 0, 0, { 0, 0, 0 } };
    cJSON *item = NULL;

    /* reset error position */
    global_error.json = NULL;
    global_error.position = 0;

    if (value == NULL || 0 == buffer_length)
    {
        goto fail;
    }

    buffer.content = (const unsigned char*)value;
    buffer.length = buffer_length;
    buffer.offset = 0;
    buffer.hooks = global_hooks;

    item = cJSON_New_Item(&global_hooks);
    if (item == NULL) /* memory fail */
    {
        goto fail;
    }

    if (!parse_value(item, buffer_skip_whitespace(skip_utf8_bom(&buffer))))
    {
        /* parse failure. ep is set. */
        goto fail;
    }

    /* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
    if (require_null_terminated)
    {
        buffer_skip_whitespace(&buffer);
        if ((buffer.offset >= buffer.length) || buffer_at_offset(&buffer)[0] != '\0')
        {
            goto fail;
        }
    }
    if (return_parse_end)
    {
        *return_parse_end = (const char*)buffer_at_offset(&buffer);
    }

    return item;

fail:
    if (item != NULL)
    {
        cJSON_Delete(item);
    }

    if (value != NULL)
    {
        error local_error;
        local_error.json = (const unsigned char*)value;
        local_error.position = 0;

        if (buffer.offset < buffer.length)
        {
            local_error.position = buffer.offset;
        }
        else if (buffer.length > 0)
        {
            local_error.position = buffer.length - 1;
        }

        if (return_parse_end != NULL)
        {
            *return_parse_end = (const char*)local_error.json + local_error.position;
        }

        global_error = local_error;
    }

    return NULL;
}

/* Default options for cJSON_Parse */
CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value)
{
    return cJSON_ParseWithOpts(value, 0, 0);
}

CJSON_PUBLIC(cJSON *) cJSON_ParseWithLength(const char *value, size_t buffer_length)
{
    return cJSON_ParseWithLengthOpts(value, buffer_length, 0, 0);
}

#define cjson_min(a, b) (((a) < (b)) ? (a) : (b))

static unsigned char *print(const cJSON * const item, cJSON_bool format, const internal_hooks * const hooks)
{
    static const size_t default_buffer_size = 256;
    printbuffer buffer[1];
    unsigned char *printed = NULL;

    memset(buffer, 0, sizeof(buffer));

    /* create buffer */
    buffer->buffer = (unsigned char*) hooks->allocate(default_buffer_size);
    buffer->length = default_buffer_size;
    buffer->format = format;
    buffer->hooks = *hooks;
    if (buffer->buffer == NULL)
    {
        goto fail;
    }

    /* print the value */
    if (!print_value(item, buffer))
    {
        goto fail;
    }
    update_offset(buffer);

    /* check if reallocate is available */
    if (hooks->reallocate != NULL)
    {
        printed = (unsigned char*) hooks->reallocate(buffer->buffer, buffer->offset + 1);
        if (printed == NULL) {
            goto fail;
        }
        buffer->buffer = NULL;
    }
    else /* otherwise copy the JSON over to a new buffer */
    {
        printed = (unsigned char*) hooks->allocate(buffer->offset + 1);
        if (printed == NULL)
        {
            goto fail;
        }
        memcpy(printed, buffer->buffer, cjson_min(buffer->length, buffer->offset + 1));
        printed[buffer->offset] = '\0'; /* just to be sure */

        /* free the buffer */
        hooks->deallocate(buffer->buffer);
        buffer->buffer = NULL;
    }

    return printed;

fail:
    if (buffer->buffer != NULL)
    {
        hooks->deallocate(buffer->buffer);
        buffer->buffer = NULL;
    }

    if (printed != NULL)
    {
        hooks->deallocate(printed);
        printed = NULL;
    }

    return NULL;
}

/* Render a cJSON item/entity/structure to text. */
CJSON_PUBLIC(char *) cJSON_Print(const cJSON *item)
{
    return (char*)print(item, true, &global_hooks);
}

CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item)
{
    return (char*)print(item, false, &global_hooks);
}

CJSON_PUBLIC(char *) cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt)
{
    printbuffer p = { 0, 0, 0, 0, 0, 0, { 0, 0, 0 } };

    if (prebuffer < 0)
    {
        return NULL;
    }

    p.buffer = (unsigned char*)global_hooks.allocate((size_t)prebuffer);
    if (!p.buffer)
    {
        return NULL;
    }

    p.length = (size_t)prebuffer;
    p.offset = 0;
    p.noalloc = false;
    p.format = fmt;
    p.hooks = global_hooks;

    if (!print_value(item, &p))
    {
        global_hooks.deallocate(p.buffer);
        p.buffer = NULL;
        return NULL;
    }

    return (char*)p.buffer;
}

CJSON_PUBLIC(cJSON_bool) cJSON_PrintPreallocated(cJSON *item, char *buffer, const int length, const cJSON_bool format)
{
    printbuffer p = { 0, 0, 0, 0, 0, 0, { 0, 0, 0 } };

    if ((length < 0) || (buffer == NULL))
    {
        return false;
    }

    p.buffer = (unsigned char*)buffer;
    p.length = (size_t)length;
    p.offset = 0;
    p.noalloc = true;
    p.format = format;
    p.hooks = global_hooks;

    return print_value(item, &p);
}

/* Parser core - when encountering text, process appropriately. */
/* 解析 JSON 值的主函数，根据输入缓冲区的内容确定值的类型并填充到 item 中 */
static cJSON_bool parse_value(cJSON * const item, parse_buffer * const input_buffer)
{
    /* 检查输入缓冲区和其内容是否为空，如果为空则无法解析，返回 false */
    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        /* 返回 false 表示解析失败 */
        return false; /* no input */
    }

    /* 开始解析不同类型的 JSON 值 */
    /* parse the different types of values */
    /* 检查是否为 null 值 */
    /* null */
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "null", 4) == 0))
    {
        /* 设置 item 的类型为 cJSON_NULL */
        item->type = cJSON_NULL;
        /* 移动缓冲区偏移量，跳过 "null" 字符串 */
        input_buffer->offset += 4;
        /* 返回 true 表示解析成功 */
        return true;
    }
    /* 检查是否为 false 值 */
    /* false */
    if (can_read(input_buffer, 5) && (strncmp((const char*)buffer_at_offset(input_buffer), "false", 5) == 0))
    {
        /* 设置 item 的类型为 cJSON_False */
        item->type = cJSON_False;
        /* 移动缓冲区偏移量，跳过 "false" 字符串 */
        input_buffer->offset += 5;
        /* 返回 true 表示解析成功 */
        return true;
    }
    /* 检查是否为 true 值 */
    /* true */
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "true", 4) == 0))
    {
        /* 设置 item 的类型为 cJSON_True */
        item->type = cJSON_True;
        /* 设置整数值为 1 */
        item->valueint = 1;
        /* 移动缓冲区偏移量，跳过 "true" 字符串 */
        input_buffer->offset += 4;
        /* 返回 true 表示解析成功 */
        return true;
    }
    /* 检查是否为字符串值，以双引号开头 */
    /* string */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '\"'))
    {
        /* 调用 parse_string 函数解析字符串 */
        return parse_string(item, input_buffer);
    }
    /* 检查是否为数字值，以 '-' 或数字开头 */
    /* number */
    if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9'))))
    {
        /* 调用 parse_number 函数解析数字 */
        return parse_number(item, input_buffer);
    }
    /* 检查是否为数组，以 '[' 开头 */
    /* array */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '['))
    {
        /* 调用 parse_array 函数解析数组 */
        return parse_array(item, input_buffer);
    }
    /* 检查是否为对象，以 '{' 开头 */
    /* object */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '{'))
    {
        /* 调用 parse_object 函数解析对象 */
        return parse_object(item, input_buffer);
    }

    /* 如果都不匹配，返回 false 表示解析失败 */
    return false;
}

/* Render a value to text. */
/* 将 cJSON 值输出为文本的函数，根据 item 的类型调用相应的打印函数 */
static cJSON_bool print_value(const cJSON * const item, printbuffer * const output_buffer)
{
    /* 初始化输出指针 */
    unsigned char *output = NULL;

    /* 检查 item 和 output_buffer 是否为空，如果为空则无法打印，返回 false */
    if ((item == NULL) || (output_buffer == NULL))
    {
        /* 返回 false 表示打印失败 */
        return false;
    }

    /* 根据 item 的类型进行 switch 分支处理 */
    switch ((item->type) & 0xFF)
    {
        /* 处理 cJSON_NULL 类型 */
        case cJSON_NULL:
            /* 确保输出缓冲区有足够空间存放 "null"（5 个字符） */
            output = ensure(output_buffer, 5);
            /* 如果确保失败，返回 false */
            if (output == NULL)
            {
                /* 返回 false 表示内存不足 */
                return false;
            }
            /* 复制 "null" 字符串到输出缓冲区 */
            strcpy((char*)output, "null");
            /* 返回 true 表示打印成功 */
            return true;

        /* 处理 cJSON_False 类型 */
        case cJSON_False:
            /* 确保输出缓冲区有足够空间存放 "false"（6 个字符） */
            output = ensure(output_buffer, 6);
            /* 如果确保失败，返回 false */
            if (output == NULL)
            {
                /* 返回 false 表示内存不足 */
                return false;
            }
            /* 复制 "false" 字符串到输出缓冲区 */
            strcpy((char*)output, "false");
            /* 返回 true 表示打印成功 */
            return true;

        /* 处理 cJSON_True 类型 */
        case cJSON_True:
            /* 确保输出缓冲区有足够空间存放 "true"（5 个字符） */
            output = ensure(output_buffer, 5);
            /* 如果确保失败，返回 false */
            if (output == NULL)
            {
                /* 返回 false 表示内存不足 */
                return false;
            }
            /* 复制 "true" 字符串到输出缓冲区 */
            strcpy((char*)output, "true");
            /* 返回 true 表示打印成功 */
            return true;

        /* 处理 cJSON_Number 类型 */
        case cJSON_Number:
            /* 调用 print_number 函数打印数字 */
            return print_number(item, output_buffer);

        /* 处理 cJSON_Raw 类型 */
        case cJSON_Raw:
        {
            /* 初始化原始长度变量 */
            size_t raw_length = 0;
            /* 检查 valuestring 是否为空 */
            if (item->valuestring == NULL)
            {
                /* 如果为空，返回 false */
                return false;
            }

            /* 计算原始字符串长度，包括空终止符 */
            raw_length = strlen(item->valuestring) + sizeof("");
            /* 确保输出缓冲区有足够空间 */
            output = ensure(output_buffer, raw_length);
            /* 如果确保失败，返回 false */
            if (output == NULL)
            {
                /* 返回 false 表示内存不足 */
                return false;
            }
            /* 复制原始字符串到输出缓冲区 */
            memcpy(output, item->valuestring, raw_length);
            /* 返回 true 表示打印成功 */
            return true;
        }

        /* 处理 cJSON_String 类型 */
        case cJSON_String:
            /* 调用 print_string 函数打印字符串 */
            return print_string(item, output_buffer);

        /* 处理 cJSON_Array 类型 */
        case cJSON_Array:
            /* 调用 print_array 函数打印数组 */
            return print_array(item, output_buffer);

        /* 处理 cJSON_Object 类型 */
        case cJSON_Object:
            /* 调用 print_object 函数打印对象 */
            return print_object(item, output_buffer);

        /* 默认情况，未知类型 */
        default:
            /* 返回 false 表示不支持的类型 */
            return false;
    }
}

/* Build an array from input text. */
/* 从输入文本解析数组的函数，解析 JSON 数组并填充到 item 中 */
static cJSON_bool parse_array(cJSON * const item, parse_buffer * const input_buffer)
{
    /* 初始化链表头指针 */
    cJSON *head = NULL; /* head of the linked list */
    /* 初始化当前项指针 */
    cJSON *current_item = NULL;

    /* 检查嵌套深度是否超过限制 */
    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        /* 如果超过，返回 false 表示嵌套过深 */
        return false; /* to deeply nested */
    }
    /* 增加嵌套深度 */
    input_buffer->depth++;

    /* 检查当前字符是否为 '['，如果不是则不是数组 */
    if (buffer_at_offset(input_buffer)[0] != '[')
    {
        /* 跳转到失败处理 */
        /* not an array */
        goto fail;
    }

    /* 移动偏移量，跳过 '[' */
    input_buffer->offset++;
    /* 跳过空白字符 */
    buffer_skip_whitespace(input_buffer);
    /* 检查是否为空数组，即紧跟着 ']' */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ']'))
    {
        /* 跳转到成功处理 */
        /* empty array */
        goto success;
    }

    /* 检查是否跳到了缓冲区末尾 */
    /* check if we skipped to the end of the buffer */
    if (cannot_access_at_index(input_buffer, 0))
    {
        /* 回退偏移量 */
        input_buffer->offset--;
        /* 跳转到失败处理 */
        goto fail;
    }

    /* 回退偏移量到第一个元素前 */
    /* step back to character in front of the first element */
    input_buffer->offset--;
    /* 循环遍历逗号分隔的数组元素 */
    /* loop through the comma separated array elements */
    do
    {
        /* 分配新项 */
        /* allocate next item */
        cJSON *new_item = cJSON_New_Item(&(input_buffer->hooks));
        /* 如果分配失败 */
        if (new_item == NULL)
        {
            /* 跳转到失败处理 */
            goto fail; /* allocation failure */
        }

        /* 将新项附加到链表 */
        /* attach next item to list */
        if (head == NULL)
        {
            /* 开始链表 */
            /* start the linked list */
            current_item = head = new_item;
        }
        else
        {
            /* 添加到末尾并前进 */
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* 解析下一个值 */
        /* parse next value */
        input_buffer->offset++;
        /* 跳过空白字符 */
        buffer_skip_whitespace(input_buffer);
        /* 如果解析值失败 */
        if (!parse_value(current_item, input_buffer))
        {
            /* 跳转到失败处理 */
            goto fail; /* failed to parse value */
        }
        /* 跳过空白字符 */
        buffer_skip_whitespace(input_buffer);
    }
    /* 循环条件：有逗号继续 */
    while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    /* 检查是否以 ']' 结束 */
    if (cannot_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ']')
    {
        /* 跳转到失败处理 */
        goto fail; /* expected end of array */
    }

success:
    /* 减少嵌套深度 */
    input_buffer->depth--;

    /* 如果头不为空，设置 prev 指针 */
    if (head != NULL) {
        head->prev = current_item;
    }

    /* 设置 item 类型为数组 */
    item->type = cJSON_Array;
    /* 设置子项为链表头 */
    item->child = head;

    /* 移动偏移量，跳过 ']' */
    input_buffer->offset++;

    /* 返回 true 表示解析成功 */
    return true;

fail:
    /* 如果头不为空，删除链表 */
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    /* 返回 false 表示解析失败 */
    return false;
}

/* Render an array to text */
/* 将数组输出为文本的函数，输出 JSON 数组格式 */
static cJSON_bool print_array(const cJSON * const item, printbuffer * const output_buffer)
{
    /* 初始化输出指针 */
    unsigned char *output_pointer = NULL;
    /* 初始化长度变量 */
    size_t length = 0;
    /* 获取数组的第一个子元素 */
    cJSON *current_element = item->child;

    /* 检查输出缓冲区是否为空 */
    if (output_buffer == NULL)
    {
        /* 返回 false 表示打印失败 */
        return false;
    }

    /* 组合输出数组 */
    /* Compose the output array. */
    /* 左方括号 */
    /* opening square bracket */
    /* 确保输出缓冲区有 1 个字节的空间 */
    output_pointer = ensure(output_buffer, 1);
    /* 如果确保失败，返回 false */
    if (output_pointer == NULL)
    {
        /* 返回 false 表示内存不足 */
        return false;
    }

    /* 写入左方括号 '[' */
    *output_pointer = '[';
    /* 增加偏移量 */
    output_buffer->offset++;
    /* 增加深度 */
    output_buffer->depth++;

    /* 遍历数组元素 */
    while (current_element != NULL)
    {
        /* 打印当前元素的值 */
        if (!print_value(current_element, output_buffer))
        {
            /* 如果打印失败，返回 false */
            return false;
        }
        /* 更新偏移量 */
        update_offset(output_buffer);
        /* 如果有下一个元素，添加逗号 */
        if (current_element->next)
        {
            /* 计算长度：格式化时为 2（逗号+空格），否则为 1（逗号） */
            length = (size_t) (output_buffer->format ? 2 : 1);
            /* 确保输出缓冲区有足够空间 */
            output_pointer = ensure(output_buffer, length + 1);
            /* 如果确保失败，返回 false */
            if (output_pointer == NULL)
            {
                /* 返回 false 表示内存不足 */
                return false;
            }
            /* 写入逗号 */
            *output_pointer++ = ',';
            /* 如果格式化，写入空格 */
            if(output_buffer->format)
            {
                *output_pointer++ = ' ';
            }
            /* 写入空终止符 */
            *output_pointer = '\0';
            /* 增加偏移量 */
            output_buffer->offset += length;
        }
        /* 移动到下一个元素 */
        current_element = current_element->next;
    }

    /* 确保输出缓冲区有 2 个字节的空间 */
    output_pointer = ensure(output_buffer, 2);
    /* 如果确保失败，返回 false */
    if (output_pointer == NULL)
    {
        /* 返回 false 表示内存不足 */
        return false;
    }
    /* 写入右方括号 ']' */
    *output_pointer++ = ']';
    /* 写入空终止符 */
    *output_pointer = '\0';
    /* 减少深度 */
    output_buffer->depth--;

    /* 返回 true 表示打印成功 */
    return true;
}

/* Build an object from the text. */
/* 从文本构建对象的函数，解析 JSON 对象并填充到 item 中 */
static cJSON_bool parse_object(cJSON * const item, parse_buffer * const input_buffer)
{
    /* 初始化链表头指针 */
    cJSON *head = NULL; /* linked list head */
    /* 初始化当前项指针 */
    cJSON *current_item = NULL;

    /* 检查嵌套深度是否超过限制 */
    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        /* 如果超过，返回 false 表示嵌套过深 */
        return false; /* to deeply nested */
    }
    /* 增加嵌套深度 */
    input_buffer->depth++;

    /* 检查是否无法访问索引 0 或当前字符不是 '{' */
    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '{'))
    {
        /* 跳转到失败处理 */
        goto fail; /* not an object */
    }

    /* 移动偏移量，跳过 '{' */
    input_buffer->offset++;
    /* 跳过空白字符 */
    buffer_skip_whitespace(input_buffer);
    /* 检查是否为空对象，即紧跟着 '}' */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '}'))
    {
        /* 跳转到成功处理 */
        goto success; /* empty object */
    }

    /* 检查是否跳到了缓冲区末尾 */
    /* check if we skipped to the end of the buffer */
    if (cannot_access_at_index(input_buffer, 0))
    {
        /* 回退偏移量 */
        input_buffer->offset--;
        /* 跳转到失败处理 */
        goto fail;
    }

    /* 回退偏移量到第一个元素前 */
    /* step back to character in front of the first element */
    input_buffer->offset--;
    /* 循环遍历逗号分隔的数组元素 */
    /* loop through the comma separated array elements */
    do
    {
        /* 分配新项 */
        /* allocate next item */
        cJSON *new_item = cJSON_New_Item(&(input_buffer->hooks));
        /* 如果分配失败 */
        if (new_item == NULL)
        {
            /* 跳转到失败处理 */
            goto fail; /* allocation failure */
        }

        /* 将新项附加到链表 */
        /* attach next item to list */
        if (head == NULL)
        {
            /* 开始链表 */
            /* start the linked list */
            current_item = head = new_item;
        }
        else
        {
            /* 添加到末尾并前进 */
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* 检查是否无法访问索引 1，即逗号后无内容 */
        if (cannot_access_at_index(input_buffer, 1))
        {
            /* 跳转到失败处理 */
            goto fail; /* nothing comes after the comma */
        }

        /* 解析子项的名称 */
        /* parse the name of the child */
        input_buffer->offset++;
        /* 跳过空白字符 */
        buffer_skip_whitespace(input_buffer);
        /* 如果解析字符串失败 */
        if (!parse_string(current_item, input_buffer))
        {
            /* 跳转到失败处理 */
            goto fail; /* failed to parse name */
        }
        /* 跳过空白字符 */
        buffer_skip_whitespace(input_buffer);

        /* 交换 valuestring 和 string，因为解析的是名称 */
        /* swap valuestring and string, because we parsed the name */
        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        /* 检查是否无法访问索引 0 或当前字符不是 ':' */
        if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':'))
        {
            /* 跳转到失败处理 */
            goto fail; /* invalid object */
        }

        /* 解析值 */
        /* parse the value */
        input_buffer->offset++;
        /* 跳过空白字符 */
        buffer_skip_whitespace(input_buffer);
        /* 如果解析值失败 */
        if (!parse_value(current_item, input_buffer))
        {
            /* 跳转到失败处理 */
            goto fail; /* failed to parse value */
        }
        /* 跳过空白字符 */
        buffer_skip_whitespace(input_buffer);
    }
    /* 循环条件：有逗号继续 */
    while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    /* 检查是否以 '}' 结束 */
    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '}'))
    {
        /* 跳转到失败处理 */
        goto fail; /* expected end of object */
    }

success:
    /* 减少嵌套深度 */
    input_buffer->depth--;

    /* 如果头不为空，设置 prev 指针 */
    if (head != NULL) {
        head->prev = current_item;
    }

    /* 设置 item 类型为对象 */
    item->type = cJSON_Object;
    /* 设置子项为链表头 */
    item->child = head;

    /* 移动偏移量，跳过 '}' */
    input_buffer->offset++;
    /* 返回 true 表示解析成功 */
    return true;

fail:
    /* 如果头不为空，删除链表 */
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    /* 返回 false 表示解析失败 */
    return false;
}

//打印cJSON对象到输出缓冲区的函数，返回操作是否成功
//参数：item：要打印的cJSON对象；output_buffer：用于存储输出的缓冲区
static cJSON_bool print_object(const cJSON * const item, printbuffer * const output_buffer)
{
    //输出缓冲区的字符指针，用于逐字符写入数据
    unsigned char *output_pointer = NULL;
    //记录需要分配的缓冲区长度
    size_t length = 0;
    //指向当前对象的第一个字节顶，即第一个键值对
    cJSON *current_item = item->child;

    //检查：输出缓冲区为空则失败
    if (output_buffer == NULL)
    {
        return false;
    }

    /* Compose the output: */
    //格式化模式时，需要”{\\n",非格式化时，只需“{”一个字符
    length = (size_t) (output_buffer->format ? 2 : 1); /* fmt: {\n */
    //保证缓冲区空间充足，+1是为了预留出“\0"结束符的位置
    output_pointer = ensure(output_buffer, length + 1);
    //缓冲区分配失败则返回false
    if (output_pointer == NULL)
    {
        return false;
    }
    //写入对象起始符，格式化时为“{\n”，非格式化时为“{”
    *output_pointer++ = '{';
    //指针后移，准备写入下一个字符
    output_buffer->depth++;
    //格式化模式：写入换行符并指针后移
    if (output_buffer->format)
    {
        *output_pointer++ = '\n';
    }
    //更新缓冲区字符数
    output_buffer->offset += length;

    //遍历所有子节点
    while (current_item)
    {
        //格式化模式：
        if (output_buffer->format)
        {
            size_t i;
            //确保缓冲区有足够空间写入缩进字符
            output_pointer = ensure(output_buffer, output_buffer->depth);
            if (output_pointer == NULL)
            {
                return false;
            }
            //写入制表符
            for (i = 0; i < output_buffer->depth; i++)
            {
                *output_pointer++ = '\t';
            }
            //更新写入的字符数
            output_buffer->offset += output_buffer->depth;
        }

        /* print key */
        //打印当前节点的键名到输出缓冲区
        if (!print_string_ptr((unsigned char*)current_item->string, output_buffer))
        {
            return false;
        }
        //更新写入字符数
        update_offset(output_buffer);
        
        //格式化模式时，需要”{\\n",非格式化时，只需“{”一个字符
        length = (size_t) (output_buffer->format ? 2 : 1);
        //确保缓冲区有足够空间写入冒号和可能的制表符
        output_pointer = ensure(output_buffer, length);
        if (output_pointer == NULL)
        {
            return false;
        }
        //写入冒号
        *output_pointer++ = ':';
        //格式化模式下，加一个制表符
        if (output_buffer->format)
        {
            *output_pointer++ = '\t';
        }
        //更新写入的字符数
        output_buffer->offset += length;

        /* print value */
        //打印值
        if (!print_value(current_item, output_buffer))
        {
            return false;
        }
        //更新写入的字符数
        update_offset(output_buffer);

        /* print comma if not last */
        //计算长度：如果格式化模式，则为逗号+换行，否则仅逗号
        length = ((size_t)(output_buffer->format ? 1 : 0) + (size_t)(current_item->next ? 1 : 0));
        //确保空间足够写入逗号和换行符
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return false;
        }
        //不是最后一个结点则写入逗号
        if (current_item->next)
        {
            *output_pointer++ = ',';
        }
        //格式化模式下写入换行符
        if (output_buffer->format)
        {
            *output_pointer++ = '\n';
        }
        //写入结束符
        *output_pointer = '\0';
        //更新写入的字符数
        output_buffer->offset += length;
        //指向下一个子节点
        current_item = current_item->next;
    }
    
    //确保缓冲区有足够空间写入对象结束符和结束符
    output_pointer = ensure(output_buffer, output_buffer->format ? (output_buffer->depth + 1) : 2);
    //格式化模式：写入符
    if (output_pointer == NULL)
    {
        return false;
    }
    //格式化模式：写入制表符
    if (output_buffer->format)
    {
        size_t i;
        for (i = 0; i < (output_buffer->depth - 1); i++)
        {
            *output_pointer++ = '\t';
        }
    }
    //写入对象结束符
    *output_pointer++ = '}';
    //写入字符串结束符
    *output_pointer = '\0';
    //减少深度
    output_buffer->depth--;
    
    //所有操作完成，返回true表示成功
    return true;
}

/* Get Array size/item / object item. */
CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array)
{
    cJSON *child = NULL;
    size_t size = 0;

    if (array == NULL)
    {
        return 0;
    }

    child = array->child;

    while(child != NULL)
    {
        size++;
        child = child->next;
    }

    /* FIXME: Can overflow here. Cannot be fixed without breaking the API */

    return (int)size;
}

static cJSON* get_array_item(const cJSON *array, size_t index)
{
    cJSON *current_child = NULL;

    if (array == NULL)
    {
        return NULL;
    }

    current_child = array->child;
    while ((current_child != NULL) && (index > 0))
    {
        index--;
        current_child = current_child->next;
    }

    return current_child;
}

CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index)
{
    if (index < 0)
    {
        return NULL;
    }

    return get_array_item(array, (size_t)index);
}

static cJSON *get_object_item(const cJSON * const object, const char * const name, const cJSON_bool case_sensitive)
{
    cJSON *current_element = NULL;

    if ((object == NULL) || (name == NULL))
    {
        return NULL;
    }

    current_element = object->child;
    if (case_sensitive)
    {
        while ((current_element != NULL) && (current_element->string != NULL) && (strcmp(name, current_element->string) != 0))
        {
            current_element = current_element->next;
        }
    }
    else
    {
        while ((current_element != NULL) && (case_insensitive_strcmp((const unsigned char*)name, (const unsigned char*)(current_element->string)) != 0))
        {
            current_element = current_element->next;
        }
    }

    if ((current_element == NULL) || (current_element->string == NULL)) {
        return NULL;
    }

    return current_element;
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItem(const cJSON * const object, const char * const string)
{
    return get_object_item(object, string, false);
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string)
{
    return get_object_item(object, string, true);
}

CJSON_PUBLIC(cJSON_bool) cJSON_HasObjectItem(const cJSON *object, const char *string)
{
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}

/* Utility for array list handling. */
static void suffix_object(cJSON *prev, cJSON *item)
{
    prev->next = item;
    item->prev = prev;
}

/* Utility for handling references. */
static cJSON *create_reference(const cJSON *item, const internal_hooks * const hooks)
{
    cJSON *reference = NULL;
    if (item == NULL)
    {
        return NULL;
    }

    reference = cJSON_New_Item(hooks);
    if (reference == NULL)
    {
        return NULL;
    }

    memcpy(reference, item, sizeof(cJSON));
    reference->string = NULL;
    reference->type |= cJSON_IsReference;
    reference->next = reference->prev = NULL;
    return reference;
}

static cJSON_bool add_item_to_array(cJSON *array, cJSON *item)
{
    cJSON *child = NULL;

    if ((item == NULL) || (array == NULL) || (array == item))
    {
        return false;
    }

    child = array->child;
    /*
     * To find the last item in array quickly, we use prev in array
     */
    if (child == NULL)
    {
        /* list is empty, start new one */
        array->child = item;
        item->prev = item;
        item->next = NULL;
    }
    else
    {
        /* append to the end */
        if (child->prev)
        {
            suffix_object(child->prev, item);
            array->child->prev = item;
        }
    }

    return true;
}

/* Add item to array/object. */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
    return add_item_to_array(array, item);
}

#if defined(__clang__) || (defined(__GNUC__)  && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))))
    #pragma GCC diagnostic push
#endif
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
/* helper function to cast away const */
static void* cast_away_const(const void* string)
{
    return (void*)string;
}
#if defined(__clang__) || (defined(__GNUC__)  && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))))
    #pragma GCC diagnostic pop
#endif


static cJSON_bool add_item_to_object(cJSON * const object, const char * const string, cJSON * const item, const internal_hooks * const hooks, const cJSON_bool constant_key)
{
    char *new_key = NULL;
    int new_type = cJSON_Invalid;

    if ((object == NULL) || (string == NULL) || (item == NULL) || (object == item))
    {
        return false;
    }

    if (constant_key)
    {
        new_key = (char*)cast_away_const(string);
        new_type = item->type | cJSON_StringIsConst;
    }
    else
    {
        new_key = (char*)cJSON_strdup((const unsigned char*)string, hooks);
        if (new_key == NULL)
        {
            return false;
        }

        new_type = item->type & ~cJSON_StringIsConst;
    }

    if (!(item->type & cJSON_StringIsConst) && (item->string != NULL))
    {
        hooks->deallocate(item->string);
    }

    item->string = new_key;
    item->type = new_type;

    return add_item_to_array(object, item);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
    return add_item_to_object(object, string, item, &global_hooks, false);
}

/* Add an item to an object with constant string as key */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item)
{
    return add_item_to_object(object, string, item, &global_hooks, true);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)
{
    if (array == NULL)
    {
        return false;
    }

    return add_item_to_array(array, create_reference(item, &global_hooks));
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item)
{
    if ((object == NULL) || (string == NULL))
    {
        return false;
    }

    return add_item_to_object(object, string, create_reference(item, &global_hooks), &global_hooks, false);
}

CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON * const object, const char * const name)
{
    cJSON *null = cJSON_CreateNull();
    if (add_item_to_object(object, name, null, &global_hooks, false))
    {
        return null;
    }

    cJSON_Delete(null);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddTrueToObject(cJSON * const object, const char * const name)
{
    cJSON *true_item = cJSON_CreateTrue();
    if (add_item_to_object(object, name, true_item, &global_hooks, false))
    {
        return true_item;
    }

    cJSON_Delete(true_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddFalseToObject(cJSON * const object, const char * const name)
{
    cJSON *false_item = cJSON_CreateFalse();
    if (add_item_to_object(object, name, false_item, &global_hooks, false))
    {
        return false_item;
    }

    cJSON_Delete(false_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean)
{
    cJSON *bool_item = cJSON_CreateBool(boolean);
    if (add_item_to_object(object, name, bool_item, &global_hooks, false))
    {
        return bool_item;
    }

    cJSON_Delete(bool_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number)
{
    cJSON *number_item = cJSON_CreateNumber(number);
    if (add_item_to_object(object, name, number_item, &global_hooks, false))
    {
        return number_item;
    }

    cJSON_Delete(number_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string)
{
    cJSON *string_item = cJSON_CreateString(string);
    if (add_item_to_object(object, name, string_item, &global_hooks, false))
    {
        return string_item;
    }

    cJSON_Delete(string_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw)
{
    cJSON *raw_item = cJSON_CreateRaw(raw);
    if (add_item_to_object(object, name, raw_item, &global_hooks, false))
    {
        return raw_item;
    }

    cJSON_Delete(raw_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON * const object, const char * const name)
{
    cJSON *object_item = cJSON_CreateObject();
    if (add_item_to_object(object, name, object_item, &global_hooks, false))
    {
        return object_item;
    }

    cJSON_Delete(object_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON * const object, const char * const name)
{
    cJSON *array = cJSON_CreateArray();
    if (add_item_to_object(object, name, array, &global_hooks, false))
    {
        return array;
    }

    cJSON_Delete(array);
    return NULL;
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemViaPointer(cJSON *parent, cJSON * const item)
{
    if ((parent == NULL) || (item == NULL) || (item != parent->child && item->prev == NULL))
    {
        return NULL;
    }

    if (item != parent->child)
    {
        /* not the first element */
        item->prev->next = item->next;
    }
    if (item->next != NULL)
    {
        /* not the last element */
        item->next->prev = item->prev;
    }

    if (item == parent->child)
    {
        /* first element */
        parent->child = item->next;
    }
    else if (item->next == NULL)
    {
        /* last element */
        parent->child->prev = item->prev;
    }

    /* make sure the detached item doesn't point anywhere anymore */
    item->prev = NULL;
    item->next = NULL;

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromArray(cJSON *array, int which)
{
    if (which < 0)
    {
        return NULL;
    }

    return cJSON_DetachItemViaPointer(array, get_array_item(array, (size_t)which));
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromArray(cJSON *array, int which)
{
    cJSON_Delete(cJSON_DetachItemFromArray(array, which));
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObject(cJSON *object, const char *string)
{
    cJSON *to_detach = cJSON_GetObjectItem(object, string);

    return cJSON_DetachItemViaPointer(object, to_detach);
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
    cJSON *to_detach = cJSON_GetObjectItemCaseSensitive(object, string);

    return cJSON_DetachItemViaPointer(object, to_detach);
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromObject(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObject(object, string));
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObjectCaseSensitive(object, string));
}

/* Replace array/object items with new ones. */
CJSON_PUBLIC(cJSON_bool) cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem)
{
    cJSON *after_inserted = NULL;

    if (which < 0 || newitem == NULL)
    {
        return false;
    }

    after_inserted = get_array_item(array, (size_t)which);
    if (after_inserted == NULL)
    {
        return add_item_to_array(array, newitem);
    }

    if (after_inserted != array->child && after_inserted->prev == NULL) {
        /* return false if after_inserted is a corrupted array item */
        return false;
    }

    newitem->next = after_inserted;
    newitem->prev = after_inserted->prev;
    after_inserted->prev = newitem;
    if (after_inserted == array->child)
    {
        array->child = newitem;
    }
    else
    {
        newitem->prev->next = newitem;
    }
    return true;
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemViaPointer(cJSON * const parent, cJSON * const item, cJSON * replacement)
{
    if ((parent == NULL) || (parent->child == NULL) || (replacement == NULL) || (item == NULL))
    {
        return false;
    }

    if (replacement == item)
    {
        return true;
    }

    replacement->next = item->next;
    replacement->prev = item->prev;

    if (replacement->next != NULL)
    {
        replacement->next->prev = replacement;
    }
    if (parent->child == item)
    {
        if (parent->child->prev == parent->child)
        {
            replacement->prev = replacement;
        }
        parent->child = replacement;
    }
    else
    {   /*
         * To find the last item in array quickly, we use prev in array.
         * We can't modify the last item's next pointer where this item was the parent's child
         */
        if (replacement->prev != NULL)
        {
            replacement->prev->next = replacement;
        }
        if (replacement->next == NULL)
        {
            parent->child->prev = replacement;
        }
    }

    item->next = NULL;
    item->prev = NULL;
    cJSON_Delete(item);

    return true;
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem)
{
    if (which < 0)
    {
        return false;
    }

    return cJSON_ReplaceItemViaPointer(array, get_array_item(array, (size_t)which), newitem);
}

static cJSON_bool replace_item_in_object(cJSON *object, const char *string, cJSON *replacement, cJSON_bool case_sensitive)
{
    if ((replacement == NULL) || (string == NULL))
    {
        return false;
    }

    /* replace the name in the replacement */
    if (!(replacement->type & cJSON_StringIsConst) && (replacement->string != NULL))
    {
        cJSON_free(replacement->string);
    }
    replacement->string = (char*)cJSON_strdup((const unsigned char*)string, &global_hooks);
    if (replacement->string == NULL)
    {
        return false;
    }

    replacement->type &= ~cJSON_StringIsConst;

    return cJSON_ReplaceItemViaPointer(object, get_object_item(object, string, case_sensitive), replacement);
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem)
{
    return replace_item_in_object(object, string, newitem, false);
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem)
{
    return replace_item_in_object(object, string, newitem, true);
}

/* Create basic types: */
CJSON_PUBLIC(cJSON *) cJSON_CreateNull(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_NULL;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateTrue(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_True;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateFalse(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_False;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateBool(cJSON_bool boolean)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = boolean ? cJSON_True : cJSON_False;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_Number;
        item->valuedouble = num;

        /* use saturation in case of overflow */
        if (num >= INT_MAX)
        {
            item->valueint = INT_MAX;
        }
        else if (num <= (double)INT_MIN)
        {
            item->valueint = INT_MIN;
        }
        else
        {
            item->valueint = (int)num;
        }
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_String;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)string, &global_hooks);
        if(!item->valuestring)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateStringReference(const char *string)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL)
    {
        item->type = cJSON_String | cJSON_IsReference;
        item->valuestring = (char*)cast_away_const(string);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObjectReference(const cJSON *child)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL) {
        item->type = cJSON_Object | cJSON_IsReference;
        item->child = (cJSON*)cast_away_const(child);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArrayReference(const cJSON *child) {
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL) {
        item->type = cJSON_Array | cJSON_IsReference;
        item->child = (cJSON*)cast_away_const(child);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateRaw(const char *raw)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_Raw;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)raw, &global_hooks);
        if(!item->valuestring)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type=cJSON_Array;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item)
    {
        item->type = cJSON_Object;
    }

    return item;
}

/* Create Arrays: */
CJSON_PUBLIC(cJSON *) cJSON_CreateIntArray(const int *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber(numbers[i]);
        if (!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateFloatArray(const float *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber((double)numbers[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateDoubleArray(const double *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber(numbers[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateStringArray(const char *const *strings, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (strings == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for (i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateString(strings[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p,n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

    return a;
}

/* Duplication */
cJSON * cJSON_Duplicate_rec(const cJSON *item, size_t depth, cJSON_bool recurse);

CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse)
{
    return cJSON_Duplicate_rec(item, 0, recurse );
}

cJSON * cJSON_Duplicate_rec(const cJSON *item, size_t depth, cJSON_bool recurse)
{
    cJSON *newitem = NULL;
    cJSON *child = NULL;
    cJSON *next = NULL;
    cJSON *newchild = NULL;

    /* Bail on bad ptr */
    if (!item)
    {
        goto fail;
    }
    /* Create new item */
    newitem = cJSON_New_Item(&global_hooks);
    if (!newitem)
    {
        goto fail;
    }
    /* Copy over all vars */
    newitem->type = item->type & (~cJSON_IsReference);
    newitem->valueint = item->valueint;
    newitem->valuedouble = item->valuedouble;
    if (item->valuestring)
    {
        newitem->valuestring = (char*)cJSON_strdup((unsigned char*)item->valuestring, &global_hooks);
        if (!newitem->valuestring)
        {
            goto fail;
        }
    }
    if (item->string)
    {
        newitem->string = (item->type&cJSON_StringIsConst) ? item->string : (char*)cJSON_strdup((unsigned char*)item->string, &global_hooks);
        if (!newitem->string)
        {
            goto fail;
        }
    }
    /* If non-recursive, then we're done! */
    if (!recurse)
    {
        return newitem;
    }
    /* Walk the ->next chain for the child. */
    child = item->child;
    while (child != NULL)
    {
        if(depth >= CJSON_CIRCULAR_LIMIT) {
            goto fail;
        }
        newchild = cJSON_Duplicate_rec(child, depth + 1, true); /* Duplicate (with recurse) each item in the ->next chain */
        if (!newchild)
        {
            goto fail;
        }
        if (next != NULL)
        {
            /* If newitem->child already set, then crosswire ->prev and ->next and move on */
            next->next = newchild;
            newchild->prev = next;
            next = newchild;
        }
        else
        {
            /* Set newitem->child and move to it */
            newitem->child = newchild;
            next = newchild;
        }
        child = child->next;
    }
    if (newitem && newitem->child)
    {
        newitem->child->prev = newchild;
    }

    return newitem;

fail:
    if (newitem != NULL)
    {
        cJSON_Delete(newitem);
    }

    return NULL;
}

static void skip_oneline_comment(char **input)
{
    *input += static_strlen("//");

    for (; (*input)[0] != '\0'; ++(*input))
    {
        if ((*input)[0] == '\n') {
            *input += static_strlen("\n");
            return;
        }
    }
}

static void skip_multiline_comment(char **input)
{
    *input += static_strlen("/*");

    for (; (*input)[0] != '\0'; ++(*input))
    {
        if (((*input)[0] == '*') && ((*input)[1] == '/'))
        {
            *input += static_strlen("*/");
            return;
        }
    }
}

static void minify_string(char **input, char **output) {
    (*output)[0] = (*input)[0];
    *input += static_strlen("\"");
    *output += static_strlen("\"");


    for (; (*input)[0] != '\0'; (void)++(*input), ++(*output)) {
        (*output)[0] = (*input)[0];

        if ((*input)[0] == '\"') {
            (*output)[0] = '\"';
            *input += static_strlen("\"");
            *output += static_strlen("\"");
            return;
        } else if (((*input)[0] == '\\') && ((*input)[1] == '\"')) {
            (*output)[1] = (*input)[1];
            *input += static_strlen("\"");
            *output += static_strlen("\"");
        }
    }
}

CJSON_PUBLIC(void) cJSON_Minify(char *json)
{
    char *into = json;

    if (json == NULL)
    {
        return;
    }

    while (json[0] != '\0')
    {
        switch (json[0])
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                json++;
                break;

            case '/':
                if (json[1] == '/')
                {
                    skip_oneline_comment(&json);
                }
                else if (json[1] == '*')
                {
                    skip_multiline_comment(&json);
                } else {
                    json++;
                }
                break;

            case '\"':
                minify_string(&json, (char**)&into);
                break;

            default:
                into[0] = json[0];
                json++;
                into++;
        }
    }

    /* and null-terminate. */
    *into = '\0';
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsInvalid(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Invalid;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsFalse(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_False;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsTrue(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xff) == cJSON_True;
}


CJSON_PUBLIC(cJSON_bool) cJSON_IsBool(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & (cJSON_True | cJSON_False)) != 0;
}
CJSON_PUBLIC(cJSON_bool) cJSON_IsNull(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_NULL;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Number;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsString(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_String;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsArray(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Array;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsObject(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Object;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsRaw(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Raw;
}

CJSON_PUBLIC(cJSON_bool) cJSON_Compare(const cJSON * const a, const cJSON * const b, const cJSON_bool case_sensitive)
{
    if ((a == NULL) || (b == NULL) || ((a->type & 0xFF) != (b->type & 0xFF)))
    {
        return false;
    }

    /* check if type is valid */
    switch (a->type & 0xFF)
    {
        case cJSON_False:
        case cJSON_True:
        case cJSON_NULL:
        case cJSON_Number:
        case cJSON_String:
        case cJSON_Raw:
        case cJSON_Array:
        case cJSON_Object:
            break;

        default:
            return false;
    }

    /* identical objects are equal */
    if (a == b)
    {
        return true;
    }

    switch (a->type & 0xFF)
    {
        /* in these cases and equal type is enough */
        case cJSON_False:
        case cJSON_True:
        case cJSON_NULL:
            return true;

        case cJSON_Number:
            if (compare_double(a->valuedouble, b->valuedouble))
            {
                return true;
            }
            return false;

        case cJSON_String:
        case cJSON_Raw:
            if ((a->valuestring == NULL) || (b->valuestring == NULL))
            {
                return false;
            }
            if (strcmp(a->valuestring, b->valuestring) == 0)
            {
                return true;
            }

            return false;

        case cJSON_Array:
        {
            cJSON *a_element = a->child;
            cJSON *b_element = b->child;

            for (; (a_element != NULL) && (b_element != NULL);)
            {
                if (!cJSON_Compare(a_element, b_element, case_sensitive))
                {
                    return false;
                }

                a_element = a_element->next;
                b_element = b_element->next;
            }

            /* one of the arrays is longer than the other */
            if (a_element != b_element) {
                return false;
            }

            return true;
        }

        case cJSON_Object:
        {
            cJSON *a_element = NULL;
            cJSON *b_element = NULL;
            cJSON_ArrayForEach(a_element, a)
            {
                /* TODO This has O(n^2) runtime, which is horrible! */
                b_element = get_object_item(b, a_element->string, case_sensitive);
                if (b_element == NULL)
                {
                    return false;
                }

                if (!cJSON_Compare(a_element, b_element, case_sensitive))
                {
                    return false;
                }
            }

            /* doing this twice, once on a and b to prevent true comparison if a subset of b
             * TODO: Do this the proper way, this is just a fix for now */
            cJSON_ArrayForEach(b_element, b)
            {
                a_element = get_object_item(a, b_element->string, case_sensitive);
                if (a_element == NULL)
                {
                    return false;
                }

                if (!cJSON_Compare(b_element, a_element, case_sensitive))
                {
                    return false;
                }
            }

            return true;
        }

        default:
            return false;
    }
}

CJSON_PUBLIC(void *) cJSON_malloc(size_t size)
{
    return global_hooks.allocate(size);
}

CJSON_PUBLIC(void) cJSON_free(void *object)
{
    global_hooks.deallocate(object);
    object = NULL;
}

/*----------扩展功能函数编写实现----------*/
//添加轻量化的字符添加函数，支持自动扩容
static int buffer_add_char(printbuffer *p, char c)
{
    // 1. 入参合法性校验
    if (p == NULL || p->buffer == NULL) {
        return 0;
    }

    // 2. 检查缓冲区是否已满，需要扩容（复用cJSON原生扩容逻辑）
    if (p->offset >= p->length) {
        // 扩容规则：每次扩展2倍，最小扩展1024字节（cJSON原生扩容策略）
        size_t new_length = (p->length == 0) ? 1024 : p->length * 2;
        char *new_buffer = (char*)realloc(p->buffer, new_length);
        if (new_buffer == NULL) {
            return 0; // 扩容失败
        }
        // 更新缓冲区指针和长度
        p->buffer = new_buffer;
        p->length = new_length;
    }

    // 3. 写入字符并更新偏移量
    p->buffer[p->offset++] = c;
    return 1;
}

//生成指定层级的缩进字符串，写入打印缓冲区
static int print_indent_custom(printbuffer *buffer,const cJSON_PrintConfig*config,int depth)
{
    if (buffer ==NULL || config ==NULL || depth < 0) {
        return 0;
    }
    
    // 计算总缩进字符数：depth * indent_step
    int indent_len = depth * config->indent_step;
    for(int i=0;i< indent_len;i++) {
        if(!buffer_add_char(buffer,config->indent_char[0])) { // 逐个添加缩进字符
            return 0;
        }

    }
    return 1;
}
//写入自定义换行符
static int print_newline_custom(printbuffer *buffer,const cJSON_PrintConfig *config)
{
    if(buffer== NULL|| config ==NULL) {
        return 0;
    }
    
    //遍历换行符字符串，逐个添加到缓冲区
    for(int i=0;config->newline_char[i]!='\0';i++) {
        if(!buffer_add_char(buffer,config->newline_char[i])) {
            return 0;
        }
    }

    //如果开启空行，额外加一个换行
    if(config->add_blank_line) {
        for(int i=0;config->newline_char[i]!='\0';i++) {
          if(!buffer_add_char(buffer,config->newline_char[i])) {
              return 0;
            }
        }
    }
    return 1;
}
//计算对象键名最大宽度
static int calculate_max_key_width(const cJSON * const object)
{
    //参数合法性测试：空指针或非对象节点，返回0
    if (object == NULL || !cJSON_IsObject(object)) {
        return 0;
    }
    //初始化最大宽度为0，遍历对象子节点，计算键名长度并更新最大宽度
    int max_width = 0;
    //指向对象的第一个子节点，开始遍历
    const cJSON *child=object->child;
    //循环遍历对象的所有键名，更新最大宽度
    while(child!=NULL) {
        //仅处理有键名的节点
        if(child->string!=NULL) {
            //计算当前键名的字符长度
            int current_key_len=strlen(child->string);
            //更新最大宽度
            if(current_key_len>max_width) {
                max_width=current_key_len;
            }
        }
        //移动到下一个节点元素
        child=child->next;
    }
    //返回最大宽度
    return max_width;
}

/*-----------改造主要函数-----------*/
//向缓冲区写入”true"
static int print_true(printbuffer *p)
{
    // 检查缓冲区有效性
    if (p == NULL) {
        return 0;
    }
    // 向缓冲区写入 "true"（长度4，含终止符）
    const char *true_str = "true";
    size_t len = strlen(true_str);
    // 调用cJSON的缓冲区添加函数（核心：自动扩容）
    for (size_t i = 0; i < len; i++) {
        if (!buffer_add_char(p, true_str[i])) {
            return 0;
        }
    }
    return 1;
}
//向缓冲区写入”false"
static int print_false(printbuffer *p)
{
    if (p == NULL) {
        return 0;
    }
    const char *false_str = "false";
    size_t len = strlen(false_str);
    for (size_t i = 0; i < len; i++) {
        if (!buffer_add_char(p, false_str[i])) {
            return 0;
        }
    }
    return 1;
}
//向缓冲区写入”null"
static int print_null(printbuffer *p)
{
    if (p == NULL) {
        return 0;
    }
    const char *null_str = "null";
    size_t len = strlen(null_str);
    for (size_t i = 0; i < len; i++) {
        if (!buffer_add_char(p, null_str[i])) {
            return 0;
        }
    }
    return 1;
}
//改造print_object()函数，增加config参数，支持自定义打印配置
static int print_object_custom(const cJSON*item,printbuffer*p,const cJSON_PrintConfig *config,int depth)
{
    int ret = 0;
    const cJSON *child = item->child;
    // 计算键名最大宽度（仅对齐时）
    int max_key_width = (config->align_key) ? calculate_max_key_width(item) : 0;

    // 写入 { + 换行 + 缩进
    ret = buffer_add_char(p, '{');
    if (!ret) return 0;
    ret = print_newline_custom(p, config); // 自定义换行
    if (!ret) return 0;

    while (child != NULL)
    {
        // 写入当前层级缩进
        ret = print_indent_custom(p, config, depth + 1);
        if (!ret) return 0;

        // 写入键名（带引号）
        ret = print_string_ptr(child->string, p); // 原生打印字符串函数
        if (!ret) return 0;

        // 键名对齐：补充空格至最大宽度
        if (config->align_key && child->string != NULL) {
            int key_len = strlen(child->string);
            int pad_len = max_key_width - key_len;
            for (int i = 0; i < pad_len; i++) {
                ret = buffer_add_char(p, ' ');
                if (!ret) return 0;
            }
        }

        // 写入冒号 + 空格（美化：冒号后加一个空格）
        ret = buffer_add_char(p, ':');
        if (!ret) return 0;
        ret = buffer_add_char(p, ' '); // 冒号后加空格，提升可读性
        if (!ret) return 0;

        // 递归打印值
        ret = print_value_custom(child, p, config, depth + 1);
        if (!ret) return 0;

        child = child->next;
        if (child != NULL) {
            // 写入逗号 + 换行
            ret = buffer_add_char(p, ',');
            if (!ret) return 0;
            ret = print_newline_custom(p, config);
            if (!ret) return 0;
        }
    }

    // 写入换行 + 缩进 + }
    ret = print_newline_custom(p, config);
    if (!ret) return 0;
    ret = print_indent_custom(p, config, depth);
    if (!ret) return 0;
    ret = buffer_add_char(p, '}');
    if (!ret) return 0;

    return 1;
}
//改造print_array()函数，增加config参数，支持自定义打印配置
static int print_array_custom(const cJSON *item, printbuffer *p, const cJSON_PrintConfig *config, int depth)
{
    int ret = 0;
    const cJSON *child = item->child;

    // 写入 [ + 换行 + 缩进
    ret = buffer_add_char(p, '[');
    if (!ret) return 0;
    ret = print_newline_custom(p, config);
    if (!ret) return 0;

    while (child != NULL)
    {
        // 写入当前层级缩进
        ret = print_indent_custom(p, config, depth + 1);
        if (!ret) return 0;

        // 递归打印数组元素
        ret = print_value_custom(child, p, config, depth + 1);
        if (!ret) return 0;

        child = child->next;
        if (child != NULL) {
            // 写入逗号 + 换行
            ret = buffer_add_char(p, ',');
            if (!ret) return 0;
            ret = print_newline_custom(p, config);
            if (!ret) return 0;
        }
    }

    // 写入换行 + 缩进 + ]
    ret = print_newline_custom(p, config);
    if (!ret) return 0;
    ret = print_indent_custom(p, config, depth);
    if (!ret) return 0;
    ret = buffer_add_char(p, ']');
    if (!ret) return 0;

    return 1;
}
//改造print_value()函数，增加config参数，支持自定义打印配置
static int print_value_custom(const cJSON*item,printbuffer*p,const cJSON_PrintConfig*config,int depth)
{
    if(item==NULL||p==NULL||config==NULL) {
        return 0;
    }
    switch(item->type) 
    {
        case cJSON_Object:
            return print_object_custom(item,p,config,depth);
        case cJSON_Array:
            return print_array_custom(item,p,config,depth);
        case cJSON_String:
            return print_string(item,p);
        case cJSON_Number:
            return print_number(item,p);
        case cJSON_True:
            return print_true(p,"true");
        case cJSON_False:
            return print_false(p,"false");
        case cJSON_NULL:
            return print_null(p,"null");
        default:
            return 0;
    }
}

