// ============================================================================
// WebView2App.cpp - MFC Application Class Implementation
// ============================================================================
#include "pch.h"
#include "WebView2App.h"
#include "WebView2Dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//#include <ShellScalingApi.h>
//#pragma comment(lib, "Shcore.lib")

// ============================================================================
// Global app object
// ============================================================================
CWebView2App theApp;

// ============================================================================
// Message Map
// ============================================================================
BEGIN_MESSAGE_MAP(CWebView2App, CWinApp)
END_MESSAGE_MAP()

// ============================================================================
// Constructor
// ============================================================================
CWebView2App::CWebView2App()
{
}

// ============================================================================
// InitInstance - MFC Dialog as main window
// ============================================================================
#include "LauncherDlg.h"

BOOL CWebView2App::InitInstance()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    CWinApp::InitInstance();

    // Initialize OLE/COM
    AfxOleInit();

    // Show Launcher Dialog
    CLauncherDlg launcherDlg;
    if (launcherDlg.DoModal() != IDOK) {
        return FALSE; // User cancelled
    }

    // ★ Modeless Dialog: DoModal() 대신 Create() + ShowWindow()
    //   DoModal()은 중첩 메시지 루프를 사용하여 WebView2와 충돌합니다.
    //   Modeless + MFC 기본 메시지 루프를 사용해야 WebView2가 정상 동작합니다.
    CWebView2Dlg* pDlg = new CWebView2Dlg(launcherDlg.m_nType, launcherDlg.m_nGridSubType);
    m_pMainWnd = pDlg;

    if (!pDlg->Create(IDD_MAIN_DIALOG, nullptr)) {
        AfxMessageBox(_T("Dialog creation failed."));
        return FALSE;
    }

    pDlg->ShowWindow(SW_SHOW);
    pDlg->UpdateWindow();

    // TRUE 반환 → MFC 기본 메시지 루프 실행 (WebView2에 필수)
    return TRUE;
}
