// ============================================================================
// WebView2App.h - MFC Application Class
// ============================================================================
#pragma once

#include "resource.h"

class CWebView2App : public CWinApp
{
public:
    CWebView2App();

    virtual BOOL InitInstance() override;

    DECLARE_MESSAGE_MAP()
};

extern CWebView2App theApp;
