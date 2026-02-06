#ifndef JSONRPC_H
#define JSONRPC_H

#include "cJSON.h"
#include "defs.h"

EXTERN_C_BLOCK

#define JSONRPC_VERSION "2.0"

// 错误码定义
typedef enum {
    JSONRPC_PARSE_ERROR = -32700,
    JSONRPC_INVALID_REQUEST = -32600,
    JSONRPC_METHOD_NOT_FOUND = -32601,
    JSONRPC_INVALID_PARAMS = -32602,
    JSONRPC_INTERNAL_ERROR = -32603
} jsonrpc_error_code_t;

typedef struct __T_JsonRpcMsg
{
    String64	strMethod;
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