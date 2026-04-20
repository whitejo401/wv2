// ============================================================================
// WebView2Dlg.cpp
//
// CWebView2Host 사용 예제 - 다른 다이얼로그에서도 동일한 패턴으로 사용
//
// [최소 패턴 요약]
//   OnInitDialog:  m_wv2.Create(this, CRect(0,0,0,0));
//                  PostMessage(WM_INIT_WEBVIEW2);
//   OnInitWebView2: m_wv2.InitWebView2();
//                   m_wv2.Navigate(L"https://...");  // 또는 NavigateToString
//   OnSize:         m_wv2.Resize(cx, cy);
//   OnDestroy:      m_wv2.Close();
// ============================================================================
#include "pch.h"
#include "WebView2App.h"
#include "WebView2Dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// Message Map
// ============================================================================
BEGIN_MESSAGE_MAP(CWebView2Dlg, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_INIT_WEBVIEW2, &CWebView2Dlg::OnInitWebView2)
END_MESSAGE_MAP()

// ============================================================================
// Constructor
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

    // ★ WebView2 child 윈도우 생성 (WebView2 초기화는 PostMessage로 지연)
    CRect rcClient;
    GetClientRect(&rcClient);
    m_wv2.Create(rcClient, this, 10001);
    m_wv2.SetOnWebMessageReceivedCallback([this](const std::wstring& msg) {
        OnWebMessage(msg);
    });

    CString installPath = GetInstallPathFromRegistry();   // check registry for WebView2
    if (installPath.IsEmpty())
        installPath = GetInstallPathFromDisk();         // check disk for WebView2
    if (installPath.IsEmpty())
        installPath = GetInstallPathFromRegistry(false);// check registry for Edge

    BOOL bWebView2Installed = (installPath.GetLength() > 0);


    // Dialog가 완전히 그려진 후 WebView2 초기화
    PostMessage(WM_INIT_WEBVIEW2);

    return TRUE;
}

// ============================================================================
// OnInitWebView2 - PostMessage로 지연된 WebView2 초기화
// ============================================================================
LRESULT CWebView2Dlg::OnInitWebView2(WPARAM, LPARAM)
{
    // ★ WebView2 초기화 → 완료 후 페이지 로드
    m_wv2.InitWebView2();

    // InitWebView2는 비동기이므로
    // 완료 직후 내부에서 Navigate가 실행될 수 있도록
    // 초기 URL(가상 호스트)을 설정합니다.
    m_wv2.SetInitialUrl(L"http://app.local/index.html");

    return 0;
}

// ============================================================================
// OnWebMessage - 웹→호스트 메시지 수신 (IWebView2MessageHandler 구현)
// ============================================================================
void CWebView2Dlg::OnWebMessage(const std::wstring& message)
{
    CString strMsg;
    strMsg.Format(_T("[WebView2→Host] %s\n"), message.c_str());
    OutputDebugString(strMsg);

    // 웹 페이지 준비 신호 또는 추가 데이터 요청 시
    if (message.find(L"READY_FOR_DATA") == 0 || message.find(L"ADD_MORE_DATA") == 0)
    {
        GenerateAndSendLargeData(message.c_str());
    }
}

void CWebView2Dlg::GenerateAndSendLargeData(const CString& message)
{
    // "ADD_MORE_DATA:100" 형태에서 숫자 파싱
    int targetCount = 10;
    if (message.Find(_T("ADD_MORE_DATA:")) == 0) {
        int colonIdx = message.Find(_T(':'));
        if (colonIdx != -1) {
            _stscanf_s(message.GetString() + colonIdx + 1, _T("%d"), &targetCount);
        }
    } else if (message.Find(_T("READY_FOR_DATA")) == 0) {
        targetCount = 100; // 초기 로드 시 100개
    }

    // [핵심 해결책] 
    // WebView2의 ExecuteScript는 내부적으로 스크립트 문자열이 약 6~8KB(데이터 100개 언저리)를 넘어가면
    // 즉시 실행하지 않고 Low-Priority 큐에 지연 보관(Defer)하는 엔진 내부의 버그/최적화 특성이 있습니다.
    // 1000개를 요청하더라도 안전한 크기(예: 50개 단위)로 작게 쪼개어(Chunking) 연속으로 전송하면
    // 이 지연 큐잉 알고리즘을 완벽하게 우회하여 즉각적으로 화면에 렌더링될 수 있습니다.
    static int s_dataCount = 0;
    if (message.Find(_T("READY_FOR_DATA")) == 0) {
        s_dataCount = 0; // 초기화
    }

    int CHUNK_SIZE = 50;
    int sentCount = 0;

    while (sentCount < targetCount) {
        int currentChunk = min(CHUNK_SIZE, targetCount - sentCount);
        
        CString jsonStr = _T("[");
        for (int i = 1; i <= currentChunk; i++)
        {
            s_dataCount++;
            CString row;
            row.Format(_T("{\"id\":%d, \"name\":\"MFC C++ User %d\", \"age\":%d, \"col\":\"%s\"}"), 
                       s_dataCount, s_dataCount, 20 + (s_dataCount % 40), (s_dataCount % 2 == 0) ? _T("blue") : _T("red"));
            
            jsonStr += row;
            if (i < currentChunk) jsonStr += _T(",");
        }
        jsonStr += _T("]");

        CString script;
        if (message.Find(_T("READY_FOR_DATA")) == 0 && sentCount == 0) {
            script = _T("window.loadGridData(") + jsonStr + _T(");");
        } else {
            script = _T("window.appendGridData(") + jsonStr + _T(");");
        }

        m_wv2.ExecuteScript(script.GetString());
        
        sentCount += currentChunk;
    }
}

// ============================================================================
// OnSize
// ============================================================================
void CWebView2Dlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    // ★ WebView2 리사이즈
    if (m_wv2.GetSafeHwnd())
        m_wv2.MoveWindow(0, 0, cx, cy);
}

// ============================================================================
// OnGetMinMaxInfo
// ============================================================================
void CWebView2Dlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = 640;
    lpMMI->ptMinTrackSize.y = 480;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

// ============================================================================
// OnDestroy
// ============================================================================
void CWebView2Dlg::OnDestroy()
{
    // ★ 한 줄로 WebView2 정리
    m_wv2.Close();
    CDialogEx::OnDestroy();
}

// ============================================================================
// 모달리스 다이얼로그 라이프사이클 관리
// ============================================================================
void CWebView2Dlg::OnCancel()
{
    DestroyWindow();
}

void CWebView2Dlg::OnOK()
{
    DestroyWindow();
}

void CWebView2Dlg::PostNcDestroy()
{
    CDialogEx::PostNcDestroy();
    
    // 메인 윈도우가 파괴되면 앱 종료
    AfxGetApp()->m_pMainWnd = nullptr;
    PostQuitMessage(0);
    
    // InitInstance에서 new CWebView2Dlg()로 생성한 인스턴스 메모리 해제
    delete this;
}



CString CWebView2Dlg::GetInstallPathFromRegistry(bool const searchWebView)
{
    CString path;

    HKEY handle = nullptr;

    LSTATUS result = ERROR_FILE_NOT_FOUND;

    if (searchWebView)
    {
        result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Microsoft EdgeWebView",
            0,
            KEY_READ,
            &handle);

        if (result != ERROR_SUCCESS)
            result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Microsoft EdgeWebView",
                0,
                KEY_READ,
                &handle);
    }
    else
    {
        result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Microsoft Edge",
            0,
            KEY_READ,
            &handle);

        if (result != ERROR_SUCCESS)
            result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Microsoft Edge",
                0,
                KEY_READ,
                &handle);
    }

    if (result == ERROR_SUCCESS)
    {
        TCHAR buffer[MAX_PATH + 1]{ 0 };
        DWORD type = REG_SZ;
        DWORD size = MAX_PATH;
        result = RegQueryValueEx(handle, L"InstallLocation", 0, &type, reinterpret_cast<LPBYTE>(buffer), &size);

        if (result == ERROR_SUCCESS)
            path += CString{ buffer };

        TCHAR version[100]{ 0 };
        size = 100;
        result = RegQueryValueEx(handle, L"Version", 0, &type, reinterpret_cast<LPBYTE>(version), &size);
        if (result == ERROR_SUCCESS)
        {
            if (path.GetAt(path.GetLength() - 1) != '\\')
                path += "\\";
            path += CString{ version };
        }
        else
            path.Empty();

        RegCloseKey(handle);
    }

    return path;
}

CString CWebView2Dlg::GetInstallPathFromDisk(bool const searchWebView)
{
    CString path =
        searchWebView ?
        CA2W("c:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application\\") :
        CA2W("c:\\Program Files (x86)\\Microsoft\\Edge\\Application\\");
    CString pattern = path;
    pattern += "*";


    WIN32_FIND_DATA ffd{ 0 };
    HANDLE hFind = FindFirstFile(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        /* [[maybe_unused]] */DWORD error = ::GetLastError();
        return {};
    }

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            CString name{ ffd.cFileName };
            int a, b, c, d;
            if (4 == swscanf_s(ffd.cFileName, L"%d.%d.%d.%d", &a, &b, &c, &d))
            {
                FindClose(hFind);
                return path + name;
            }
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);

    return {};
}