// ============================================================================
// WebView2.h
//
// MFC CWnd를 상속받은 재사용 가능한 WebView2 컨트롤
//
// [사용법]
//   1. 헤더에 멤버 추가:
//        #include "WebView2.h"
//        CWebView2 m_webview;
//
//   2. OnInitDialog에서 생성:
//        CRect rc;
//        GetDlgItem(IDC_WEBVIEW)->GetWindowRect(rc);
//        ScreenToClient(&rc);
//        m_webview.Create(rc, this, 10001);
//
//   3. 초기화 (비동기, Environment는 앱 전체 싱글톤 공유):
//        m_webview.InitWebView2();
//
//   4. 페이지 로드:
//        m_webview.Navigate(L"https://example.com");
// ============================================================================
#pragma once

#include <string>
#include <functional>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include "WebView2EnvironmentOptions.h"
#include "WebView2EnvManager.h"

class CWebView2 : public CWnd
{
    DECLARE_DYNAMIC(CWebView2)

public:
    CWebView2();
    virtual ~CWebView2();

    BOOL Create(const RECT& rect, CWnd* pParentWnd, UINT nID);

    void InitWebView2();

    // 콜백 등록
    void SetOnInitCompleteCallback(std::function<void()> callback)                         { m_onInitComplete = callback; }
    void SetOnWebMessageReceivedCallback(std::function<void(const std::wstring&)> callback){ m_onWebMessage   = callback; }

    // 탐색 / 스크립트 실행 — LPCWSTR 오버로드 (MFC 친화적)
    HRESULT Navigate(LPCWSTR url);
    HRESULT NavigateToString(LPCWSTR html);
    HRESULT ExecuteScript(LPCWSTR script);
    HRESULT PostWebMessageAsJson(LPCWSTR jsonStr);

    // std::wstring 오버로드 (하위 호환)
    HRESULT Navigate(const std::wstring& url)          { return Navigate(url.c_str()); }
    HRESULT NavigateToString(const std::wstring& html) { return NavigateToString(html.c_str()); }
    HRESULT ExecuteScript(const std::wstring& script)  { return ExecuteScript(script.c_str()); }
    HRESULT PostWebMessageAsJson(const std::wstring& j){ return PostWebMessageAsJson(j.c_str()); }

    // 초기화 전 페이지 예약
    void SetInitialHtml(const std::wstring& html) { m_initialHtml = html; }
    void SetInitialUrl(const std::wstring& url)   { m_initialUrl  = url;  }

    bool IsReady() const { return m_webviewController != nullptr; }

    wil::com_ptr<ICoreWebView2>           GetWebView()    { return m_webview; }
    wil::com_ptr<ICoreWebView2Controller> GetController() { return m_webviewController; }

    void Close();

protected:
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();

    DECLARE_MESSAGE_MAP()

private:
    wil::com_ptr<ICoreWebView2Controller> m_webviewController;
    wil::com_ptr<ICoreWebView2>           m_webview;

    std::wstring m_initialHtml;
    std::wstring m_initialUrl;

    volatile BOOL m_isInitializing { FALSE };  // ★ volatile 보호

    std::function<void()>                      m_onInitComplete;
    std::function<void(const std::wstring&)>   m_onWebMessage;
};
