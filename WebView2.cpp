// ============================================================================
// WebView2.cpp
//
// MFC CWnd를 상속받은 재사용 가능한 WebView2 컨트롤 구현
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
CWebView2::CWebView2()
{
    m_isInitializing = false;
}

CWebView2::~CWebView2()
{
    Close();
}

// ============================================================================
// Create - 윈도우 생성
// ============================================================================
BOOL CWebView2::Create(const RECT& rect, CWnd* pParentWnd, UINT nID)
{
    // 클래스 등록 (최초 1회)
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASS wc = {};
        wc.lpfnWndProc   = ::DefWindowProc;
        wc.hInstance     = AfxGetInstanceHandle();
        wc.lpszClassName = kWebView2CtrlClass;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClass(&wc);
        s_registered = true;
    }

    // CWnd 생성 (WS_CLIPCHILDREN 필수)
    return CWnd::CreateEx(
        0,
        kWebView2CtrlClass,
        _T(""),
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        rect,
        pParentWnd,
        nID
    );
}

// ============================================================================
// WM_CREATE 핸들러
// ============================================================================
int CWebView2::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    // 생성 직후 비동기 초기화를 시작하려면 여기서 PostMessage 등을 보낼 수 있으나,
    // 명시적인 InitWebView2() 호출을 권장합니다.
    return 0;
}

// ============================================================================
// InitWebView2 - 실제 WebView2 초기화 수행
// ============================================================================
void CWebView2::InitWebView2()
{
    if (m_isInitializing || m_webviewController != nullptr) {
        return; // 이미 초기화 중이거나 완료됨
    }

    HWND hWndHost = GetSafeHwnd();
    if (!::IsWindow(hWndHost)) {
        TRACE(_T("[CWebView2] InitWebView2: HWND is invalid. Call Create() first.\n"));
        return;
    }

    m_isInitializing = true;

    // User Data Folder 설정 (로컬 AppData 사용 권장)
    wchar_t userDataPath[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, userDataPath))) {
        wcscat_s(userDataPath, L"\\WebView2HelloWorld_Data"); // 앱마다 고유한 폴더명 사용 필수
    }

    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(L"--no-sandbox --disable-web-security");

    using namespace Microsoft::WRL;

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataPath, options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hWndHost](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {

                if (FAILED(result) || !env) {
                    m_isInitializing = false;
                    TRACE(_T("[CWebView2] Failed to create WebView2 Environment. 0x%08X\n"), result);
                    return result;
                }

                if (!::IsWindow(hWndHost)) {
                    m_isInitializing = false;
                    return E_ABORT;
                }

                env->CreateCoreWebView2Controller(hWndHost,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, hWndHost](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            
                            m_isInitializing = false;

                            if (FAILED(result) || !controller) {
                                TRACE(_T("[CWebView2] Controller failed. 0x%08X\n"), result);
                                return result;
                            }

                            m_webviewController = controller;
                            m_webviewController->get_CoreWebView2(&m_webview);

                            // --- WebView2 Settings ---
                            wil::com_ptr<ICoreWebView2Settings> settings;
                            m_webview->get_Settings(&settings);
                            if (settings) {
                                settings->put_IsScriptEnabled(TRUE);
                                settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                settings->put_IsWebMessageEnabled(TRUE);
                                settings->put_IsStatusBarEnabled(FALSE);
                            }

                            // Controller Bounds 설정
                            CRect rc;
                            ::GetClientRect(hWndHost, &rc);
                            m_webviewController->put_Bounds(rc);
                            m_webviewController->put_IsVisible(TRUE);

                            // 메시지 라우팅 설정
                            EventRegistrationToken token;
                            m_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        wil::unique_cotaskmem_string msg;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&msg))) {
                                            if (m_onWebMessage) {
                                                m_onWebMessage(msg.get());
                                            }
                                        }
                                        return S_OK;
                                    }).Get(), &token);

                            // 가상 폴더 매핑 설정 (옵션)
                            wil::com_ptr<ICoreWebView2_3> webview3;
                            if (SUCCEEDED(m_webview->QueryInterface(IID_PPV_ARGS(&webview3)))) {
                                wchar_t modulePath[MAX_PATH];
                                ::GetModuleFileNameW(NULL, modulePath, MAX_PATH);
                                std::wstring exePath(modulePath);
                                size_t pos = exePath.find_last_of(L"\\/");
                                std::wstring exeDir = exePath.substr(0, pos);
                                
                                std::wstring assetsPath = exeDir + L"\\assets";
                                if (::GetFileAttributesW(assetsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                                    assetsPath = exeDir + L"\\..\\..\\assets";
                                }

                                webview3->SetVirtualHostNameToFolderMapping(
                                    L"app.local",
                                    assetsPath.c_str(),
                                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
                                );
                            }

                            // 예약된 페이지 로드
                            if (!m_initialHtml.empty()) {
                                m_webview->NavigateToString(m_initialHtml.c_str());
                            } else if (!m_initialUrl.empty()) {
                                m_webview->Navigate(m_initialUrl.c_str());
                            }

                            // 초기화 완료 콜백 호출
                            if (m_onInitComplete) {
                                m_onInitComplete();
                            }

                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

// ============================================================================
// 리사이즈 (WM_SIZE)
// ============================================================================
void CWebView2::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);

    if (m_webviewController) {
        CRect rc(0, 0, cx, cy);
        m_webviewController->put_Bounds(rc);
    }
}

// ============================================================================
// 폐기 (WM_DESTROY)
// ============================================================================
void CWebView2::OnDestroy()
{
    Close();
    CWnd::OnDestroy();
}

void CWebView2::Close()
{
    if (m_webviewController) {
        m_webviewController->Close();
        m_webviewController = nullptr;
    }
    m_webview = nullptr;
}

// ============================================================================
// 브라우징 메서드들
// ============================================================================
HRESULT CWebView2::Navigate(const std::wstring& url)
{
    if (!m_webview) return E_FAIL;
    return m_webview->Navigate(url.c_str());
}

HRESULT CWebView2::NavigateToString(const std::wstring& html)
{
    if (!m_webview) return E_FAIL;
    return m_webview->NavigateToString(html.c_str());
}

HRESULT CWebView2::ExecuteScript(const std::wstring& script)
{
    if (!m_webview) return E_FAIL;
    
    // 강제 IPC Flush를 위해 빈 콜백(Dummy Handler)을 제공합니다.
    // nullptr을 전달하면 WebView2가 스크립트 크기에 따라 Background low-priority 큐에 집어넣어버리는 버그(지연 현상)를 방지합니다.
    return m_webview->ExecuteScript(script.c_str(), 
        Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                return S_OK;
            }).Get());
}

HRESULT CWebView2::PostWebMessageAsJson(const std::wstring& jsonStr)
{
    if (!m_webview) return E_FAIL;
    return m_webview->PostWebMessageAsJson(jsonStr.c_str());
}
