/**************************************************************************
* 版    权：Copyright (c) 2022
* 文件名称：MOJsonRpc.c
* 文件标识： 
* 内容摘要：JSON-RPC消息管理
* 其它说明：
* 当前版本： 
* 作    者： 
* 完成日期：2025年11月 26日
*
* 修改记录1	：
*	修改日期：
*	版 本 号：
*	修 改 人：
*	修改内容：
**************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MOJsonRpc.h"
#include "MOJsonRpcMsg.h"
#include "common/jsonrpc/jsonrpcService.h"
/***********************************************************
 *						常量定义		                       		*
 **********************************************************/
#define DEBUG_PORT	9103

/***********************************************************
 *				文件内部使用的宏                      *
 **********************************************************/
 
 /***********************************************************
 *			文件内部使用的数据类型 	*
 **********************************************************/

typedef struct
{
	HANDLE	hServer;
}T_MOJsonRpc;

/***********************************************************
 *						全局变量						*
 **********************************************************/


/***********************************************************
 *						本地变量						*
 **********************************************************/
static T_MOJsonRpc	*g_sptMOJsonRpc = NULL;


/***********************************************************
 * 						本地函数						*
 **********************************************************/

/***********************************************************
 * 						全局函数						*
 **********************************************************/ 
E_StateCode MOJsonRpcInit()
{
	E_StateCode	eCode = STATE_CODE_NO_ERROR;
	T_MOJsonRpc	*ptObj = NULL;

	LJ_SAFE_MALLOC(ptObj, sizeof(T_MOJsonRpc));

	if(NULL == ptObj)
	{
		eCode = STATE_CODE_ALLOCATION_FAILURE;
		goto cleanup;
	}

	g_sptMOJsonRpc = ptObj;
	
	ptObj->hServer = jsonrpc_service_create(DEBUG_PORT, NULL);
	if (NULL == ptObj->hServer)
	{
		eCode = STATE_CODE_SOCKET_BIND_FAILURE;
		goto cleanup;
	}

	MOJsonRpcMsgInit(ptObj->hServer);

	eCode = jsonrpc_service_start(ptObj->hServer);
	if (!STATE_OK(eCode))
	{
		goto cleanup;
	}

	// ptObj->ptThis = ptModule;
	
cleanup:
	if (!STATE_OK(eCode))
	{
		MOJsonRpcDestroy();
	}
	return eCode;
}

E_StateCode MOJsonRpcDestroy()
{
	E_StateCode	eCode = STATE_CODE_NO_ERROR;
	T_MOJsonRpc	*ptObj = NULL;

	ptObj = g_sptMOJsonRpc;
	if (NULL == ptObj)
	{
		return STATE_CODE_INVALID_HANDLE;
	}

	MOJsonRpcMsgDestroy();
	jsonrpc_service_stop(ptObj->hServer);
	if (NULL != ptObj->hServer)
	{
		jsonrpc_service_free(ptObj->hServer);
	}
	
	return eCode;
}

// E_StateCode MOJsonRpcResponseWindowBuffer(void *ptBuffer)
// {
// 	E_StateCode	eCode  = STATE_CODE_NO_ERROR;
// 	INT8		*pcSend = NULL;
// 	T_MOJsonRpc	*ptObj = NULL;

// 	ptObj = g_sptMOJsonRpc;
// 	if (NULL == ptObj)
// 	{
// 		return STATE_CODE_ALLOCATION_FAILURE;
// 	}
	
// 	if (NULL == ptObj->ptThis)
// 	{
// 		return STATE_CODE_INVALID_HANDLE;
// 	}

// 	eCode = MediaMakeListElement("bufferInfo", ptBuffer, &pcSend);
// 	if (!STATE_OK(eCode))
//  	{
//  		SysErr("MOJsonRpcServerResponseBuffer failed, eCode = %d\n", eCode);
// 		return eCode;
//  	}

// 	if (g_bFullDebug)
// 	{
// 		dbprintf("JsonRpcServerNotify %s\n", pcSend);
// 	}
	
// 	JsonRpcServerNotify(ptObj->hSyncServer, "decBuffer", pcSend);

// 	if (NULL != pcSend)
// 	{
// 		free(pcSend);
// 	}
// 	return eCode;
// }

// void MOJsonRpcPoll()
// {
// 	E_StateCode		eCode  = STATE_CODE_NO_ERROR;
// 	T_JsonRpcMsg	*ptMsg = NULL;
// 	T_MOJsonRpc		*ptObj = NULL;

// 	ptObj = g_sptMOJsonRpc;
// 	if (NULL == ptObj)
// 	{
// 		return;
// 	}

// 	MOJsonRpcMsgPollTrace(ptObj->ptThis->pPrivate);

// 	MOPollWindowIsReady(ptObj->ptThis->pPrivate);

// 	for (;;)
// 	{
// 		ptMsg = JsonRpcServerAllocMsg(ptObj->hWindowServer, OSAL_TIMEOUT_NONE);
// 		if (NULL == ptMsg)
// 		{
// 			break;
// 		}

// 		if (g_bFullDebug)
// 		{
// 			dbprintf("REQ::%s\n", ptMsg->strMethod);
// 			dbprintf("RES::%u\n", ptMsg->uiCallId);
// 			dbprintf("JSON::%s\n", (INT8 *)ptMsg->pcData);
// 		}

// 		eCode = MOJsonRpcMsgProcess(ptObj->ptThis->pPrivate, ptMsg);
// 		if (!STATE_OK(eCode))
// 		{
// 			SysErr("MOJsonRpcMsgProcess failed, eCode = %d, %s %s\n", eCode, ptMsg->strMethod, ptMsg->pcData);
// 		}
// 		JsonRpcServerFreeMsg(ptObj->hWindowServer, ptMsg);
// 	}
// }

