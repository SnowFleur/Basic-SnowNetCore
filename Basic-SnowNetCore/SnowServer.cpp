#include"SnowServer.h"

CSnowServer::CSnowServer(const uint32_t workerThreadCount):
    workerThreadCount_(workerThreadCount)
    , cNetAddress_{}
{
    vecWorkerThread_.reserve(workerThreadCount_);
}

CSnowServer::~CSnowServer() noexcept
{

    WSACleanup();
}

void CSnowServer::StartSnowServer(const char* pServerIP, const USHORT port)
{
    if (pServerIP == nullptr) return;

    WSADATA stWSAData;
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &stWSAData) != 0)
    {
        PRINT_ERROR_LOG("Can Not Load winsock.dll", WSAGetLastError());
        return;
    }

    for (uint32_t i = 0; i < workerThreadCount_; ++i)
    {
        UptrSnowThread pSnowThread = std::make_unique<CSnowThread>(&CSnowServer::ExcuteWorkerThread, this);
        pSnowThread->SetThreadID(i * WORERK_THREAD_ID);
        vecWorkerThread_.emplace_back(std::move(pSnowThread));
    }

    bool isRunningAccpet = true;

    CSnowSocket accpetSocket{ SOCKET_TYPE::TCP_TYPE };
    if (accpetSocket.OnBind(pServerIP, port) == false) return;
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

    for (auto& iter : vecWorkerThread_)
    {
        iter->WaitForThread();
    }

}

uint32_t CSnowServer::ExcuteWorkerThread()
{
    bool isRunning = true;

    DWORD IoByte;
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

            OverlappedEx* pOverEx = reinterpret_cast<OverlappedEx*>(overlapped);

            switch (pOverEx->ioEvent_)
            {
            case IO_EVENT::RECV:
            {
                CompletedRecv(pOverEx->pSession_, IoByte);
                SAFE_DELETE(pOverEx);
                break;
            }
            case IO_EVENT::SEND:
            {
                CompletedSend(pOverEx->pSession_, IoByte);
                SAFE_DELETE(pOverEx);
                break;
            }
            default:
            {
                std::cout << "Error: Not Define Io Event\n";
                break;
            }
            } // End Switch
        }
    }
    return 0;
}

