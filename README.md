# WebView2 Hello World (MFC Dialog)

Microsoft 공식 "[Win32 앱에서 WebView2 시작하기](https://learn.microsoft.com/ko-kr/microsoft-edge/webview2/get-started/win32)" 가이드를 기반으로 한 **MFC Dialog + WebView2** Hello World 프로젝트입니다.

## 📋 필수 조건

| 항목 | 설명 |
|------|------|
| **OS** | Windows 10 (1809+) 또는 Windows 11 |
| **IDE** | Visual Studio 2019/2022 (`Desktop development with C++` + MFC 워크로드) |
| **CMake** | 3.20 이상 (VS와 함께 설치됨) |
| **Edge Runtime** | WebView2 Runtime (Windows 10/11에 기본 설치) |

> **NuGet 패키지 (WebView2 SDK, WIL)는 CMake 빌드 시 자동으로 다운로드됩니다.**

## 🚀 빌드 & 실행

```powershell
# 1. build 폴더 생성
mkdir build
cd build

# 2. CMake 프로젝트 생성 (Visual Studio 2022 기준)
cmake .. -G "Visual Studio 17 2022" -A x64

# 3. 빌드
cmake --build . --config Release

# 4. 실행
.\Release\WebView2HelloWorld.exe
```

## 📁 프로젝트 구조

```
wv2/
├── CMakeLists.txt           # CMake 빌드 설정 (MFC + NuGet 자동 다운로드)
├── pch.h / pch.cpp          # Precompiled Header (MFC + WRL + WebView2)
├── targetver.h              # Windows SDK 타겟 버전
├── resource.h               # 리소스 ID 정의
├── WebView2HelloWorld.rc    # MFC 다이얼로그 리소스 (.rc)
├── WebView2App.h / .cpp     # CWinApp 파생 클래스 (앱 진입점)
├── WebView2Dlg.h / .cpp     # CDialogEx 파생 클래스 (WebView2 호스트)
└── README.md
```

## 🏗️ 아키텍처

```
┌──────────────────────────────────────────────────┐
│          CWebView2App (CWinApp)                  │
│  ┌────────────────────────────────────────────┐  │
│  │      CWebView2Dlg (CDialogEx)             │  │
│  │  ┌──────────────────────────────────────┐  │  │
│  │  │     ICoreWebView2Controller          │  │  │
│  │  │  ┌────────────────────────────────┐  │  │  │
│  │  │  │                                │  │  │  │
│  │  │  │   HTML/CSS/JS Content          │  │  │  │
│  │  │  │  (NavigateToString 로드)        │  │  │  │
│  │  │  │                                │  │  │  │
│  │  │  │  chrome.webview.postMessage()  │  │  │  │
│  │  │  │         ↕ Host 통신             │  │  │  │
│  │  │  └────────────────────────────────┘  │  │  │
│  │  └──────────────────────────────────────┘  │  │
│  └────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────┘
```

## 📚 참고 자료

- [MS 공식 가이드 (한국어)](https://learn.microsoft.com/ko-kr/microsoft-edge/webview2/get-started/win32)
- [MS 공식 가이드 (영어)](https://learn.microsoft.com/en-us/microsoft-edge/webview2/get-started/win32)
- [MFC 공식 샘플 코드](https://github.com/MicrosoftEdge/WebView2Samples/tree/main/SampleApps/WebView2APISample)
- [WebView2 API Reference](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/)
