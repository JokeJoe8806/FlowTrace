#include "stdafx.h"
#include "WorkerThread.h"


WorkerThread::WorkerThread(void)
{
    m_bWorking = false;
    m_hThread = 0;
}

WorkerThread::~WorkerThread(void)
{
}

void WorkerThread::StopWork()
{
  if (m_bWorking) {
    m_bWorking = false; // signal end of work
    Terminate();
  }
    if (m_hThread)
    {
        while (m_hThread != 0 && WAIT_OBJECT_0 != WaitForSingleObject(m_hThread, 0))
        {
            //MSG msg;
            //while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
            //{
            //    TranslateMessage(&msg);
            //    DispatchMessage(&msg);
            //}
            Sleep(0); // alow Worker thread complite its waorks
        }
    }
}

void WorkerThread::StartWork(LPVOID pWorkParam /*=0*/)
{
    StopWork();

    m_pWorkParam = pWorkParam;
    m_hTreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hThread = CreateThread(0, 0, ThreadFunk, this, 0, &m_dwTID);
    WaitForSingleObject(m_hTreadEvent, INFINITE);
    CloseHandle(m_hTreadEvent);
}

DWORD WINAPI WorkerThread::ThreadFunk(LPVOID lpParameter)
{
    WorkerThread* This = (WorkerThread*)lpParameter;

    This->m_bWorking = true;

    SetEvent(This->m_hTreadEvent);
    This->Work(This->m_pWorkParam);

    This->m_bWorking = false;
    CloseHandle(This->m_hThread);
    This->m_hThread = NULL;

    return 0;
}

void WorkerThread::OnThreadReady()
{
  SetEvent(m_hTreadEvent);
}

