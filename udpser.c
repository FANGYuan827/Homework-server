
/***************************************************************************************
****************************************************************************************
* FILE     : udpser.c
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

#include "sha1.h"
#include "udpser.h"
#include "common.h"

/* Private define ------------------------------------------------------------*/
#define OPEN_FAIL -1
#define RECV_FAIL -1
#define WRITE_FAIL -1
#define RECVFROM_FAIL -1
#define SENDTO_FAIL -1

/*==================================================================
* Function      : UDPService   
* Description   : 服务端UDP服务线程
* Input Para    : 无
* Output Para   : 无
* Return Value  : 无
==================================================================*/
void UDPService(void *arg)
{
    struct sockaddr_in stClientAddr;
    struct sockaddr_in stServerAddr;
    char szClientAddr[INET_ADDRSTRLEN];
    socklen_t iLenClientAddr = sizeof(stClientAddr);

    int errnum = -1;  
    int rev = 0;    


    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == sockfd)
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return ;
    }

    /*初始化地址*/
    stServerAddr.sin_family = AF_INET;
    stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    stServerAddr.sin_port = htons(MCAST_PORT);

    /*绑定socket*/
    errnum = bind(sockfd, (struct sockaddr*)&stServerAddr, sizeof(stServerAddr));

    if (0 > errnum)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        return ;
    }

    /*设置回环许可*/
    int iLoop = 1;
    errnum = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &iLoop, sizeof(iLoop));

    if (errnum < 0)
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return ;
    }

    printf("UDP服务已开启...\n");

    struct ip_mreq stMreq;/*加入多播组*/
    stMreq.imr_multiaddr.s_addr = inet_addr(MCAST_ADDR);//多播地址
    stMreq.imr_interface.s_addr = htonl(INADDR_ANY); //网络接口为默认

    /*将本机加入多播组*/
    errnum = setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &stMreq, sizeof(stMreq));

    if (0 > errnum)
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return ;
    }

    printf("使能服务器可被发现...\n");
    
    /* 接收多播组的消息 */
    //while (1)
    {
        memset(&stClientAddr, 0, iLenClientAddr);
        memset(g_pszTransBuf, 0, BUFF_SIZE);

        rev = recvfrom(sockfd, g_pszTransBuf, BUFF_SIZE, 0, (struct sockaddr*)&stClientAddr, &iLenClientAddr);

        if(RECVFROM_FAIL == rev)
        {
            fprintf(stderr, "%s\n", strerror(errno));
            return ; 
        }
        
        /*打印服务器收到的多播信息*/
        //printf("buffer: %s\n", g_pszTransBuf);
        if(0 == strncmp(g_pszTransBuf, "build", 5))
        {
            memset(szClientAddr, 0, sizeof(szClientAddr)); 
            inet_ntop(AF_INET, &stClientAddr.sin_addr, szClientAddr, sizeof(szClientAddr));
            printf("client客户端[%s:%d]已找到该服务器\n", szClientAddr, ntohs(stClientAddr.sin_port));

            sendto(sockfd, "ok", sizeof("ok"),  0, (struct sockaddr *)&stClientAddr, iLenClientAddr);
        }
    }

    /*退出多播组, 退出服务器发现状态*/
    errnum = setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &stMreq, sizeof(stMreq));

    printf("退出服务器可被发现状态...\n");
 
    /* 循环接收消息 */
    while (1)
    {
        //printf("等待接收传输状态...\n");
        rev = recvfrom(sockfd, (TRANS_STATE_E*)&g_enTransState, sizeof(TRANS_STATE_E), 0, (struct sockaddr*)&stClientAddr, &iLenClientAddr);

        if(RECVFROM_FAIL == rev)
        {
            fprintf(stderr, "%s\n",strerror(errno));
            return ;
        }
        
        sendto(sockfd, "ok", sizeof("ok"),  0, (struct sockaddr *)&stClientAddr, iLenClientAddr);

        switch (g_enTransState)
        {
            case TRANS_UPLOAD:
            {
                g_enTransState = TRANS_STAND_BY;
                printf("接收客户端文件中...\n");

                rev = recvfrom(sockfd, (COM_TRANS_INFO_S*)g_pstComTransInfo, sizeof(COM_TRANS_INFO_S), 0, (struct sockaddr*)&stClientAddr, &iLenClientAddr);

                if(RECVFROM_FAIL == rev)
                {
                    fprintf(stderr, "%s\n",strerror(errno));
                    return ;
                }
                else
                {
                    /* 发送本次接收应答 */
                    sendto(sockfd, "ok", sizeof("ok"),  0, (struct sockaddr *)&stClientAddr, iLenClientAddr);
                }
                
                /*打印服务器收到的多播信息*/
                printf("SHA1: %s\n", g_pstComTransInfo->szSHA1);
                printf("FileName: %s\n", g_pstComTransInfo->szFilename);
                printf("FileSize: %d\n", g_pstComTransInfo->iFileSize);
                //printf("enTransFlag: %d\n", g_pstComTransInfo->enTransFlag);
            
                UDPRcvFile(sockfd, &stClientAddr, iLenClientAddr);
                break;
            }

            case TRANS_DOWNLOAD:
            {
                g_enTransState = TRANS_STAND_BY;
                printf("查看服务器文件中...\n");
                getcwd(g_pszPath, PATH_MAX);
                printf("服务器工作目录:%s\n", g_pszPath);

                if(SENDTO_FAIL == sendto(sockfd, g_pszPath, PATH_MAX,  0, (struct sockaddr *)&stClientAddr, iLenClientAddr))
                {
                    fprintf(stderr, "%s\n",strerror(errno));
                    return ;
                }
                else
                {
                    if(RECVFROM_FAIL == recvfrom(sockfd, g_szAckBuf, ACK_SIZE, 0, (struct sockaddr*)&stClientAddr, &iLenClientAddr))
                    {
                        fprintf(stderr, "%s\n",strerror(errno));
                        return ;
                    }
                    else
                    {
                        strncmp(g_szAckBuf, "ok", 2);
                        printf("服务器目录发送成功!\n");
                    }
                }

                //接收查看目录路径
                recvfrom(sockfd, g_pszPath, PATH_MAX, 0, (struct sockaddr*)&stClientAddr, &iLenClientAddr);
                //读取文件目录并发送
                UDPSendDirList(g_pszPath, sockfd, &stClientAddr, iLenClientAddr);
                //接收传输文件路径
                recvfrom(sockfd, g_pszPath, PATH_MAX, 0, (struct sockaddr*)&stClientAddr, &iLenClientAddr);

                /* 向客户端传输文件 */
                printf("向客户端传输文件信息中...\n");
                /* 传输之前获取文件SHA1值 */
                SHA1File(g_pszPath, g_pstComTransInfo->szSHA1);         //存储SHA1摘要
                printf("客户端待下载文件SHA1: %s\n", g_pstComTransInfo->szSHA1); 
                char *tmp = strrchr(g_pszPath, '/');
                tmp++;
                snprintf(g_pstComTransInfo->szFilename, NAME_MAX, "%s", tmp);//存储文件名
                /* 获取文件大小 */
                g_pstComTransInfo->iFileSize = GetFileSize(g_pszPath);

                /* 发送文件相关信息 */
                if(SENDTO_FAIL == sendto(sockfd, (COM_TRANS_INFO_S*)g_pstComTransInfo, sizeof(COM_TRANS_INFO_S), 0, (struct sockaddr *)&stClientAddr, iLenClientAddr)) 
                {  
                    fprintf(stderr, "%s\n",strerror(errno));
                    return ;
                }

                UDPSendFile(sockfd, &stClientAddr, iLenClientAddr, g_pszPath);

                break;
            }
            default:
            {
                break;
            }
        }
    }

    close(sockfd);
    return ;
}


/*==================================================================
* Function      : UDPRcvFile   
* Description   : 文件接收函数
* Input Para    : sockfd:   pstClientAddr:  iLenClientAddr:
* Output Para   : 无
* Return Value  : 无
==================================================================*/
void UDPRcvFile(int sockfd, struct sockaddr_in *pstClientAddr, socklen_t iLenClientAddr)
{
    fd_set readfd;    
    struct timeval sttv = {0, 0};
    int fd; 
    int TimeoutCount = 0;  
    int rev = 0;   
    int FileSize = 0;   

    fd = open(g_pstComTransInfo->szFilename, O_RDWR | O_CREAT, 0664);

    if(OPEN_FAIL == fd)
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return ;
    }

    while (1)
    {
        FD_ZERO(&readfd);
        FD_SET(sockfd, &readfd);
        sttv.tv_sec = 3;
        sttv.tv_usec = 0;
        select(sockfd+1, &readfd, NULL, NULL, &sttv);
        
        if(FD_ISSET(sockfd, &readfd))
        {
            TimeoutCount = 0;  //超时时间内收到数据重新计数

            rev = recvfrom(sockfd, (char*)g_pszTransBuf, BUFFER_SIZE, 0, (struct sockaddr*)pstClientAddr, &iLenClientAddr);

            if(RECVFROM_FAIL == rev)
            {
                close(fd);
                fprintf(stderr, "%s\n",strerror(errno));
                return ;
            }
            
            if(WRITE_FAIL == write(fd, g_pszTransBuf, rev))
            {
                close(fd);
                fprintf(stderr, "%s\n",strerror(errno));
                return ;
            }
            
            FileSize += rev;

            //发送响应
            rev = sendto(sockfd, "ok", 2, 0, (struct sockaddr*)pstClientAddr, iLenClientAddr);
            if(SENDTO_FAIL == rev)
            {
                close(fd);
                fprintf(stderr, "%s\n",strerror(errno));
                return ;
            }
            
            //文件接收计数达到文件大小则接收结束
            if(FileSize == g_pstComTransInfo->iFileSize)
            {
                break;
            }
        }
        else
        {
            printf("超时%d次,达到3次本次传输退出!\n", ++TimeoutCount);

            if(TimeoutCount == 3)
            {
                printf("网络异常，文件传输未完成!\n");
                close(fd);
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



/*==================================================================
* Function      : UDPSendFile   
* Description   : 文件发送函数
* Input Para    : sockfd:   pstClientAddr:  iLenClientAddr: p_PathName:
* Output Para   : 无
* Return Value  : 无
==================================================================*/
void UDPSendFile(int sockfd, struct sockaddr_in *pstClientAddr, socklen_t iLenClientAddr, const char *p_PathName) 
{
    if((NULL == p_PathName) || (NULL == pstClientAddr))
    {
        printf("传入参数错误！\n");
        return;
    }

    int length = 0;
    int rev = 0;                   
    int fd = open(p_PathName, O_RDONLY);    

    if (OPEN_FAIL == fd)
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return ; 
    }

    while ((length = read(fd, g_pszTransBuf, BUFFER_SIZE)) > 0)
    {
        //printf("读取%d字节消息\n", length);   //发送
        if((rev = (sendto(sockfd, g_pszTransBuf, length, 0, (struct sockaddr*)pstClientAddr, sizeof(struct sockaddr)))) < 0)          
        {            
            printf("%s发送文件失败!\n", p_PathName);
            break; 
        }

        /* 接收本次发送应答 */
        if((recvfrom(sockfd, g_szAckBuf, ACK_SIZE, 0, (struct sockaddr*)pstClientAddr, &iLenClientAddr)) < 0)          
        {            
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
    return ;
}

/************************ (C) COPYRIGHT HIKVISION *****END OF FILE****/

