#include "jsonrpc.h"
#include <stdlib.h>
#include <string.h>

/* ========== JSON-RPC 消息创建 ========== */

cJSON* jsonrpc_create_request(const char *method, cJSON *params, cJSON *id) {
    cJSON *request = cJSON_CreateObject();
    if (!request) return NULL;
    
    cJSON_AddStringToObject(request, "jsonrpc", JSONRPC_VERSION);
    cJSON_AddStringToObject(request, "method", method);
    
    if (params) {
        cJSON_AddItemToObject(request, "params", params);
    }
    
    if (id) {
        cJSON_AddItemToObject(request, "id", id);
    } else {
        cJSON_AddNullToObject(request, "id");
    }
    
    return request;
}

cJSON* jsonrpc_create_response(cJSON *result, cJSON *id) {
    cJSON *response = cJSON_CreateObject();
    if (!response) return NULL;
    
    cJSON_AddStringToObject(response, "jsonrpc", JSONRPC_VERSION);
    
    if (result) {
        cJSON_AddItemToObject(response, "result", result);
    } else {
        cJSON_AddNullToObject(response, "result");
    }
    
    if (id) {
        cJSON_AddItemToObject(response, "id", id);
    }
    
    return response;
}

cJSON* jsonrpc_create_error(int code, const char *message, cJSON *data, cJSON *id) {
    cJSON *error_obj = cJSON_CreateObject();
    cJSON *response = cJSON_CreateObject();
    
    if (!error_obj || !response) {
        if (error_obj) cJSON_Delete(error_obj);
        if (response) cJSON_Delete(response);
        return NULL;
    }
    
    cJSON_AddNumberToObject(error_obj, "code", code);
    cJSON_AddStringToObject(error_obj, "message", message);
    
    if (data) {
        cJSON_AddItemToObject(error_obj, "data", data);
    }
    
    cJSON_AddStringToObject(response, "jsonrpc", JSONRPC_VERSION);
    cJSON_AddItemToObject(response, "error", error_obj);
    
    if (id) {
        cJSON_AddItemToObject(response, "id", id);
    }
    
    return response;
}

const char* jsonrpc_error_message(int code) {
    switch (code) {
        case JSONRPC_PARSE_ERROR:
            return "Parse error";
        case JSONRPC_INVALID_REQUEST:
            return "Invalid Request";
        case JSONRPC_METHOD_NOT_FOUND:
            return "Method not found";
        case JSONRPC_INVALID_PARAMS:
            return "Invalid params";
        case JSONRPC_INTERNAL_ERROR:
            return "Internal error";
        default:
            return "Unknown error";
    }
}