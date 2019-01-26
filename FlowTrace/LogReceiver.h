#pragma once
#include "WorkerThread.h"
#include "Archive.h"

//#define USE_TCP

#ifdef USE_TCP
#define MAX_NET_BUF 16*1024  //maximum TCP window size in Microsoft Windows 2000 is 17,520 bytes 
#else
#define MAX_NET_BUF 16*1024  //max UDP datagam is 65515 Bytes
#endif

class NetThread : public WorkerThread
{
public:
    virtual void Terminate();

protected:
    SOCKET s;
};

class UdpThread : public NetThread
{
    virtual void Work(LPVOID pWorkParam);
    char buf[MAX_NET_BUF + sizeof(NET_PACK_INFO)];
public:
    UdpThread();
};

#ifdef USE_TCP

class TcpListenThread : public NetThread
{
    virtual void Work(LPVOID pWorkParam);
public:
    TcpListenThread();
};

class TcpReceiveThread : public NetThread
{
    virtual void Work(LPVOID pWorkParam);
    char buf[MAX_NET_BUF + sizeof(NET_PACK_INFO)];
public:
    TcpReceiveThread(SOCKET sclientSocket);
};

#endif //USE_TCP

class LogReceiver
{
public:
    LogReceiver();
    void start();
    void stop();
    void add(NetThread* pNetThread);
    void lock() { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
    bool working() { return m_working; }
private:
    bool m_working;
    CRITICAL_SECTION cs;
};

extern LogReceiver gLogReceiver;