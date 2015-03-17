#include "udpsocket.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

namespace paxoslease
{

UdpSocket::UdpSocket(int port):port_(port)
{
    assert(port_ > 0);

    memset((void *)&broadcast_addr_, 0, sizeof(broadcast_addr_));
    broadcast_addr_.sin_family = AF_INET;
    broadcast_addr_.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr_.sin_port = htons(port_);

    udp_fd_ = -1;
}

int UdpSocket::Open()
{
    if(udp_fd_ > 0) {
        return -1;
    }

    udp_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_fd_ < 0) {
        perror("socket:"); 
        return -1;
    }

    int enabled = 1;
    int ret = setsockopt(udp_fd_, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled));
    if(ret < 0) {
        perror("setsockopt:");
        Close(); 
        return -1;
    }

    int reuse_addr = 1;
    ret = setsockopt(udp_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
    if(ret < 0) {
       perror("setsockopt:");
       Close();
       return -1;
    }

    ret = bind(udp_fd_, (struct sockaddr*)&broadcast_addr_, sizeof(broadcast_addr_));
    if(ret < 0) { 
        perror("bind:");
        Close();
        return -1;
    } 

    return 0;
}

int UdpSocket::Send(const char* buf, int size, const sockaddr_in* send_addr, const int addr_len)
{
    return sendto(udp_fd_, buf, size, 0, (struct sockaddr*)send_addr, addr_len); 
}

int UdpSocket::Broadcast(const char* buf, int size)
{
    return Send(buf, size, &broadcast_addr_, sizeof(broadcast_addr_));
}

int UdpSocket::Recv(char* buf, int size, sockaddr_in* recv_addr, int* addr_len)
{
    return recvfrom(udp_fd_, buf, size, 0, (struct sockaddr*)recv_addr, (socklen_t*)addr_len);
}

void UdpSocket::Close()
{
    if(udp_fd_ > 0) {
        close(udp_fd_);
        udp_fd_ = -1;
    }
}

UdpSocket::~UdpSocket()
{
    Close();
}

} //namespace paxoslease
