// ============================================================================
// pch.h - Precompiled Header for MFC + WebView2
// ============================================================================
#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN    // Exclude rarely-used stuff from Windows headers
#endif

#include "targetver.h"

// MFC core headers
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC core and standard
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC OLE automation (AfxOleInit)
#include <afxdialogex.h>    // MFC Dialog Ex
#include <afxmt.h>          // CCriticalSection, CSingleLock

// Shell API (SHGetFolderPath)
#include <Shlobj.h>

// COM / WRL
#include <wrl.h>
#include <wil/com.h>

// C++ STL (WebView2 인프라 레이어 사용: EnvManager, CWebView2 래퍼)
#include <string>
#include <functional>
#include <vector>

// WebView2
#include "WebView2.h"
