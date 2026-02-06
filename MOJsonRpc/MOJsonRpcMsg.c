#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MOJsonRpcMsg.h"
#include "media.h"
#include "MO/MO.h"
#include "MO/MOChannel.h"
#include "common/JsonParse/JsonParse.h"
#include "common/jsonrpc/jsonrpcService.h"
/***********************************************************
 *						常量定义		                       		*
 **********************************************************/

/***********************************************************
 *				文件内部使用的宏                      *
 **********************************************************/

/***********************************************************
 *			文件内部使用的数据类型 	*
 **********************************************************/

typedef struct
{
	HANDLE hRpcServer;
} T_MOJsonRpcMsgTrace;

typedef struct
{
	const char *method_name;
	E_StateCode (*proc)(void *pPrivate, void *ptMsgInfo);
} T_MOJsonRpcReg;

typedef struct __T_MOJsonRpcRegNode
{
	struct __T_MOJsonRpcRegNode *next;
	T_MOJsonRpcReg reg;
} T_MOJsonRpcRegNode;

/***********************************************************
 *						全局变量						*
 **********************************************************/

/***********************************************************
 *						本地变量						*
 **********************************************************/
static T_MOJsonRpcMsgTrace *g_sptMsgTrace = NULL;
static T_MOJsonRpcRegNode *g_sptRegList = NULL;

static cJSON *MOJsonRpc_Handler(cJSON *params, cJSON *id, void *user_data)
{
	T_MOJsonRpcReg *ptReg = (T_MOJsonRpcReg *)user_data;
	E_StateCode eCode = STATE_CODE_NO_ERROR;

	// ??????????????? JSON ??? body
	char *pcBody = NULL;
	if (params)
	{
		pcBody = cJSON_PrintUnformatted(params);
	}
	else
	{
		pcBody = strdup("null");
	}

	T_JsonRpcMsg tJsonMsg;
	memset(&tJsonMsg, 0x0, sizeof(tJsonMsg));
	strncpy(tJsonMsg.strMethod, ptReg ? ptReg->method_name : "", sizeof(tJsonMsg.strMethod) - 1);
	tJsonMsg.pcData = pcBody;
	tJsonMsg.uiCallId = 0;
	if (id && cJSON_IsNumber(id))
	{
		tJsonMsg.uiCallId = (UINT32)id->valuedouble;
	}

	// ??????????
	eCode = MOJsonRpcMsgProcess(NULL, &tJsonMsg);

	if (pcBody)
		free(pcBody);

	// ??????????? { "eCode": <int> }
	cJSON *result = cJSON_CreateObject();
	if (!result)
		return NULL;
	cJSON_AddNumberToObject(result, "eCode", (int)eCode);
	return result;
}

/***********************************************************
 * 						本地函数						*
 **********************************************************/
static E_StateCode MOJsonMsgChangePid(void *pPrivate, void *ptMsgInfo)
{
	E_StateCode eCode = STATE_CODE_NO_ERROR;
	T_MO *ptPrivate = (T_MO *)pPrivate;
	T_MsgV2 *ptMsg = (T_MsgV2 *)ptMsgInfo;
	HANDLE ptChItem = NULL;
	UINT16 uivideopid = 0;

	if ((NULL == ptMsg) || (NULL == ptPrivate))
	{
		return STATE_CODE_INVALID_HANDLE;
	}

	eCode =	MediaParseListElement("moch", ptMsg->pcBody, &uivideopid);
	if (!STATE_OK(eCode))
	{
		syserr("MOJsonMsgParseWindowCtrlParam failed, eCode = %d, %s %s\n", eCode, ptMsg->strMethod, ptMsg->pcBody);
		return eCode;
	}

	ptChItem = ptPrivate->hMainCh;

	// eCode = MOChFreezeChannel(ptChItem, &tSetup);
	// if (!STATE_OK(eCode))
	// {
	// 	return eCode;
	// }

	return eCode;
}

/* 消息处理表*/
static T_MsgProcV2 g_satMOWindowTable[] =
	{
		{"ChangePid", MOJsonMsgChangePid},
		{NULL, NULL}
	};

/***********************************************************
 * 						全局函数						*
 **********************************************************/

E_StateCode MOJsonRpcMsgInit(HANDLE hRpcServer)
{
	E_StateCode eCode = STATE_CODE_NO_ERROR;
	T_MOJsonRpcMsgTrace *ptObj = NULL;

	ptObj = (T_MOJsonRpcMsgTrace *)malloc(sizeof(T_MOJsonRpcMsgTrace));
	if (NULL == ptObj)
	{
		return STATE_CODE_ALLOCATION_FAILURE;
	}

	memset(ptObj, 0x0, sizeof(T_MOJsonRpcMsgTrace));

	ptObj->hRpcServer = hRpcServer;

	g_sptMsgTrace = ptObj;

	// ???? JSON-RPC ????????????????????????
	// ? RPC ??????????-???
	{
		T_MsgProcV2 *ptMsgProc = g_satMOWindowTable;
		while (ptMsgProc && ptMsgProc->pcMsg)
		{
			T_MOJsonRpcRegNode *ptNode = (T_MOJsonRpcRegNode *)malloc(sizeof(T_MOJsonRpcRegNode));
			if (!ptNode)
			{
				eCode = STATE_CODE_ALLOCATION_FAILURE;
				break;
			}
			memset(ptNode, 0x0, sizeof(*ptNode));
			ptNode->reg.method_name = ptMsgProc->pcMsg;
			ptNode->reg.proc = ptMsgProc->pfMsgProcFxn;
			ptNode->next = g_sptRegList;
			g_sptRegList = ptNode;

			(void)jsonrpc_service_register_method(hRpcServer, ptMsgProc->pcMsg, MOJsonRpc_Handler, &ptNode->reg);
			ptMsgProc++;
		}
	}

	return eCode;
}

E_StateCode MOJsonRpcMsgDestroy()
{
	E_StateCode eCode = STATE_CODE_NO_ERROR;
	T_MOJsonRpcMsgTrace *ptObj = g_sptMsgTrace;

	if (NULL == ptObj)
	{
		return STATE_CODE_INVALID_HANDLE;
	}

	free(ptObj);
	g_sptMsgTrace = NULL;

	// ??????
	while (g_sptRegList)
	{
		T_MOJsonRpcRegNode *ptNext = g_sptRegList->next;
		free(g_sptRegList);
		g_sptRegList = ptNext;
	}

	return eCode;
}

E_StateCode MOJsonRpcMsgProcess(void *pPrivate, T_JsonRpcMsg *ptJsonRpcMsg)
{
	E_StateCode eCode = STATE_CODE_NO_ERROR;
	INT8 *pcFind = NULL;
	UINT32 uiCmpLen = 0;
	T_MsgV2 tMsg;
	T_MsgProcV2 *ptMsgProc = NULL;
	// BOOL bDelayRes = FALSE;

	if (NULL == g_sptMsgTrace)
	{
		return STATE_CODE_INVALID_HANDLE;
	}

	memset(&tMsg, 0x0, sizeof(tMsg));

	pcFind = strrchr(ptJsonRpcMsg->strMethod, '.');
	if (NULL != pcFind)
	{
		uiCmpLen = pcFind - ptJsonRpcMsg->strMethod;
	}
	else
	{
		uiCmpLen = strlen(ptJsonRpcMsg->strMethod);
	}

	ptMsgProc = g_satMOWindowTable;
	while (NULL != ptMsgProc->pcMsg)
	{
		if (0 == strncmp(ptMsgProc->pcMsg, ptJsonRpcMsg->strMethod, uiCmpLen))
		{
			break;
		}

		ptMsgProc++;
	}

	if (NULL == ptMsgProc->pfMsgProcFxn)
	{
		if (NULL == ptMsgProc->pcMsg)
		{
			eCode = STATE_CODE_INVALID_COMMAND;
		}
	}
	else
	{
		strcpy(tMsg.strMethod, ptJsonRpcMsg->strMethod);
		tMsg.pcBody = ptJsonRpcMsg->pcData;
		tMsg.uiCallId = ptJsonRpcMsg->uiCallId;
		eCode = ptMsgProc->pfMsgProcFxn(pPrivate, &tMsg);
	}

	// if (!bDelayRes)
	// {
	// 	JsonRpcServerReply(g_sptMsgTrace->hRpcServer, ptJsonRpcMsg->uiCallId, eCode, NULL);
	// }
	return eCode;
}