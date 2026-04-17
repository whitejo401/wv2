// ============================================================================
// WebView2Dlg.h - CWebView2Host를 사용하는 MFC 다이얼로그 예제
//
// 다른 다이얼로그에서 WebView2를 사용하는 방법:
//   1. #include "WebView2Host.h" 추가
//   2. CWebView2Host m_wv2; 멤버 선언
//   3. 필요하면 IWebView2MessageHandler 구현
//   4. 아래 패턴 그대로 복사
// ============================================================================
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

    // 모달리스 다이얼로그 필수 오버라이드
    virtual void OnCancel() override;     // ESC 키 처리
    virtual void OnOK() override;         // Enter 키 처리
    virtual void PostNcDestroy() override; // 메모리 해제

    // 웹 메시지 수신 핸들러 (콜백으로 연결됨)
    void OnWebMessage(const std::wstring& message);

    DECLARE_MESSAGE_MAP()

private:
    CWebView2 m_wv2;

    CString GetInstallPathFromRegistry(bool const searchWebView=true);
    CString GetInstallPathFromDisk(bool const searchWebView=true);
};
