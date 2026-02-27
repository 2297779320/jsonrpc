#include "jsonrpcService.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <strings.h>
#include "cJSON.h"
#include "common.h"

 /***********************************************************
 *                      常量定义                                    *
 **********************************************************/

/***********************************************************
 *              文件内部使用的宏                      *
 **********************************************************/
#define RPC_MAX_CONNECTIONS  1  // 只允许一个客户端连接

#define JSONRPC_SERVICE_CHECK_AND_SET(ptSetup, retVal) \
	do                                              \
	{                                               \
		if (NULL == ptSetup)                        \
		{                                           \
			return retVal;                          \
		}                                           \
	} while (0)

 /***********************************************************
 *          文件内部使用的数据类型     *
 **********************************************************/
typedef struct __jsonrpc_service_t
{
    int port;
    int server_fd;

    TSK_Handle hDetectTsk;
    BOOL bTskDone;

    // T_StdListDef  tMethodList;
    CommQueID  Msgque;
	T_MutexObj tMutex; // 操作锁

    // void *user_data;

} jsonrpc_service_t;

typedef struct
{
    int client_fd;
    jsonrpc_service_t *service;
} client_context_t;

/* ========== 内部函数 ========== */

static int socket_send_all(int fd, const void *data, size_t len)
{
    size_t sent = 0;
    const char *p = (const char *)data;
    while (sent < len)
    {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0)
        {
            return ERROR_INVALID_REQUEST;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int socket_send(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0)
        {
            return ERROR_INVALID_REQUEST;
        }
        sent += n;
    }
    return 0;
}

static int socket_recv_all(int fd, void *buffer, size_t len)
{
    size_t received = 0;
    char *p = (char *)buffer;
    while (received < len)
    {
        ssize_t n = recv(fd, p + received, len - received, 0);
        if (n <= 0)
            return ERROR_INVALID_REQUEST;
        received += (size_t)n;
    }
    return 0;
}

static int http_read_headers(int fd, char *header_buf, size_t header_buf_sz, size_t *out_header_len)
{
    size_t used = 0;
    if (!header_buf || header_buf_sz < 4)
        return ERROR_INVALID_REQUEST;

    while (used + 1 < header_buf_sz)
    {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0)
            return ERROR_INVALID_REQUEST;
        header_buf[used++] = c;
        header_buf[used] = '\0';

        if (used >= 4 &&
            header_buf[used - 4] == '\r' &&
            header_buf[used - 3] == '\n' &&
            header_buf[used - 2] == '\r' &&
            header_buf[used - 1] == '\n')
        {
            if (out_header_len)
                *out_header_len = used;
            return 0;
        }
    }
    return ERROR_INVALID_REQUEST;
}

static long http_parse_content_length(const char *headers)
{
    if (!headers)
        return ERROR_INVALID_REQUEST;

    const char *p = headers;
    while (*p)
    {
        const char *line = p;
        const char *eol = strstr(line, "\r\n");
        if (!eol)
            break;
        p = eol + 2;

        const char *key = "Content-Length:";
        size_t key_len = strlen(key);
        if (strncasecmp(line, key, key_len) == 0)
        {
            const char *v = line + key_len;
            while (*v == ' ' || *v == '\t')
                v++;
            char *endptr = NULL;
            long val = strtol(v, &endptr, 10);
            if (endptr == v || val < 0)
                return ERROR_INVALID_REQUEST;
            return val;
        }
    }

    return ERROR_INVALID_JSON;
}

static int http_send_response(int fd, int status, const char *json_body)
{
    const char *reason = "OK";
    if (status == HTTP_STATUS_NO_CONTENT) reason = "No Content";
    else if (status == HTTP_STATUS_BAD_REQUEST) reason = "Bad Request";
    else if (status == HTTP_STATUS_INTERNAL_ERROR) reason = "Internal Server Error";

    if (!json_body || status == HTTP_STATUS_NO_CONTENT)
    {
        char hdr[256];
        int n = snprintf(hdr, sizeof(hdr),
                         "HTTP/1.1 %d %s\r\n"
                         "Connection: keep-alive\r\n"
                         "Content-Length: 0\r\n"
                         "\r\n",
                         status, reason);
        if (n <= 0 || (size_t)n >= sizeof(hdr))
            return ERROR_NETWORK_FAILED;
        return socket_send_all(fd, hdr, (size_t)n);
    }

    size_t body_len = strlen(json_body);
    char hdr[512];
    int n = snprintf(hdr, sizeof(hdr),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Connection: keep-alive\r\n"
                     "Content-Length: %zu\r\n"
                     "\r\n",
                     status, reason, body_len);
    if (n <= 0 || (size_t)n >= sizeof(hdr))
        return ERROR_NETWORK_FAILED;
    if (socket_send_all(fd, hdr, (size_t)n) != 0)
        return ERROR_NETWORK_FAILED;
    return socket_send_all(fd, json_body, body_len);
}

static int http_recv_request(int fd, char *body_buf, size_t body_buf_sz)
{
    size_t received = 0;

    if (!body_buf || body_buf_sz == 0)
        return ERROR_INVALID_REQUEST;

    // headers
    char headers[4096];
    size_t headers_len = 0;
    if (http_read_headers(fd, headers, sizeof(headers), &headers_len) != 0)
        return ERROR_INVALID_REQUEST;

    long content_len = http_parse_content_length(headers);
    if (content_len < 0)
        return ERROR_INVALID_REQUEST;
    if ((size_t)content_len >= body_buf_sz)
        return ERROR_INVALID_REQUEST;

    if (content_len == 0)
    {
        body_buf[0] = '\0';
        return 0;
    }

    if (socket_recv_all(fd, body_buf, (size_t)content_len) != 0)
        return ERROR_NETWORK_FAILED;
    received = (size_t)content_len;
    body_buf[received] = '\0';
    return (int)received;
}

// static jsonrpc_method_t *MethodFindNode(jsonrpc_service_t *ptObj, const char *name)
// {
// 	jsonrpc_method_t *ptNode = NULL;
// 	jsonrpc_method_t *ptNextNode = NULL;


// 	ptNode = (jsonrpc_method_t *)StdListGetHeadNode(&ptObj->tMethodList);

// 	while (NULL != ptNode)
// 	{
// 		ptNextNode = (jsonrpc_method_t *)StdListGetNextNode((PT_StdNodeDef)ptNode);
// 		if (strcmp(ptNode->method_name, name) == 0)
//         {
//             return ptNode;
//         }
// 		ptNode = ptNextNode;
// 	}
// 	return NULL;
// }

static int handle_jsonrpc_request(jsonrpc_service_t *service, const char *request,
                                  int client_fd)
{
    cJSON *request_json = cJSON_Parse(request);
    T_JsonRpcMsg *pTMsg = NULL;
    int ret = 0;

    if (!request_json)
    {
        goto done;
    }

    // 验证JSON-RPC版本
    cJSON *version = cJSON_GetObjectItem(request_json, "jsonrpc");
    if (!version || !cJSON_IsString(version) ||
        strcmp(version->valuestring, JSONRPC_VERSION) != 0)
    {
        goto done;
    }

    // 获取方法名
    cJSON *method = cJSON_GetObjectItem(request_json, "method");
    if (!method || !cJSON_IsString(method))
    {
        goto done;
    }

    // 获取ID
    cJSON *id = cJSON_GetObjectItem(request_json, "id");

    // 获取参数
    cJSON *params = cJSON_GetObjectItem(request_json, "params");

    pTMsg = CommQue_GetEmpty(service->Msgque);
    if (NULL == pTMsg)
    return ERROR_INVALID_PARAMETER;
    {
        goto done;
    }

    // pTMsg->client_fd = client_fd;
    strncpy(pTMsg->strMethod, method->valuestring, sizeof(pTMsg->strMethod) - 1);
    pTMsg->strMethod[sizeof(pTMsg->strMethod) - 1] = '\0';;
    pTMsg->pcData = cJSON_Print(params);
    pTMsg->uiCallId = id->valueint;

    CommQue_PutFull(service->Msgque, pTMsg);
done:
    if (request_json)
        cJSON_Delete(request_json);
    return ret;
}

static void *handle_client_connection(void *arg)
{
    client_context_t *ctx = (client_context_t *)arg;
    int client_fd = ctx->client_fd;
    jsonrpc_service_t *service = ctx->service;
    free(ctx);

    char buffer[4096];

    while (1)
    {
        int len = http_recv_request(client_fd, buffer, sizeof(buffer));
        if (len <= 0)
        {
            break;
        }

        syslog("Service: Received request: %s\n", buffer);
        handle_jsonrpc_request(service, buffer, client_fd);
    }

    close(client_fd);
    return NULL;
}

static void *service_main_loop(void *arg)
{
    jsonrpc_service_t *service = (jsonrpc_service_t *)arg;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 创建socket
    if ((service->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        return NULL;
    }

    // 设置socket选项
    int opt = 1;
    if (setsockopt(service->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        close(service->server_fd);
        return NULL;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(service->port);

    if (bind(service->server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(service->server_fd);
        return NULL;
    }

    if (listen(service->server_fd, RPC_MAX_CONNECTIONS) < 0)
    {
        perror("listen failed");
        close(service->server_fd);
        return NULL;
    }

    syslog("JSON-RPC Service started on port %d\n", service->port);

    while (!service->bTskDone)
    {
        int client_fd = accept(service->server_fd, (struct sockaddr *)&address,
                               (socklen_t *)&addrlen);
        if (client_fd < 0)
        {
            if (!service->bTskDone)
            {
                perror("accept failed");
            }
            continue;
        }

        syslog("Service: New client connected\n");

        // 为每个客户端创建新线程
        client_context_t *ctx = malloc(sizeof(client_context_t));
        if (ctx == NULL) {
            return ERROR_OUT_OF_MEMORY;
        }
            ctx->client_fd = client_fd;
            ctx->service = service;
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, handle_client_connection, ctx);
            pthread_detach(client_thread);
        }

    close(service->server_fd);
    return NULL;
}

E_StateCode JsonRpcServerReply(HANDLE hService, UINT32 uiCallId, E_StateCode eCode, void *data)
{
    E_StateCode eCode = STATE_CODE_NO_ERROR;
    jsonrpc_service_t *service = NULL;
    cJSON *response_json = NULL;
    char *response_str = NULL;
    JSONRPC_SERVICE_CHECK_AND_SET(hService, STATE_CODE_INVALID_HANDLE);

    service = (jsonrpc_service_t *)hService;

    cJSON *result = cJSON_Parse(data);
    response_json = jsonrpc_create_response(result, cJSON_CreateNumber(uiCallId));
    cJSON_AddNumberToObject(response_json, "eCode", eCode);

    if (response_json)
    {
        response_str = cJSON_PrintUnformatted(response_json);
        if (response_str)
        {
            // 发送响应
            uint32_t len = htonl(strlen(response_str));
            if (socket_send(service->client_fd, (char *)&len, sizeof(len)) == 0)
            {
                socket_send(service->client_fd, response_str, strlen(response_str));
            }
            free(response_str);
        }
        cJSON_Delete(response_json);
    }
    cJSON_Delete(result);
    return eCode;
}

HANDLE jsonrpc_service_create(int port)
{
    jsonrpc_service_t *service = malloc(sizeof(jsonrpc_service_t));
    if (service == NULL) {
        return ERROR_OUT_OF_MEMORY;
    }
    if (!service)
        return NULL;

    service->port = port;
    service->server_fd = -1;
    service->bTskDone = FALSE;
    OSAL_MutexInit(&service->tMutex);
    StdListInit(&service->tMethodList);
    service->Msgque = CommQue_Create(50, sizeof(T_JsonRpcMsg), NULL);

    return service;
}


static E_StateCode clear_queue(CommQueID  Msgque)
{
    E_StateCode eCode = STATE_CODE_NO_ERROR;
    T_JsonRpcMsg *pTMsg = NULL;
    if(!Msgque)
    {
        return STATE_CODE_INVALID_PARAM;
    }
   
    while (1)
    {
        pTMsg = CommQue_GetFull(Msgque, OSAL_TIMEOUT_NONE);
        if(!pTMsg)
        {
            break;
        }
        
        if(pTMsg->pcData)
        {
            free(pTMsg->pcData);
            pTMsg->pcData = NULL;
        }
        CommQue_PutEmpty(Msgque, pTMsg);
    }
    CommQue_Clear(Msgque);

    return eCode;
}

E_StateCode jsonrpc_service_free(HANDLE hService)
{
    E_StateCode eCode = STATE_CODE_NO_ERROR;
    jsonrpc_service_t *ptObj = (jsonrpc_service_t *)hService;
    if (!ptObj)
        return eCode;

    eCode = jsonrpc_service_stop(ptObj);
    clear_queue(ptObj->Msgque);
    CommQue_Delete(ptObj->Msgque);
    OSAL_MutexDestroy(&ptObj->tMutex);
    free(ptObj);
    return eCode;
}

E_StateCode jsonrpc_service_start(HANDLE hservice)
{
    E_StateCode eCode = STATE_CODE_NO_ERROR;
    jsonrpc_service_t *service = NULL;
    JSONRPC_SERVICE_CHECK_AND_SET(hservice, STATE_CODE_INVALID_HANDLE);

    service = (jsonrpc_service_t *)hservice;

    OSAL_MutexLock(&service->tMutex);        
    TSK_Attrs attrs = DEFAULT_TSK_ATTR;
    service->bTskDone = FALSE;

    attrs.name = "detect";
    attrs.bFifo = TRUE;
    attrs.priority = 10;
    attrs.stackSize = 1024 * 1024;

    service->hDetectTsk = TSK_create(service_main_loop, &attrs, (void*)service);
    if (!service->hDetectTsk) 
    {
        syserr("TSK_create failed\n");
        eCode =  STATE_CODE_ALLOCATION_FAILURE;
    }

    OSAL_MutexUnlock(&service->tMutex);
    return eCode;
}

E_StateCode jsonrpc_service_stop(HANDLE hservice)
{
    E_StateCode eCode = STATE_CODE_NO_ERROR;
    jsonrpc_service_t *service = (jsonrpc_service_t *)hservice;
    JSONRPC_SERVICE_CHECK_AND_SET(hservice, STATE_CODE_INVALID_HANDLE);

    OSAL_MutexLock(&service->tMutex);
    service->bTskDone = TRUE;
    // 关闭server socket来中断accept
    if (service->server_fd != -1)
    {
        shutdown(service->server_fd, SHUT_RDWR);
        close(service->server_fd);
        service->server_fd = -1;
    }
    TSK_delete(service->hDetectTsk);

    OSAL_MutexUnlock(&service->tMutex);
    syslog("Service stopped\n");
    return eCode;
}

/**********************************************************************
 * 函数名称：JsonRpcServerAllocMsg
 * 功能描述：获取JSON-RPC消息
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
T_JsonRpcMsg* JsonRpcServerAllocMsg(HANDLE hService, UINT32 timeout)
{
    jsonrpc_service_t *service = (jsonrpc_service_t *)hService;
    T_JsonRpcMsg *pTMsg = NULL;

    JSONRPC_SERVICE_CHECK_AND_SET(hService, NULL);

    pTMsg = CommQue_GetFull(service->Msgque, timeout);
    if (NULL == pTMsg)
    return ERROR_INVALID_PARAMETER;
    {
        goto done;
    }
done:
    return pTMsg;
}

/**********************************************************************
 * 函数名称：JsonRpcServerFreeMsg
 * 功能描述： 释放JSON-RPC消息
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：    状态码
 * 其它说明：
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2025/11/10        V1.0              chengjiahao
 ***********************************************************************/
E_StateCode JsonRpcServerFreeMsg(HANDLE hService, T_JsonRpcMsg* ptMsg)
{
    jsonrpc_service_t *service = (jsonrpc_service_t *)hService;
    E_StateCode eCode = STATE_CODE_NO_ERROR;

    JSONRPC_SERVICE_CHECK_AND_SET(hService, STATE_CODE_INVALID_HANDLE);

    if (NULL != ptMsg)
    {
        if (NULL != ptMsg->pcData)
        {
            free(ptMsg->pcData);
            ptMsg->pcData = NULL;
        }
    }
    memset(ptMsg, 0, sizeof(T_JsonRpcMsg));
    CommQue_PutEmpty(service->Msgque, ptMsg);
    return eCode;
}
