// ============================================================================
// WebView2.cpp
//
// MFC CWnd 기반 WebView2 컨트롤 구현
//
// [변경 이력]
//   - WebView2EnvManager 싱글톤 사용으로 다중 창 메모리 최적화
//   - --disable-web-security 제거 (가상 호스트 매핑으로 대체)
//   - m_isInitializing volatile BOOL 보호
//   - LPCWSTR API 오버로드 추가
// ============================================================================
#include "pch.h"
#include "WebView2.h"

static const TCHAR* kWebView2CtrlClass = _T("MFC_WebView2_Ctrl");

IMPLEMENT_DYNAMIC(CWebView2, CWnd)

BEGIN_MESSAGE_MAP(CWebView2, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

// ============================================================================
// 생성자 / 소멸자
// ============================================================================
CWebView2::CWebView2()  {}
CWebView2::~CWebView2() { Close(); }

// ============================================================================
// Create - 윈도우 생성
// ============================================================================
BOOL CWebView2::Create(const RECT& rect, CWnd* pParentWnd, UINT nID)
{
    static bool s_registered = false;
    if (!s_registered)
    {
        WNDCLASS wc       = {};
        wc.lpfnWndProc    = ::DefWindowProc;
        wc.hInstance      = AfxGetInstanceHandle();
        wc.lpszClassName  = kWebView2CtrlClass;
        wc.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClass(&wc);
        s_registered = true;
    }

    return CWnd::CreateEx(
        0, kWebView2CtrlClass, _T(""),
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        rect, pParentWnd, nID);
}

int CWebView2::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    return CWnd::OnCreate(lpCreateStruct) == -1 ? -1 : 0;
}

// ============================================================================
// InitWebView2 - 싱글톤 Environment에서 Controller만 생성
// ============================================================================
void CWebView2::InitWebView2()
{
    if (m_isInitializing || m_webviewController)
        return;

    HWND hWndHost = GetSafeHwnd();
    if (!::IsWindow(hWndHost))
    {
        TRACE(_T("[CWebView2] InitWebView2: 유효하지 않은 HWND. Create() 먼저 호출하세요.\n"));
        return;
    }

    InterlockedExchange((volatile LONG*)&m_isInitializing, TRUE);

    // ★ 핵심: Environment 싱글톤에서 가져옴 (없으면 생성 후 콜백)
    CWebView2EnvManager::Instance().GetEnvironment(
        [this, hWndHost](ICoreWebView2Environment* env)
        {
            if (!env || !::IsWindow(hWndHost))
            {
                InterlockedExchange((volatile LONG*)&m_isInitializing, FALSE);
                return;
            }

            // Controller는 각 인스턴스가 독립적으로 생성 → JS 컨텍스트 격리
            env->CreateCoreWebView2Controller(hWndHost,
                Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this, hWndHost](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
                    {
                        InterlockedExchange((volatile LONG*)&m_isInitializing, FALSE);

                        if (FAILED(result) || !controller)
                        {
                            TRACE(_T("[CWebView2] Controller 생성 실패. 0x%08X\n"), result);
                            return result;
                        }

                        m_webviewController = controller;
                        m_webviewController->get_CoreWebView2(&m_webview);

                        // WebView2 설정
                        wil::com_ptr<ICoreWebView2Settings> settings;
                        m_webview->get_Settings(&settings);
                        if (settings)
                        {
                            settings->put_IsScriptEnabled(TRUE);
                            settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                            settings->put_IsWebMessageEnabled(TRUE);
                            settings->put_IsStatusBarEnabled(FALSE);
                        }

                        // 컨트롤러 바운드 설정
                        CRect rc;
                        ::GetClientRect(hWndHost, &rc);
                        m_webviewController->put_Bounds(rc);
                        m_webviewController->put_IsVisible(TRUE);

                        // JS → C++ 메시지 수신 등록
                        EventRegistrationToken token;
                        m_webview->add_WebMessageReceived(
                            Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                {
                                    wil::unique_cotaskmem_string msg;
                                    if (SUCCEEDED(args->TryGetWebMessageAsString(&msg)))
                                        if (m_onWebMessage)
                                            m_onWebMessage(msg.get());
                                    return S_OK;
                                }).Get(), &token);

                        // 가상 호스트 매핑 (app.local → assets 폴더)
                        wil::com_ptr<ICoreWebView2_3> webview3;
                        if (SUCCEEDED(m_webview->QueryInterface(IID_PPV_ARGS(&webview3))))
                        {
                            wchar_t modulePath[MAX_PATH];
                            ::GetModuleFileNameW(NULL, modulePath, MAX_PATH);
                            std::wstring exeDir(modulePath);
                            exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));

                            std::wstring assetsPath = exeDir + L"\\assets";
                            if (::GetFileAttributesW(assetsPath.c_str()) == INVALID_FILE_ATTRIBUTES)
                                assetsPath = exeDir + L"\\..\\..\\assets";

                            webview3->SetVirtualHostNameToFolderMapping(
                                L"app.local",
                                assetsPath.c_str(),
                                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                        }

                        // 예약된 페이지 로드
                        if (!m_initialHtml.empty())
                            m_webview->NavigateToString(m_initialHtml.c_str());
                        else if (!m_initialUrl.empty())
                            m_webview->Navigate(m_initialUrl.c_str());

                        if (m_onInitComplete)
                            m_onInitComplete();

                        return S_OK;
                    }).Get());
        });
}

// ============================================================================
// OnSize / OnDestroy / Close
// ============================================================================
void CWebView2::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    if (m_webviewController)
    {
        CRect rc(0, 0, cx, cy);
        m_webviewController->put_Bounds(rc);
    }
}

void CWebView2::OnDestroy()
{
    Close();
    CWnd::OnDestroy();
}

void CWebView2::Close()
{
    if (m_webviewController)
    {
        m_webviewController->Close();
        m_webviewController = nullptr;
    }
    m_webview = nullptr;
}

// ============================================================================
// 탐색 / 스크립트 실행 (LPCWSTR 오버로드)
// ============================================================================
HRESULT CWebView2::Navigate(LPCWSTR url)
{
    if (!m_webview) return E_FAIL;
    return m_webview->Navigate(url);
}

HRESULT CWebView2::NavigateToString(LPCWSTR html)
{
    if (!m_webview) return E_FAIL;
    return m_webview->NavigateToString(html);
}

HRESULT CWebView2::ExecuteScript(LPCWSTR script)
{
    if (!m_webview) return E_FAIL;

    // 빈 완료 콜백: nullptr 전달 시 저우선순위 큐로 지연되는 버그 방지
    return m_webview->ExecuteScript(script,
        Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [](HRESULT, LPCWSTR) -> HRESULT { return S_OK; }).Get());
}

HRESULT CWebView2::PostWebMessageAsJson(LPCWSTR jsonStr)
{
    if (!m_webview) return E_FAIL;
    return m_webview->PostWebMessageAsJson(jsonStr);
}
