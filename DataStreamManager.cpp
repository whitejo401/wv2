// ============================================================================
// DataStreamManager.cpp
// ============================================================================
#include "pch.h"
#include "DataStreamManager.h"

CDataStreamManager::CDataStreamManager()
{
    srand(static_cast<UINT>(::GetTickCount64()));
}

CDataStreamManager::~CDataStreamManager()
{
    Stop();
}

// ============================================================================
// Start / Stop / SetRate
// ============================================================================
void CDataStreamManager::Start(int rate)
{
    InterlockedExchange(&m_nRate, (LONG)rate);
    InterlockedExchange((volatile LONG*)&m_bRunning, TRUE);

    m_pThread = AfxBeginThread(ThreadProc, this,
                               THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    m_pThread->m_bAutoDelete = FALSE;
    m_pThread->ResumeThread();
}

void CDataStreamManager::Stop()
{
    if (!m_bRunning) return;

    InterlockedExchange((volatile LONG*)&m_bRunning, FALSE);

    if (m_pThread)
    {
        ::WaitForSingleObject(m_pThread->m_hThread, 3000);
        delete m_pThread;
        m_pThread = nullptr;
    }

    CSingleLock lock(&m_csQueue, TRUE);
    m_queue.RemoveAll();
}

void CDataStreamManager::SetRate(int rate)
{
    InterlockedExchange(&m_nRate, (LONG)rate);
}

// ============================================================================
// DrainQueue - UI 스레드의 OnTimer에서 호출
// ============================================================================
void CDataStreamManager::DrainQueue(CStringList& outBatch)
{
    CSingleLock lock(&m_csQueue, TRUE);
    outBatch.AddTail(&m_queue);
    m_queue.RemoveAll();
}

// ============================================================================
// ThreadProc (정적 진입점) / WorkerProc (실제 루프)
// ============================================================================
UINT AFX_CDECL CDataStreamManager::ThreadProc(LPVOID pParam)
{
    reinterpret_cast<CDataStreamManager*>(pParam)->WorkerProc();
    return 0;
}

void CDataStreamManager::WorkerProc()
{
    while (m_bRunning)
    {
        LONG rate = m_nRate;
        if (rate == -1)
            rate = 10 + rand() % 991;   // Chaos: 10~1000건/초 랜덤

        DWORD sleepMs = (DWORD)max(1L, 1000L / rate);
        LONG  id      = InterlockedIncrement(&m_nCount);

        {
            CSingleLock lock(&m_csQueue, TRUE);
            m_queue.AddTail(GenerateRow(id));
        }

        ::Sleep(sleepMs);
    }
}

// ============================================================================
// GenerateRow - 단일 JSON 객체 생성
// ============================================================================
CString CDataStreamManager::GenerateRow(LONG id)
{
    static const TCHAR* colors[] = {
        _T("red"), _T("blue"), _T("green"), _T("orange"), _T("purple"),
        _T("cyan"), _T("magenta"), _T("yellow"), _T("white"), _T("coral")
    };
    static const TCHAR* genders[] = { _T("male"), _T("female") };

    CString row;
    row.Format(
        _T("{\"id\":%d,\"name\":\"Stream User %d\",")
        _T("\"age\":%d,\"gender\":\"%s\",")
        _T("\"height\":%.2f,\"col\":\"%s\",")
        _T("\"dob\":\"2024-01-01\"}"),
        id, id,
        20 + (id % 60), genders[id % 2],
        1.5 + (id % 50) * 0.01, colors[id % 10]);

    return row;
}
