// ============================================================================
// WebView2Dlg.cpp
//
// [아키텍처: C++ Push 방식]
//   - Worker Thread: m_nRatePerSec 속도에 맞춰 JSON 행을 m_dataQueue에 push
//   - UI Thread (OnTimer): 16ms마다 큐 전체를 drain → 배치 JSON → ExecuteScript
//   - JS는 요청(Pull) 없이 순수하게 수신/렌더링만 담당
//
// 메시지 프로토콜 (JS → C++):
//   STREAM_START:{rate}  → 워커 스레드 (재)시작, rate=-1은 Chaos
//   STREAM_STOP          → 워커 스레드 정지
//   STREAM_RATE:{rate}   → 실행 중 속도 즉시 변경 (스레드 재시작 없음)
// ============================================================================
#include "pch.h"
#include "WebView2App.h"
#include "WebView2Dlg.h"

#include <chrono>
#include <random>

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
        OnWebMessage(msg);
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
void CWebView2Dlg::OnWebMessage(const std::wstring& message)
{
    if (message.rfind(L"STREAM_START:", 0) == 0)
    {
        int rate = _wtoi(message.c_str() + wcslen(L"STREAM_START:"));
        StopDataWorker();
        StartDataWorker(rate);
    }
    else if (message == L"STREAM_STOP")
    {
        StopDataWorker();
    }
    else if (message.rfind(L"STREAM_RATE:", 0) == 0)
    {
        int rate = _wtoi(message.c_str() + wcslen(L"STREAM_RATE:"));
        m_nRatePerSec.store(rate);
    }
}

// ============================================================================
// StartDataWorker / StopDataWorker
// ============================================================================
void CWebView2Dlg::StartDataWorker(int ratePerSec)
{
    m_nRatePerSec.store(ratePerSec);
    m_bRunWorker.store(true);
    m_workerThread = std::thread([this]() { WorkerProc(); });
    SetTimer(BATCH_TIMER_ID, BATCH_TIMER_MS, nullptr);
}

void CWebView2Dlg::StopDataWorker()
{
    if (!m_bRunWorker.load()) return;

    m_bRunWorker.store(false);
    if (m_workerThread.joinable())
        m_workerThread.join();

    KillTimer(BATCH_TIMER_ID);

    std::lock_guard<std::mutex> lock(m_queueMutex);
    while (!m_dataQueue.empty())
        m_dataQueue.pop();
}

// ============================================================================
// WorkerProc - 백그라운드 데이터 생성 루프
// 주의: COM/WebView2 객체 절대 접근 금지. 큐에만 push.
// ============================================================================
void CWebView2Dlg::WorkerProc()
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> chaosDist(10, 1000);

    while (m_bRunWorker.load())
    {
        int rate = m_nRatePerSec.load();
        if (rate == -1)
            rate = chaosDist(rng);

        int sleepMs = max(1, 1000 / rate);

        int id = ++m_nDataCount;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_dataQueue.push(GenerateRow(id));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

// ============================================================================
// GenerateRow - 단일 JSON 객체 문자열 생성
// ============================================================================
std::wstring CWebView2Dlg::GenerateRow(int id)
{
    static const wchar_t* colors[] = {
        L"red", L"blue", L"green", L"orange", L"purple",
        L"cyan", L"magenta", L"yellow", L"white", L"coral"
    };
    static const wchar_t* genders[] = { L"male", L"female" };

    wchar_t buf[256];
    swprintf_s(buf, _countof(buf),
        L"{\"id\":%d,\"name\":\"Stream User %d\","
        L"\"age\":%d,\"gender\":\"%s\","
        L"\"height\":%.2f,\"col\":\"%s\","
        L"\"dob\":\"2024-01-01\"}",
        id, id,
        20 + (id % 60), genders[id % 2],
        1.5 + (id % 50) * 0.01, colors[id % 10]);

    return std::wstring(buf);
}

// ============================================================================
// OnTimer - UI 스레드 배치 전송 (~60fps)
// ============================================================================
void CWebView2Dlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == BATCH_TIMER_ID)
    {
        std::queue<std::wstring> batch;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            std::swap(batch, m_dataQueue); // O(1) swap으로 락 시간 최소화
        }

        if (!batch.empty())
        {
            std::wstring json = L"[";
            bool first = true;
            while (!batch.empty())
            {
                if (!first) json += L",";
                json += batch.front();
                batch.pop();
                first = false;
            }
            json += L"]";
            m_wv2.ExecuteScript((L"window.appendGridData(" + json + L");").c_str());
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