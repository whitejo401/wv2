// ============================================================================
// WebView2Dlg.cpp
//
// [구조]
//   CWebView2Dlg      — UI 라이프사이클 + 메시지 라우팅
//   CDataStreamManager — Worker Thread 전담 (Start/Stop/SetRate/DrainQueue)
//   CWebView2         — WebView2 래퍼 (Environment는 싱글톤 공유)
//
// [타이머 역할]
//   16ms마다 m_stream.DrainQueue() → 배치 JSON → ExecuteScript (UI 스레드)
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
// OnInitWebView2 - 지연된 WebView2 초기화
// ============================================================================
LRESULT CWebView2Dlg::OnInitWebView2(WPARAM, LPARAM)
{
    m_wv2.InitWebView2();
    m_wv2.SetInitialUrl(L"http://app.local/index.html");
    return 0;
}

// ============================================================================
// OnWebMessage - JS → C++ 메시지 처리
// ============================================================================
void CWebView2Dlg::OnWebMessage(const CString& message)
{
    if (message.Find(_T("STREAM_START:")) == 0)
    {
        int rate = _ttoi(message.Mid((int)_tcslen(_T("STREAM_START:"))));
        m_stream.Stop();
        m_stream.Start(rate);
        SetTimer(BATCH_TIMER_ID, BATCH_TIMER_MS, nullptr);
    }
    else if (message == _T("STREAM_STOP"))
    {
        m_stream.Stop();
        KillTimer(BATCH_TIMER_ID);
    }
    else if (message.Find(_T("STREAM_RATE:")) == 0)
    {
        int rate = _ttoi(message.Mid((int)_tcslen(_T("STREAM_RATE:"))));
        m_stream.SetRate(rate);
    }
}

// ============================================================================
// OnTimer - 배치 drain + JS 전송 (~60fps)
// ============================================================================
void CWebView2Dlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == BATCH_TIMER_ID)
    {
        CStringList batch;
        m_stream.DrainQueue(batch);     // ← 한 줄로 큐 전체 수거

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
    m_stream.Stop();
    KillTimer(BATCH_TIMER_ID);
    m_wv2.Close();
    CDialogEx::OnDestroy();
}

// ============================================================================
// 모달리스 다이얼로그 라이프사이클
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