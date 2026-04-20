// ============================================================================
// WebView2.h
//
// MFC CWnd를 상속받은 재사용 가능한 WebView2 컨트롤
//
// [사용법]
//   1. 다이얼로그나 뷰의 헤더에 멤버 추가:
//        #include "WebView2.h"
//        CWebView2 m_webview;
//
//   2. 리소스 에디터에 넣은 Static Control (예: IDC_WEBVIEW) 자리에 붙이려면 
//      OnInitDialog 에서 Subclassing 하거나 동적 Create:
//        CRect rc;
//        GetDlgItem(IDC_WEBVIEW)->GetWindowRect(rc);
//        ScreenToClient(&rc);
//        m_webview.Create(rc, this, 10001); // 10001은 제어용 ID
//
//   3. 초기화 (비동기):
//        m_webview.InitWebView2();
//
//   4. 페이지 로드:
//        m_webview.Navigate(L"https://example.com");
//
// ============================================================================
#pragma once

#include <string>
#include <functional>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include "WebView2EnvironmentOptions.h"

// ----------------------------------------------------------------------------
// CWebView2 클래스
// ----------------------------------------------------------------------------
class CWebView2 : public CWnd
{
    DECLARE_DYNAMIC(CWebView2)

public:
    CWebView2();
    virtual ~CWebView2();

    // 윈도우 생성
    BOOL Create(const RECT& rect, CWnd* pParentWnd, UINT nID);

    // WebView2 컴포넌트 초기화 실행
    void InitWebView2();

    // 초기화 완료 시 호출될 콜백 설정
    void SetOnInitCompleteCallback(std::function<void()> callback) { m_onInitComplete = callback; }

    // 웹에서 호스트로 보낸 메시지(JavaScript window.chrome.webview.postMessage) 수신 콜백 설정
    void SetOnWebMessageReceivedCallback(std::function<void(const std::wstring&)> callback) { m_onWebMessage = callback; }

    // 탐색 및 스크립트 실행
    HRESULT Navigate(const std::wstring& url);
    HRESULT NavigateToString(const std::wstring& html);
    HRESULT ExecuteScript(const std::wstring& script);
    HRESULT PostWebMessageAsJson(const std::wstring& jsonStr);   

    // 초기화 끝나기 전이라도 로드할 페이지 예약
    void SetInitialHtml(const std::wstring& html) { m_initialHtml = html; }
    void SetInitialUrl(const std::wstring& url)   { m_initialUrl  = url;  }

    // 상태 확인
    bool IsReady() const { return m_webviewController != nullptr; }

    // WebView2 컨트롤 직접 접근 가능
    wil::com_ptr<ICoreWebView2>           GetWebView()     { return m_webview; }
    wil::com_ptr<ICoreWebView2Controller> GetController()  { return m_webviewController; }

    void Close();

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();

    DECLARE_MESSAGE_MAP()

private:
    wil::com_ptr<ICoreWebView2Controller> m_webviewController;
    wil::com_ptr<ICoreWebView2>           m_webview;

    std::wstring m_initialHtml;
    std::wstring m_initialUrl;

    bool m_isInitializing;

    std::function<void()> m_onInitComplete;
    std::function<void(const std::wstring&)> m_onWebMessage;   
};
