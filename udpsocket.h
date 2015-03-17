#ifndef PAXOSLEASE_UDPSOCKET_H
#define PAXOSLEASE_UDPSOCKET_H

#include <arpa/inet.h>

namespace paxoslease
{

class UdpSocket
{
public:
UdpSocket(int port);
~UdpSocket();

int Open(); 

int Send(const char* buf, int size, const sockaddr_in* send_addr, const int addr_len);
int Broadcast(const char* buf, int size);
int Recv(char* buf, int size, sockaddr_in* recv_addr, int* addr_len);

void Close();

private:
    int udp_fd_;
    int port_;
    struct sockaddr_in broadcast_addr_;

};

} //namespace paxoslease

#endif //PAXOSLEASE_UDPSOCKET_H
