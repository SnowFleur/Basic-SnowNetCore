#include"SnowServer.h"

CSnowServer::CSnowServer(const char* pServerIP, const USHORT port) :
    workerThreadCount_()
    , netAddress_{}
{

    netAddress_.SetAddrInfor(pServerIP, port);

    WSADATA stWSAData;
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &stWSAData) != 0)
    {
        PRINT_ERROR_LOG("Can Not Load winsock.dll", WSAGetLastError());
        return;
    }
    vecWorkerThread_.reserve(workerThreadCount_);
}

CSnowServer::~CSnowServer() noexcept
{
    vecWorkerThread_.clear();
    WSACleanup();
}

void CSnowServer::StartSnowServer()
{
 
    StartWorkerThread();

    bool isRunningAccpet = true;

    CSnowSocket accpetSocket{ SOCKET_TYPE::TCP_TYPE };
    if (accpetSocket.OnBind(netAddress_.GetAddrInfor()) == false) return;
    if (accpetSocket.OnListen() == false) return;

    uint32_t sessionID = 0;

    while (isRunningAccpet)
    {
        CNetAddress cClientAddress;
        SOCKET tempSocket = accpetSocket.OnAccept(reinterpret_cast<SOCKADDR*>(cClientAddress.GetAddrInfor()));

        if (tempSocket != INVALID_SOCKET)
        {
            try
            {
                CSnowSession* pSnowSession = new CSnowSession{ SOCKET_TYPE::TCP_TYPE,sessionID };
                pSnowSession->SetSocket(tempSocket);
                RegitIocp(pSnowSession->GetSocket());
                sessionID++;
                CompletedAccpet(pSnowSession);
            }

            catch (std::exception& ex)
            {
                PRINT_ERROR_LOG("Accpet", ex.what(), "\n");
            }
        }
    }

    WaitForWorkerThread();


}

bool CSnowServer::CreateWorkerTherad(const uint32_t threadCount, const bool isStartThread)
{
    workerThreadCount_ = threadCount;
    vecWorkerThread_.reserve(workerThreadCount_);

    for (ThreadID i = 0; i < threadCount; ++i)
    {
        UptrSnowThread pSnowThread = std::make_unique<CSnowThread>(isStartThread, &CSnowServer::ExcuteWorkerThread, this);
        pSnowThread->SetThreadID(i * WORERK_THREAD_ID);
        vecWorkerThread_.emplace_back(std::move(pSnowThread));
    }
    return true;
}

void CSnowServer::StartWorkerThread()
{
    for (const auto& iter : vecWorkerThread_)
    {
        if (iter->GetIsStartedThread() == false)
        {
            if (iter->StartThread() == true)
            {
                PRINT_INFO_LOG("Sucess Start WorkerThread id:", iter->GetThreadID(), "\n");
            }
            else
            {
                PRINT_ERROR_LOG("Failed Start WorkerThread id:", iter->GetThreadID(), "\n");
            }
        }
        else
        {
            PRINT_INFO_LOG(" Started WorkerThread id:", iter->GetThreadID(), "\n");
        }
    }
}

void CSnowServer::WaitForWorkerThread()
{
    for (auto& iter : vecWorkerThread_)
    {
        iter->WaitForThread();
    }
}

uint32_t CSnowServer::ExcuteWorkerThread()
{
    bool isRunning = true;
    DWORD IoByte = 0;
    LPOVERLAPPED  overlapped = nullptr;
    bool          IoEventResult = true;
#ifdef _WIN64
    ULONGLONG Iokey;
#else
    ULONG Iokey;
#endif //  _WIN64

    while (isRunning)
    {
        IoEventResult = GetQueuedCompletionStatus(hIocpHandle_, &IoByte, &Iokey, &overlapped, INFINITE);

        if (IoEventResult == false)
        {
            if (overlapped == nullptr && WSAGetLastError() != 64)
            {
                std::cout << WSAGetLastError() << "\n";
                return 0;
            }

        }

        OverlappedEx* pOverEx = reinterpret_cast<OverlappedEx*>(overlapped);

        switch (pOverEx->ioEvent_)
        {
        case IO_EVENT::RECV:
        {
            CompletedRecv(pOverEx->pSession_, IoByte);
            break;
        }
        case IO_EVENT::SEND:
        {
            CompletedSend(pOverEx->pSession_, IoByte);
            break;
        }
        default:
        {
            PRINT_ERROR_LOG("Not Define Io Event\n");
            break;
        }
        SAFE_DELETE(pOverEx);
        }
    }
    return 0;
}

