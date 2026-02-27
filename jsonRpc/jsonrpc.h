#ifndef JSONRPC_H
#define JSONRPC_H

#include "cJSON.h"
#include "defs.h"

EXTERN_C_BLOCK

#define JSONRPC_VERSION "2.0"
#define STACK_SIZE_DEFAULT 1024 * 1024 /* 默认栈大小 (1MB) */
#define MAX_SMALL_BUFFER_SIZE 256
#define MAX_SMALL_BUFFER_SIZE 512
#define MAX_BUFFER_SIZE 4096
#define MAX_BUFFER_SIZE 4096
#define MAX_HEADER_SIZE 1024


#ifndef JSON_RPC_2_0_H
#define JSON_RPC_2_0_H

/* JSON-RPC 2.0 协议支持 */
#define JSON_RPC_VERSION "2.0"
#define JSON_RPC_VERSION_ID 200

/* JSON-RPC 2.0 标准错误码 */
#define JSON_RPC_PARSE_ERROR          -32700
#define JSON_RPC_INVALID_REQUEST      -32600
#define JSON_RPC_METHOD_NOT_FOUND     -32601
#define JSON_RPC_INVALID_PARAMS       -32602
#define JSON_RPC_INTERNAL_ERROR       -32603

/* JSON-RPC 2.0 响应字段 */
#define JSON_RPC_FIELD_VERSION      "jsonrpc"
#define JSON_RPC_FIELD_RESULT       "result"
#define JSON_RPC_FIELD_ERROR        "error"
#define JSON_RPC_FIELD_ID           "id"
#define JSON_RPC_FIELD_METHOD       "method"
#define JSON_RPC_FIELD_PARAMS       "params"

#endif /* JSON_RPC_2_0_H */

#ifndef HTTP_STATUS_CODES_H
#define HTTP_STATUS_CODES_H

/* HTTP状态码定义 */
#define HTTP_STATUS_CONTINUE           100
#define HTTP_STATUS_OK                 200
#define HTTP_STATUS_CREATED            201
#define HTTP_STATUS_NO_CONTENT         204
#define HTTP_STATUS_BAD_REQUEST        400
#define HTTP_STATUS_UNAUTHORIZED       401
#define HTTP_STATUS_FORBIDDEN          403
#define HTTP_STATUS_NOT_FOUND          404
#define HTTP_STATUS_INTERNAL_ERROR     500
#define HTTP_STATUS_BAD_GATEWAY        502
#define HTTP_STATUS_SERVICE_UNAVAILABLE 503

#endif /* HTTP_STATUS_CODES_H */

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

/* 错误码定义 */
#define ERROR_SUCCESS              0
#define ERROR_INVALID_PARAMETER   -1
#define ERROR_OUT_OF_MEMORY       -2
#define ERROR_NETWORK_FAILED      -3
#define ERROR_TIMEOUT            -4
#define ERROR_INVALID_JSON        -5
#define ERROR_METHOD_NOT_FOUND    -6
#define ERROR_INVALID_REQUEST     -7

#endif /* ERROR_CODES_H */

#ifndef MEMORY_MANAGEMENT_H
#define MEMORY_MANAGEMENT_H

/* 安全内存管理宏 */
#define SAFE_MALLOC(size) malloc(size)
#define SAFE_CALLOC(nmemb, size) calloc(nmemb, size)
#define SAFE_REALLOC(ptr, size) realloc(ptr, size)
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

/* 内存分配检查宏 */
#define CHECK_MALLOC(ptr) do { \
    if ((ptr) == NULL) { \
        return ERROR_OUT_OF_MEMORY; \
    } \
} while(0)

#endif /* MEMORY_MANAGEMENT_H */

typedef struct __T_JsonRpcMsg
{
    String128	strMethod;
	void*	    pcData;
    UINT32      uiCallId;
} T_JsonRpcMsg;

// 工具函数
/**********************************************************************
 * 函数名称：jsonrpc_create_request
 * 功能描述：创建JSON-RPC请求对象
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
cJSON* jsonrpc_create_request(const char *method, cJSON *params, cJSON *id);

/**********************************************************************
 * 函数名称：jsonrpc_create_response
 * 功能描述：创建JSON-RPC响应对象
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
cJSON* jsonrpc_create_response(cJSON *result, cJSON *id);

/**********************************************************************
 * 函数名称：jsonrpc_create_error
 * 功能描述：创建JSON-RPC错误对象
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
cJSON* jsonrpc_create_error(int code, const char *message, cJSON *data, cJSON *id);

/**********************************************************************
 * 函数名称：jsonrpc_error_message
 * 功能描述：获取错误消息字符串
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
const char* jsonrpc_error_message(int code);

EXTERN_C_BLOCK_END

#endif // JSONRPC_H