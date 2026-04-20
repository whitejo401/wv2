#pragma once

#include "resource.h"
#include "WebView2.h"
#include "DataStreamManager.h"

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
    afx_msg LRESULT OnInitWebView2(WPARAM, LPARAM);
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    virtual void OnCancel() override;
    virtual void OnOK() override;
    virtual void PostNcDestroy() override;

    void OnWebMessage(const CString& message);

    DECLARE_MESSAGE_MAP()

private:
    CWebView2           m_wv2;
    CDataStreamManager  m_stream;   // Worker Thread 전담

    static constexpr UINT_PTR BATCH_TIMER_ID = 1;
    static constexpr UINT     BATCH_TIMER_MS = 16;  // ~60fps
};
