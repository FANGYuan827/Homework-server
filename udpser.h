/** @file udpser.h
 *  @note 
 *  @brief 服务端UDP服务实现头文件
 *  
 *  @author 
 *  @date 2019年07月28日
 *  
 *  @note 
 *  
 *  @warning 
 */

#ifndef _UDPSER_H
#define _UDPSER_H 1

void *UDPService(void *arg);
void UDPRcvFile(int sockfd, struct sockaddr_in *pstClientAddr, socklen_t iLenClientAddr);
void UDPSendFile(int sockfd, struct sockaddr_in *pstClientAddr, socklen_t iLenClientAddr, const char *pszPath);

#endif
