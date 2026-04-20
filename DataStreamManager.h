// ============================================================================
// DataStreamManager.h
//
// Worker Thread 기반 스트리밍 데이터 생성/큐 관리 전담 클래스 (순수 MFC/Win32)
//
// [사용법]
//   CDataStreamManager m_stream;
//   m_stream.Start(100);            // 100건/초 시작
//   m_stream.SetRate(500);          // 속도 즉시 변경 (재시작 불필요)
//   m_stream.DrainQueue(batch);     // OnTimer에서 큐 전체 수거
//   m_stream.Stop();                // 중지
// ============================================================================
#pragma once

class CDataStreamManager
{
public:
    CDataStreamManager();
    ~CDataStreamManager();

    // rate: 건/초, -1 = Chaos (10~1000 랜덤)
    void Start(int rate);
    void Stop();
    void SetRate(int rate);

    bool IsRunning() const { return m_bRunning == TRUE; }

    // UI 스레드의 OnTimer에서 호출: 큐 전체를 outBatch로 이동 (O(1))
    void DrainQueue(CStringList& outBatch);

private:
    CWinThread*      m_pThread  { nullptr };
    volatile BOOL    m_bRunning { FALSE };
    volatile LONG    m_nRate    { 50 };
    volatile LONG    m_nCount   { 0 };

    CCriticalSection m_csQueue;
    CStringList      m_queue;

    static UINT AFX_CDECL ThreadProc(LPVOID pParam);
    void WorkerProc();

    static CString GenerateRow(LONG id);
};
