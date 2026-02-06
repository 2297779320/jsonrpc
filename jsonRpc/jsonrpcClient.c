#include "jsonrpcClient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <pthread.h>
#include <errno.h>

/***********************************************************
 *              文件内部使用的宏                      *
 **********************************************************/
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
typedef struct  __jsonrpc_client_t
{
    char *host;
    int port;
    char *path; // HTTP path, default: /rpc
    int sock_fd;
    int connected;
    pthread_mutex_t lock;
    int next_request_id;
} jsonrpc_client_t;

static int socket_send_all(int fd, const void *buf, size_t len)
{
    size_t sent = 0;
    const char *p = (const char *)buf;
    while (sent < len)
    {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0)
        {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int socket_recv_all(int fd, void *buf, size_t len)
{
    size_t recvd = 0;
    char *p = (char *)buf;
    while (recvd < len)
    {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n <= 0)
        {
            return -1;
        }
        recvd += (size_t)n;
    }
    return 0;
}

static int client_connect(HANDLE hclient)
{
    jsonrpc_client_t *ptObj = (jsonrpc_client_t *)hclient;
    if (ptObj->connected)
        return 0;

    struct sockaddr_in serv_addr;

    if ((ptObj->sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(ptObj->port);

    if (inet_pton(AF_INET, ptObj->host, &serv_addr.sin_addr) <= 0)
    {
        perror("invalid address");
        close(ptObj->sock_fd);
        return -1;
    }

    if (connect(ptObj->sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connection failed");
        close(ptObj->sock_fd);
        return -1;
    }

    ptObj->connected = 1;
    return 0;
}

static int http_read_headers(int fd, char *header_buf, size_t header_buf_sz, size_t *out_header_len)
{
    // Read byte-by-byte until "\r\n\r\n" is found (simple and robust enough here)
    size_t used = 0;
    if (!header_buf || header_buf_sz < 4)
        return -1;

    while (used + 1 < header_buf_sz)
    {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0)
            return -1;
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
    return -1;
}

static long http_parse_content_length(const char *headers)
{
    // Very small parser: find "Content-Length:" case-insensitively and parse decimal
    if (!headers)
        return -1;

    const char *p = headers;
    while (*p)
    {
        // find line start
        const char *line = p;
        const char *eol = strstr(line, "\r\n");
        if (!eol)
            break;

        // Move p to next line
        p = eol + 2;

        // Compare prefix case-insensitively
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
                return -1;
            return val;
        }
    }

    return -1;
}

static int client_send_request(jsonrpc_client_t *client, const char *request_json,
                               char *response_json, size_t max_response)
{
    if (!client->connected && client_connect(client) != 0)
    {
        return -1;
    }

    if (!request_json || !response_json || max_response == 0)
        return -1;

    // Build HTTP/1.1 POST request
    size_t body_len = strlen(request_json);
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
                              "POST %s HTTP/1.1\r\n"
                              "Host: %s:%d\r\n"
                              "Content-Type: application/json\r\n"
                              "Accept: application/json\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: keep-alive\r\n"
                              "\r\n",
                              client->path ? client->path : "/rpc",
                              client->host, client->port, body_len);
    if (header_len <= 0 || (size_t)header_len >= sizeof(header))
        return -1;

    if (socket_send_all(client->sock_fd, header, (size_t)header_len) != 0)
    {
        client->connected = 0;
        return -1;
    }

    if (socket_send_all(client->sock_fd, request_json, body_len) != 0)
    {
        client->connected = 0;
        return -1;
    }

    // Read HTTP response headers
    char resp_headers[4096];
    size_t resp_headers_len = 0;
    if (http_read_headers(client->sock_fd, resp_headers, sizeof(resp_headers), &resp_headers_len) != 0)
    {
        client->connected = 0;
        return -1;
    }

    long content_len = http_parse_content_length(resp_headers);
    if (content_len < 0)
        return -1;

    if (content_len == 0)
    {
        // Notification style response (e.g., 204). Treat as empty JSON.
        if (max_response > 0)
            response_json[0] = '\0';
        return 0;
    }

    if ((size_t)content_len >= max_response)
        return -1;

    if (socket_recv_all(client->sock_fd, response_json, (size_t)content_len) != 0)
    {
        client->connected = 0;
        return -1;
    }

    response_json[(size_t)content_len] = '\0';
    return (int)content_len;
}

HANDLE jsonrpc_client_create(const char *host, int port)
{
    jsonrpc_client_t *client = malloc(sizeof(jsonrpc_client_t));
    if (!client)
        return NULL;

    client->host = strdup(host);
    client->port = port;
    client->path = strdup("/rpc");
    client->sock_fd = -1;
    client->connected = 0;
    pthread_mutex_init(&client->lock, NULL);
    client->next_request_id = 1;

    return client;
}

E_StateCode jsonrpc_client_set_path(HANDLE hclient, const char *path)
{
    jsonrpc_client_t *client = (jsonrpc_client_t *)hclient;
    if (!client || !path || path[0] != '/')
        return STATE_CODE_INVALID_PARAM;

    char *new_path = strdup(path);
    if (!new_path)
        return STATE_CODE_ALLOCATION_FAILURE;

    pthread_mutex_lock(&client->lock);
    free(client->path);
    client->path = new_path;
    pthread_mutex_unlock(&client->lock);

    return STATE_CODE_NO_ERROR;
}

void jsonrpc_client_free(HANDLE hclient)
{
    jsonrpc_client_t *client = (jsonrpc_client_t *)hclient;
    if (!client)
        return;

    if (client->sock_fd != -1)
    {
        close(client->sock_fd);
    }

    pthread_mutex_destroy(&client->lock);
    free(client->host);
    free(client->path);
    free(client);
}

E_StateCode jsonrpc_client_call(HANDLE hclient, const char *method, 
                          cJSON *params, cJSON **result)
{
    E_StateCode eCode = STATE_CODE_NO_ERROR;
    jsonrpc_client_t *client = (jsonrpc_client_t *)hclient;

    if (!client || !method)
    {
        return STATE_CODE_INVALID_PARAM;
    }

    int request_id = 0;
    pthread_mutex_lock(&client->lock);
    request_id = client->next_request_id++;
    pthread_mutex_unlock(&client->lock);

    // 创建请求
    // 重要：jsonrpc_create_request 会接管 params 的所有权，这里做一次副本，避免调用者后续释放导致 double-free
    cJSON *params_dup = params ? cJSON_Duplicate(params, 1) : NULL;
    cJSON *request = jsonrpc_create_request(method, params_dup, cJSON_CreateNumber(request_id));
    if (!request)
    {
        if (params_dup)
            cJSON_Delete(params_dup);
        return STATE_CODE_FAILED_TO_PROCEED_COMMAND;
    }

    char *request_str = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);

    if (!request_str)
    {
        return STATE_CODE_INVALID_PARAM;
    }

    printf("Client: Sending request: %s\n", request_str);

    // 发送请求并接收响应
    char response_buf[4096];
    int response_len = client_send_request(client, request_str, response_buf, sizeof(response_buf));
    free(request_str);

    if (response_len <= 0)
    {
        return STATE_CODE_INVALID_PARAM;
    }

    printf("Client: Received response: %s\n", response_buf);

    // 解析响应
    cJSON *response = cJSON_Parse(response_buf);
    if (!response)
    {
        return STATE_CODE_INVALID_PARAM;
    }

    // 检查错误
    cJSON *error = cJSON_GetObjectItem(response, "error");
    if (error)
    {
        cJSON *code = cJSON_GetObjectItem(error, "code");
        eCode = code ? code->valueint : STATE_CODE_FAILED_TO_PROCEED_COMMAND;
        cJSON_Delete(response);
        return eCode;
    }

    // 获取结果
    cJSON *temp = cJSON_GetObjectItem(response, "result");
    if (temp)
    {
        *result = cJSON_Duplicate(temp, 1);
    }

    cJSON_Delete(response);

    return eCode;
}