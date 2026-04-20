#pragma once

#include "resource.h"
#include "WebView2.h"

// Custom message for deferred WebView2 initialization
#define WM_INIT_WEBVIEW2 (WM_USER + 100)

class CWebView2Dlg : public CDialogEx
{
public:
    CWebView2Dlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MAIN_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnInitWebView2(WPARAM wParam, LPARAM lParam);
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    // 모달리스 다이얼로그 필수 오버라이드
    virtual void OnCancel() override;
    virtual void OnOK() override;
    virtual void PostNcDestroy() override;

    void OnWebMessage(const CString& message);

    DECLARE_MESSAGE_MAP()

private:
    CWebView2 m_wv2;

    // ── Worker Thread (데이터 생성 전담) ────────────────────────────────────
    CWinThread*    m_pWorkerThread { nullptr };
    volatile BOOL  m_bRunWorker    { FALSE };
    volatile LONG  m_nRatePerSec   { 50 };      // -1 = Chaos 모드
    volatile LONG  m_nDataCount    { 0 };        // 총 생성 행 수 (ID 채번)

    // ── UI Thread 전송용 큐 ─────────────────────────────────────────────────
    CCriticalSection m_csQueue;
    CStringList      m_dataQueue;

    // ── Timer ───────────────────────────────────────────────────────────────
    static constexpr UINT_PTR BATCH_TIMER_ID = 1;
    static constexpr UINT     BATCH_TIMER_MS = 16; // ~60fps

    // ── 메서드 ──────────────────────────────────────────────────────────────
    void    StartDataWorker(int ratePerSec);
    void    StopDataWorker();
    CString GenerateRow(LONG id);

    // AfxBeginThread 용 정적 진입점
    static UINT AFX_CDECL StaticWorkerProc(LPVOID pParam);
    void WorkerProc();
};
