
/***************************************************************************************
****************************************************************************************
* FILE     : udpser.h
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

#ifndef _UDPSER_H
#define _UDPSER_H 1

void UDPService(void *arg);
void UDPRcvFile(int sockfd, struct sockaddr_in *pstClientAddr, socklen_t iLenClientAddr);
void UDPSendFile(int sockfd, struct sockaddr_in *pstClientAddr, socklen_t iLenClientAddr, const char *p_PathName);

#endif

/************************ (C) COPYRIGHT HIKVISION *****END OF FILE****/
