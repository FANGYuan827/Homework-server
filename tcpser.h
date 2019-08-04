
/***************************************************************************************
****************************************************************************************
* FILE     : tcpser.h
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

#ifndef _TCPSER_H
#define _TCPSER_H 1

void TCPService(void *arg);
void TCPRcvFile(int iSockfd);
void TCPSendFile(int iSockfd, const char *pszPath);

#endif

/************************ (C) COPYRIGHT HIKVISION *****END OF FILE****/
