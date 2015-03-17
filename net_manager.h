#ifndef PAXOSLEASE_NET_MANAGER_H
#define PAXOSLEASE_NET_MANAGER_H

namespace paxoslease {


class Connection {
public:
    enum kEventType {
        EVENT_NET_READ  = 0,
        EVENT_NET_WROTE = 1,
        EVENT_NET_ERROR = 2,
        EVENT_TIMER_READ = 3,
        EVENT_PIPE_READ = 4
    };

    enum kFdType {
        TYPE_SOCKET = 1;
        TYPE_TIMER = 2;
        TYPE_PIPE = 3;
    };

    typedef std::function<void(kEventType code, void* data)> ConnCallback; 

    Connection(int fd, kFdType fd_type, ConnCallback cb);
    
    int HandleReadEvent();

    int HandleWriteEvent();

    int HandleErrorEvent();

    int fd() const { return conn_fd_; }
    
private:
    ConnCallback conn_callback_;
    int conn_fd_;
    kFdType conn_fd_type_;

};

class NetManager {
public:
    class kEpollType {
        static const IN = 0;
        static const OUT = 1;
        static const ERR = 2;
    };
    void Loop();

    int AddConnection();
    int RemoveConnction();

private:
    bool running_;

};


} // namespace paxoslease

#endif // PAXOSLEASE_NET_MANAGER_H
