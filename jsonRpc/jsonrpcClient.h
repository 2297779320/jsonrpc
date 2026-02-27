#ifndef JSONRPC_CLIENT_H
#define JSONRPC_CLIENT_H

#include "jsonrpc.h"

EXTERN_C_BLOCK

/**********************************************************************
 * 函数名称：jsonrpc_client_create
 * 功能描述：创建JSON-RPC客户端
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
HANDLE jsonrpc_client_create(const char *host, int port);

/**********************************************************************
 * 函数名称：jsonrpc_client_set_path
 * 功能描述：设置HTTP请求路径(默认 /rpc)
 * 输入参数：path: 例如 "/rpc" 或 "/jsonrpc"
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：此接口线程安全(内部加锁)
 ***********************************************************************/
E_StateCode jsonrpc_client_set_path(HANDLE hclient, const char *path);

/**********************************************************************
 * 函数名称：jsonrpc_client_free
 * 功能描述：释放JSON-RPC客户端
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
void jsonrpc_client_free(HANDLE hclient);

/**********************************************************************
 * 函数名称：jsonrpc_client_call
 * 功能描述：远程调用JSON-RPC方法
 * 输入参数：hclient: 客户端句柄
 *          method: 方法名
 *          params: 参数JSON对象(可NULL)
 *          result: 指向结果JSON对象指针的指针(可NULL)
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
E_StateCode jsonrpc_client_call(HANDLE hclient, const char *method, 
                          cJSON *params, cJSON **result);
                          
EXTERN_C_BLOCK_END

#endif // JSONRPC_CLIENT_H