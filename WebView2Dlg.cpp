// ============================================================================
// WebView2Dlg.cpp
//
// [아키텍처: C++ Push 방식 / MFC 전용 구현]
//   - Worker Thread: AfxBeginThread + CWinThread
//   - 원자적 연산:   InterlockedExchange / InterlockedIncrement (volatile LONG)
//   - 동기화:        CCriticalSection + CSingleLock
//   - 큐:           CStringList
//   - UI 전송:       SetTimer(16ms) → OnTimer에서 배치 drain → ExecuteScript
//
// 메시지 프로토콜 (JS → C++):
//   STREAM_START:{rate}  → 워커 스레드 (재)시작, rate=-1은 Chaos
//   STREAM_STOP          → 워커 스레드 정지
//   STREAM_RATE:{rate}   → 실행 중 속도 즉시 변경 (스레드 재시작 없음)
// ============================================================================
#include "pch.h"
#include "WebView2App.h"
#include "WebView2Dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CWebView2Dlg, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_MESSAGE(WM_INIT_WEBVIEW2, &CWebView2Dlg::OnInitWebView2)
END_MESSAGE_MAP()

// ============================================================================
// Constructor / DoDataExchange
// ============================================================================
CWebView2Dlg::CWebView2Dlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_MAIN_DIALOG, pParent)
{
    srand(static_cast<UINT>(::GetTickCount64()));
}

void CWebView2Dlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

// ============================================================================
// OnInitDialog
// ============================================================================
BOOL CWebView2Dlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("WebView2 Hello World (MFC Dialog)"));
    SetWindowPos(nullptr, 0, 0, 1200, 800, SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    CRect rcClient;
    GetClientRect(&rcClient);
    m_wv2.Create(rcClient, this, 10001);
    m_wv2.SetOnWebMessageReceivedCallback([this](const std::wstring& msg) {
        OnWebMessage(CString(msg.c_str()));
    });

    PostMessage(WM_INIT_WEBVIEW2);
    return TRUE;
}

// ============================================================================
// OnInitWebView2 - PostMessage로 지연된 WebView2 초기화
// ============================================================================
LRESULT CWebView2Dlg::OnInitWebView2(WPARAM, LPARAM)
{
    m_wv2.InitWebView2();
    m_wv2.SetInitialUrl(L"http://app.local/index.html");
    return 0;
}

// ============================================================================
// OnWebMessage - 웹→호스트 메시지 수신
// ============================================================================
void CWebView2Dlg::OnWebMessage(const CString& message)
{
    if (message.Find(_T("STREAM_START:")) == 0)
    {
        int rate = _ttoi(message.Mid((int)_tcslen(_T("STREAM_START:"))));
        StopDataWorker();
        StartDataWorker(rate);
    }
    else if (message == _T("STREAM_STOP"))
    {
        StopDataWorker();
    }
    else if (message.Find(_T("STREAM_RATE:")) == 0)
    {
        LONG rate = (LONG)_ttoi(message.Mid((int)_tcslen(_T("STREAM_RATE:"))));
        InterlockedExchange(&m_nRatePerSec, rate);
    }
}

// ============================================================================
// StartDataWorker / StopDataWorker
// ============================================================================
void CWebView2Dlg::StartDataWorker(int ratePerSec)
{
    InterlockedExchange(&m_nRatePerSec, (LONG)ratePerSec);
    InterlockedExchange((volatile LONG*)&m_bRunWorker, TRUE);

    // m_bAutoDelete = FALSE: 스레드 핸들을 직접 관리하여 종료 대기 가능
    m_pWorkerThread = AfxBeginThread(StaticWorkerProc, this,
                                     THREAD_PRIORITY_NORMAL, 0,
                                     CREATE_SUSPENDED);
    m_pWorkerThread->m_bAutoDelete = FALSE;
    m_pWorkerThread->ResumeThread();

    SetTimer(BATCH_TIMER_ID, BATCH_TIMER_MS, nullptr);
}

void CWebView2Dlg::StopDataWorker()
{
    if (!m_bRunWorker) return;

    InterlockedExchange((volatile LONG*)&m_bRunWorker, FALSE);

    if (m_pWorkerThread != nullptr)
    {
        // 스레드가 자연 종료될 때까지 대기 (최대 3초)
        ::WaitForSingleObject(m_pWorkerThread->m_hThread, 3000);
        delete m_pWorkerThread;
        m_pWorkerThread = nullptr;
    }

    KillTimer(BATCH_TIMER_ID);

    // 잔여 큐 비우기
    CSingleLock lock(&m_csQueue, TRUE);
    m_dataQueue.RemoveAll();
}

// ============================================================================
// StaticWorkerProc - AfxBeginThread 진입점 (정적 함수)
// ============================================================================
UINT AFX_CDECL CWebView2Dlg::StaticWorkerProc(LPVOID pParam)
{
    reinterpret_cast<CWebView2Dlg*>(pParam)->WorkerProc();
    return 0;
}

// ============================================================================
// WorkerProc - 백그라운드 데이터 생성 루프
// 주의: COM/WebView2 객체 절대 접근 금지. 큐에만 push.
// ============================================================================
void CWebView2Dlg::WorkerProc()
{
    while (m_bRunWorker)
    {
        LONG rate = m_nRatePerSec;

        // Chaos 모드: 10~1000건/초 랜덤
        if (rate == -1)
            rate = 10 + rand() % 991;

        // 건당 대기 시간 (최소 1ms)
        DWORD sleepMs = (DWORD)max(1L, 1000L / rate);

        LONG id = InterlockedIncrement(&m_nDataCount);

        {
            CSingleLock lock(&m_csQueue, TRUE);
            m_dataQueue.AddTail(GenerateRow(id));
        }

        ::Sleep(sleepMs);
    }
}

// ============================================================================
// GenerateRow - 단일 JSON 객체 CString 생성
// ============================================================================
CString CWebView2Dlg::GenerateRow(LONG id)
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

// ============================================================================
// OnTimer - UI 스레드 배치 전송 (~60fps)
// ============================================================================
void CWebView2Dlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == BATCH_TIMER_ID)
    {
        CStringList batch;
        {
            // O(1) 이동으로 락 시간 최소화
            CSingleLock lock(&m_csQueue, TRUE);
            batch.AddTail(&m_dataQueue);
            m_dataQueue.RemoveAll();
        }

        if (!batch.IsEmpty())
        {
            CString json = _T("[");
            BOOL bFirst = TRUE;
            while (!batch.IsEmpty())
            {
                if (!bFirst) json += _T(",");
                json += batch.RemoveHead();
                bFirst = FALSE;
            }
            json += _T("]");

            CString script = _T("window.appendGridData(") + json + _T(");");
            m_wv2.ExecuteScript(script.GetString());
        }
        return;
    }
    CDialogEx::OnTimer(nIDEvent);
}

// ============================================================================
// OnSize / OnGetMinMaxInfo / OnDestroy
// ============================================================================
void CWebView2Dlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (m_wv2.GetSafeHwnd())
        m_wv2.MoveWindow(0, 0, cx, cy);
}

void CWebView2Dlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = 640;
    lpMMI->ptMinTrackSize.y = 480;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CWebView2Dlg::OnDestroy()
{
    StopDataWorker();
    m_wv2.Close();
    CDialogEx::OnDestroy();
}

// ============================================================================
// 모달리스 다이얼로그 라이프사이클 관리
// ============================================================================
void CWebView2Dlg::OnCancel() { DestroyWindow(); }
void CWebView2Dlg::OnOK()     { DestroyWindow(); }

void CWebView2Dlg::PostNcDestroy()
{
    CDialogEx::PostNcDestroy();
    AfxGetApp()->m_pMainWnd = nullptr;
    PostQuitMessage(0);
    delete this;
}