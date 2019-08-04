
/***************************************************************************************
****************************************************************************************
* FILE     : tcpser.c
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "tcpser.h"
#include "sha1.h"
#include "common.h"


/* Private define ------------------------------------------------------------*/
#define CONNECT_FAIL -1
#define SEND_FAIL -1
#define OPEN_FAIL -1
#define RECV_FAIL -1
#define WRITE_FAIL -1


/*==================================================================
* Function      : TCPService   
* Description   : 服务端TCP服务线程
* Input Para    : 无
* Output Para   : 无
* Return Value  : 无
==================================================================*/
void TCPService(void *arg)
{
    int sockfd = *(int *)arg;  //得到线程参数  
    
    int rev = 0;   
 
    /* 循环接收消息 */
    while (1)
    {

        rev = recv(sockfd, (TRANS_STATE_E*)&g_enTransState, sizeof(TRANS_STATE_E), 0);

        if(RECV_FAIL == rev)
        {
            fprintf(stderr, "%s\n",strerror(errno));
            return;
        }
        
        send(sockfd, "ok", sizeof("ok"),  0);

        switch (g_enTransState)
        {

            case TRANS_UPLOAD:
            {
                g_enTransState = TRANS_STAND_BY;
                printf("客户端上传文件中...\n");
                rev = recv(sockfd, (COM_TRANS_INFO_S*)g_pstComTransInfo, sizeof(COM_TRANS_INFO_S), 0);

                if(RECV_FAIL == rev)
                {
                    fprintf(stderr, "%s\n",strerror(errno));
                    return;
                }
                else
                {
                    /* 发送本次接收应答 */
                    send(sockfd, "ok", sizeof("ok"),  0);
                }
                
                /*打印服务器收到的文件信息*/
                printf("SHA1: %s\n", g_pstComTransInfo->szSHA1);
                printf("FileName: %s\n", g_pstComTransInfo->szFilename);
                printf("FileSize: %d\n", g_pstComTransInfo->iFileSize);
                //printf("enTransFlag: %d\n", g_pstComTransInfo->enTransFlag);
            
                TCPRcvFile(sockfd);
                break;
            }

            case TRANS_DOWNLOAD:
            {
                g_enTransState = TRANS_STAND_BY;
                printf("查看服务器文件中...\n");
                getcwd(g_pszPath, PATH_MAX);
                printf("服务器工作目录:%s\n", g_pszPath);

                if(SEND_FAIL == send(sockfd, g_pszPath, PATH_MAX, 0))
                {
                    fprintf(stderr, "%s\n",strerror(errno));
                    return;
                }

                else
                {

                    if(RECV_FAIL == recv(sockfd, g_szAckBuf, ACK_SIZE, 0))
                    {
                        fprintf(stderr, "%s\n",strerror(errno));
                        return;
                    }

                    else
                    {
                        strncmp(g_szAckBuf, "ok", 2);
                        printf("服务器目录发送成功!\n");
                    }

                }

                /* 接收查看目录路径 */
                recv(sockfd, g_pszPath, PATH_MAX, 0);

                /* 读取文件目录并发送 */
                TCPSendDirList(g_pszPath, sockfd);

                /* 接收传输文件路径 */
                recv(sockfd, g_pszPath, PATH_MAX, 0);

                /* 向客户端传输文件 */
                printf("向客户端传输文件信息中...\n");
                
                /* 传输之前获取文件SHA1值 */
                SHA1File(g_pszPath, g_pstComTransInfo->szSHA1);         //存储SHA1摘要
                printf("客户端待下载文件SHA1: %s\n", g_pstComTransInfo->szSHA1); 
                char *tmp = strrchr(g_pszPath, '/');
                tmp++;
                snprintf(g_pstComTransInfo->szFilename, NAME_MAX, "%s", tmp);
                
                /* 获取文件大小 */
                g_pstComTransInfo->iFileSize = GetFileSize(g_pszPath);

                /* 发送文件相关信息 */
                if(SEND_FAIL == send(sockfd, (COM_TRANS_INFO_S*)g_pstComTransInfo, sizeof(COM_TRANS_INFO_S), 0)) 
                {  
                    fprintf(stderr, "%s\n",strerror(errno));
                    return;
                }

                TCPSendFile(sockfd, g_pszPath);

                break;
            }

            default:
            {
                break;
            }

        }
    }

    close(sockfd);

}

 /**@fn 
 *  @brief  文件接收函数
 *  @param c 参数描述
 *  @param n 参数描述
 *  @return 成功返回字符串地址，失败返回空
 */

/*==================================================================
* Function      : TCPRcvFile   
* Description   : 文件接收函数
* Input Para    : sockfd：
* Output Para   : 无
* Return Value  : 无
==================================================================*/
void TCPRcvFile(int sockfd)
{
    fd_set readfd;      
    int fd;            
    int rev = 0;   
    int FileSize = 0;   

    fd = open(g_pstComTransInfo->szFilename, O_RDWR | O_CREAT, 0664);

    if(OPEN_FAIL == fd)
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return;
    }

    while (1)
    {
        FD_ZERO(&readfd);
        FD_SET(sockfd, &readfd);
        select(sockfd+1, &readfd, NULL, NULL, NULL);
        
        if(FD_ISSET(sockfd, &readfd))
        {
            rev = recv(sockfd, (char*)g_pszTransBuf, BUFFER_SIZE, 0);
            if(RECV_FAIL == rev)
            {
                close(fd);
                fprintf(stderr, "%s\n",strerror(errno));
                return;
            }
            
            if(WRITE_FAIL == write(fd, g_pszTransBuf, rev))
            {
                close(fd);
                fprintf(stderr, "%s\n",strerror(errno));
                return;
            }

            FileSize += rev;
            
            //发送响应
            rev = send(sockfd, "ok3", 3, 0);

            if(SEND_FAIL == rev)
            {
                close(fd);
                fprintf(stderr, "%s\n",strerror(errno));
                return;
            }

            //文件接收计数达到文件大小则接收结束
            if(FileSize == g_pstComTransInfo->iFileSize)
            {
                break;
            }
        }
    }
    
    close(fd);

    SHA1File(g_pstComTransInfo->szFilename, g_pszSha1Digest);
    printf("客户端上传文件SHA1: %s\n", g_pszSha1Digest);

    if(0 == strncmp(g_pstComTransInfo->szSHA1, g_pszSha1Digest, 40))
    {
        printf("SHA1相同，文件传输正常\n");
    }

}

 /**@fn 
 *  @brief  文件发送函数
 *  @param c 参数描述
 *  @param n 参数描述
 *  @return 成功返回字符串地址，失败返回空
 */

/*==================================================================
* Function      : TCPSendFile   
* Description   : 文件发送函数
* Input Para    : sockfd:    p_PathName:
* Output Para   : 无
* Return Value  : 无
==================================================================*/
void TCPSendFile(int sockfd, const char *p_PathName)
{
    if (NULL == p_PathName)
    {
        printf("传入参数错误！\n");
        return;
    }

    int length = 0;
    int rev = 0; 

    int fd = open(p_PathName, O_RDONLY);    // ifd

    if (OPEN_FAIL == fd)
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return;
    }

    while ((length = read(fd, g_pszTransBuf, BUFFER_SIZE)) > 0)
    {

        if((rev = send(sockfd, g_pszTransBuf, length, 0)) < 0)
        {
            close(fd);
            printf("%s发送文件失败!\n", p_PathName);
            break; 
        }

        /* 接收本次发送应答 */
        if(recv(sockfd, g_szAckBuf, ACK_SIZE, 0) < 0)
        {
            close(fd);
            fprintf(stderr, "%s\n", strerror(errno));
            break;
        }

        if(0 == strncmp(g_szAckBuf, "ok", 2))
        {
            //printf("发送%d字节消息成功\n", rev);
        }
    }
     
    printf("%s发送文件成功!\n", p_PathName); 

    close(fd);

}

/************************ (C) COPYRIGHT HIKVISION *****END OF FILE****/

