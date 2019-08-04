
/***************************************************************************************
****************************************************************************************
* FILE     : server.c
* Description  : 
*            
* Copyright (c) 2019 by Hikvision. All Rights Reserved.
* 
* History:
* Version      Name        Date                Description
   0.1         fangyuan9   2019/08/02          Initial Version 1.0.0
   
****************************************************************************************
****************************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

#include "common.h"
#include "sha1.h"
#include "udpser.h"
#include "tcpser.h"


/*==================================================================
* Function      : GlobalMemMalloc   
* Description   : 
* Input Para    : 无
* Output Para   : 无
* Return Value  : 无
==================================================================*/
void GlobalMemMalloc(void)
{
    //发送或接收缓存
    g_pszTransBuf = (char*)malloc(BUFFER_SIZE);

    if(NULL == g_pszTransBuf)
    {
        printf("内存申请失败！\n");
        return;
    }

    /* 文件信息结构 */
    g_pstComTransInfo = (COM_TRANS_INFO_S*)malloc(sizeof(COM_TRANS_INFO_S));

    if (NULL == g_pstComTransInfo)
    {
        printf("内存申请失败\n");
        return;
    }

    g_pszPath = (char*)malloc(PATH_MAX);
    if (NULL == g_pszPath)
    {
        printf("内存申请失败\n");
        return;
    }

    g_pszSha1Digest = (char*)malloc(COM_SHA1DIGEST_LEN);
    if (NULL == g_pszSha1Digest)
    {
        printf("内存申请失败\n");
        return;
    }

    /* 全局变量内存初始化 */
    memset(g_pszTransBuf, 0, BUFFER_SIZE);
    memset((void*)g_pstComTransInfo, 0, sizeof(COM_TRANS_INFO_S));
    memset(g_pszPath, 0, PATH_MAX);
    memset(g_pszSha1Digest, 0, COM_SHA1DIGEST_LEN);
}



/*==================================================================
* Function      : GlobalMemFree   
* Description   : 释放内存资源
* Input Para    : 无
* Output Para   : 无
* Return Value  : 无
==================================================================*/
void GlobalMemFree(char *g_pszTransBuf, COM_TRANS_INFO_S *g_pstComTransInfo, char *g_pszPath, char *g_pszSha1Digest)
{

    if(g_pszTransBuf != NULL)
    {
        free(g_pszTransBuf);
        g_pstComTransInfo = NULL;
    }


    if(g_pstComTransInfo != NULL)
    {
        free(g_pstComTransInfo);
        g_pstComTransInfo = NULL;
    }
 
    if(g_pszPath != NULL)
    {
        free(g_pszPath);
        g_pszPath = NULL;
    }

    if(g_pszSha1Digest != NULL)
    {
        free(g_pszSha1Digest);
        g_pszSha1Digest = NULL;
    }

}


/*==================================================================
* Function      : main   
* Description   : 主函数
* Input Para    : 
* Output Para   : 无
* Return Value  : 无
==================================================================*/
int main(int argc, char *argv[])
{
    struct sockaddr_in stClientAddr = {0};
    struct sockaddr_in stServerAddr = {0};
    char szClientAddr[INET_ADDRSTRLEN] = {0};

    pthread_t UDPtid;   //UDP线程标识
    pthread_t TCPtid;   //TCP线程标识
    int clientsockfd;  
    int serversockfd; 
    int iCliAddrLen = sizeof(stClientAddr);


    /* 全局资源申请 */
    GlobalMemMalloc();

    /*  //0804:直接在定义的时候初始化为0
    memset(&stClientAddr, 0, sizeof(stClientAddr));
    memset(&stServerAddr, 0, sizeof(stServerAddr));
    */

    getcwd(g_pszPath, PATH_MAX);    //获取当前工作目录
    //printf("Working Path: %s\n", g_pszPath);



    /* 创建UDP服务线程 */
    if(-1 == pthread_create(&UDPtid, NULL, UDPService, NULL))
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return -1;
    }

    pthread_detach(UDPtid);   //分离线程


    /* TCP服务 */
    stServerAddr.sin_family = AF_INET;
    stServerAddr.sin_addr.s_addr = INADDR_ANY;
    stServerAddr.sin_port = htons(TCP_PORT);

    if(-1 == (serversockfd = socket(AF_INET, SOCK_STREAM, 0)))
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return -1;
    }

    #if 0
    int iOptVal = 1;
    if (-1 == setsockopt(serversockfd, SOL_SOCKET, SO_REUSEADDR, &iOptVal, sizeof(iOptVal))) 
    {
        fprintf(stderr, "%s\n", strerror(errno));
        close(serversockfd);
        return;
    }
    #endif
    
    if(-1 == bind(serversockfd, (struct sockaddr*) &stServerAddr, sizeof(stServerAddr)))
    {
        fprintf(stderr, "%s\n", strerror(errno));
        return -1;
    }

    if(-1 == listen(serversockfd, 16))
    {
        fprintf(stderr, "%s\n", strerror(errno));
        return -1;
    }

    printf("TCP服务已开启...\n");
    
    while (1)
    {

        if(-1 == (clientsockfd = accept(serversockfd, (struct sockaddr *) &stClientAddr, &iCliAddrLen)))
        {
            fprintf(stderr, "%s\n", strerror(errno));
            return -1;
        }

        inet_ntop(AF_INET, &stClientAddr.sin_addr, szClientAddr, sizeof(szClientAddr));
        printf("TCP客户端[%s:%d]已建立连接...\n", szClientAddr, ntohs(stClientAddr.sin_port));

        /* 创建线程为其服务 */
        if(-1 == pthread_create(&TCPtid, NULL, TCPService, (void *)&clientsockfd))
        {
            fprintf(stderr, "%s\n",strerror(errno));
            return -1;
        }

        pthread_detach(TCPtid);   //线程分离

        sleep(1);
        continue;
    }
    
    /* 内存资源释放 */
    GlobalMemFree(g_pszTransBuf, g_pstComTransInfo, g_pszPath, g_pszSha1Digest);

    printf("End\n");

    return 0;
}

/************************ (C) COPYRIGHT HIKVISION *****END OF FILE****/

