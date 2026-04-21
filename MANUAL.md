# MFC + WebView2 통합 개발 매뉴얼

> **대상**: 이 프로젝트를 참고하여 실제 MFC 애플리케이션에 WebView2를 통합하려는 개발자  
> **기준 코드**: `d:\prosrc\AI\wv2`

---

## 목차

1. [전체 아키텍처](#1-전체-아키텍처)
2. [파일별 역할](#2-파일별-역할)
3. [빠른 시작 - 다른 다이얼로그에 붙이기](#3-빠른-시작)
4. [JS 와 C++ 통신 프로토콜](#4-js-와-c-통신-프로토콜)
5. [실시간 스트리밍 DataStreamManager](#5-실시간-스트리밍)
6. [다중 창 운영](#6-다중-창-운영)
7. [핵심 설계 개념](#7-핵심-설계-개념)
8. [반드시 지켜야 할 규칙](#8-반드시-지켜야-할-규칙)
9. [향후 확장 방안](#9-향후-확장-방안)

---

## 1. 전체 아키텍처

```
앱 전체 공유 (Singleton)
  CWebView2EnvManager
   - Edge 렌더러 프로세스 1개 생성 및 공유
   - 창 N개 = Edge 프로세스 1개 (메모리 절약)

각 윈도우/다이얼로그 (독립 인스턴스)
  CWebView2  (CWnd 상속)
   - Environment 싱글톤 공유, Controller는 각자 생성
   - JS 컨텍스트 완전 격리 (창 간 간섭 없음)
   - Resize / Navigate / ExecuteScript / 메시지 수신

선택 - 실시간 스트리밍이 필요한 다이얼로그만 사용
  CDataStreamManager
   - AfxBeginThread 기반 Worker Thread
   - CCriticalSection + CStringList 큐
   - DrainQueue() → OnTimer → ExecuteScript 배치 전송
```

### 데이터 흐름 (스트리밍 시)

```
[Worker Thread]                        [UI Thread]
  WorkerProc()                           WM_TIMER (16ms)

  GenerateRow(id)                        OnTimer()
  CSingleLock lock         ──→ 큐 ──→     m_stream.DrainQueue()
  m_queue.AddTail()                       JSON 조립
  Sleep(sleepMs)                          ExecuteScript() ──→ JS

  (빠른 생산, UI 무관)                  (안전한 소비, UI 스레드)
```

---

## 2. 파일별 역할

| 파일 | 레이어 | 역할 | 수정 필요성 |
|------|--------|------|-----------|
| `WebView2EnvManager.h/cpp` | 앱 인프라 | Environment 싱글톤 | 거의 없음 |
| `WebView2.h/cpp` | UI 컨트롤 | CWnd 기반 WebView2 래퍼 | 기능 추가 시 |
| `DataStreamManager.h/cpp` | 비즈니스 | Worker Thread + 큐 캡슐화 | 데이터 구조 변경 시 |
| `WebView2Dlg.h/cpp` | 애플리케이션 | 예제 다이얼로그 (템플릿) | 새 다이얼로그 작성 시 참고 |
| `pch.h` | 빌드 | 사전 컴파일 헤더 | 신규 라이브러리 추가 시 |
| `CMakeLists.txt` | 빌드 | 소스 파일 목록 + 의존성 | 새 파일 추가 시 |
| `assets/index.html` | 프론트엔드 | 웹 UI (HTML/CSS/JS) | 화면 변경 시 |

---

## 3. 빠른 시작

### 단계 1: 헤더에 멤버 선언

```cpp
// MyDlg.h
#include "WebView2.h"              // CWebView2 컨트롤
// #include "DataStreamManager.h" // 스트리밍이 필요할 때만 추가

class CMyDlg : public CDialogEx
{
private:
    CWebView2 m_wv2;               // WebView2 컨트롤

    static constexpr UINT_PTR BATCH_TIMER_ID = 1;
    static constexpr UINT     BATCH_TIMER_MS = 16;  // 약 60fps
};
```

### 단계 2: OnInitDialog 구현

```cpp
BOOL CMyDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 1. WebView2 자식 윈도우 생성
    CRect rcClient;
    GetClientRect(&rcClient);
    m_wv2.Create(rcClient, this, 10001);

    // 2. JS→C++ 메시지 수신 콜백 등록
    m_wv2.SetOnWebMessageReceivedCallback([this](const std::wstring& msg) {
        OnWebMessage(CString(msg.c_str()));
    });

    // 3. 초기화 지연 실행 (다이얼로그가 완전히 그려진 후)
    PostMessage(WM_USER + 100);
    return TRUE;
}

LRESULT CMyDlg::OnInitWebView2(WPARAM, LPARAM)
{
    m_wv2.InitWebView2();
    m_wv2.SetInitialUrl(L"http://app.local/index.html");
    return 0;
}
```

### 단계 3: 리사이즈 및 종료 처리

```cpp
void CMyDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (m_wv2.GetSafeHwnd())
        m_wv2.MoveWindow(0, 0, cx, cy);
}

void CMyDlg::OnDestroy()
{
    m_wv2.Close();
    CDialogEx::OnDestroy();
}
```

### 단계 4: CMakeLists.txt에 새 파일 등록

```cmake
set(SOURCES  ...  MyDlg.cpp )
set(HEADERS  ...  MyDlg.h   )
```

---

## 4. JS 와 C++ 통신 프로토콜

### C++ 에서 JS 로 (ExecuteScript)

```cpp
// 반드시 UI 스레드에서 호출
CString script = _T("window.myFunction(") + jsonData + _T(");");
m_wv2.ExecuteScript(script.GetString());
```

### JS 에서 C++ 로 (postMessage)

```javascript
// JS 쪽에서 전송
window.chrome.webview.postMessage("MY_COMMAND:payload");
```

```cpp
// C++ 쪽에서 수신
void CMyDlg::OnWebMessage(const CString& message)
{
    if (message.Find(_T("MY_COMMAND:")) == 0)
    {
        CString payload = message.Mid((int)_tcslen(_T("MY_COMMAND:")));
    }
}
```

### 현재 예제의 스트리밍 프로토콜

**JS → C++ 메시지**

| 메시지 | 의미 |
|--------|------|
| `STREAM_START:100` | 100건/초로 스트리밍 시작 |
| `STREAM_STOP` | 스트리밍 중지 |
| `STREAM_RATE:500` | 속도 즉시 변경 (재시작 없음) |
| `STREAM_START:-1` | Chaos 모드 (10~1000건/초 랜덤) |

**C++ → JS 함수**

| 함수 | 의미 |
|------|------|
| `window.appendGridData([...])` | 그리드에 데이터 행 배치 추가 |

---

## 5. 실시간 스트리밍

### DataStreamManager API

```cpp
m_stream.Start(100);        // 100건/초 Worker Thread 시작
m_stream.SetRate(500);      // 속도 즉시 변경 (Thread 재시작 없음)
m_stream.Stop();            // Thread 종료 + 큐 비우기
m_stream.DrainQueue(list);  // 큐 전체를 list로 이동 (OnTimer에서 호출)
m_stream.IsRunning();       // 실행 중 여부 확인
```

### OnTimer 패턴 (반드시 이 패턴 사용)

```cpp
void CMyDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == BATCH_TIMER_ID)
    {
        CStringList batch;
        m_stream.DrainQueue(batch);      // 큐 전체 수거

        if (!batch.IsEmpty())
        {
            CString json = _T("[");
            BOOL bFirst = TRUE;
            while (!batch.IsEmpty())
            {
                if (!bFirst) json += _T(",");
                json += batch.RemoveHead();
                bFirst = FALSE;
            }
            json += _T("]");

            CString script = _T("window.appendGridData(") + json + _T(");");
            m_wv2.ExecuteScript(script.GetString());  // UI 스레드 안전
        }
        return;
    }
    CDialogEx::OnTimer(nIDEvent);
}
```

### GenerateRow 데이터 구조 변경

`DataStreamManager.cpp`의 `GenerateRow()` 함수를 수정합니다.

```cpp
CString CDataStreamManager::GenerateRow(LONG id)
{
    CString row;
    row.Format(
        _T("{\"id\":%d,\"name\":\"User %d\",\"value\":%.2f}"),
        id, id, id * 1.5);
    return row;
}
```

---

## 6. 다중 창 운영

### Environment 자동 공유

각 다이얼로그에서 `InitWebView2()`만 호출하면 자동으로 공유됩니다.

```
창 A: m_wv2.InitWebView2() → EnvManager → 환경 생성 (최초 1회)
창 B: m_wv2.InitWebView2() → EnvManager → 기존 환경 재사용
창 C: m_wv2.InitWebView2() → EnvManager → 기존 환경 재사용

Edge 렌더러 프로세스: 항상 1개
각 창의 JS 컨텍스트: 완전히 독립적
```

### 앱 종료 시 정리 (선택)

```cpp
// WebView2App.cpp
int CWebView2App::ExitInstance()
{
    CWebView2EnvManager::Instance().Shutdown();
    return CWinApp::ExitInstance();
}
```

### 리소스 에디터 Static Control에 붙이기

```cpp
// IDC_WEB_PLACEHOLDER = 리소스 에디터에서 배치한 Static Control ID
CRect rc;
GetDlgItem(IDC_WEB_PLACEHOLDER)->GetWindowRect(&rc);
ScreenToClient(&rc);
m_wv2.Create(rc, this, IDC_WEB_PLACEHOLDER);

void CMyDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (m_wv2.GetSafeHwnd() && GetDlgItem(IDC_WEB_PLACEHOLDER))
    {
        CRect rc;
        GetDlgItem(IDC_WEB_PLACEHOLDER)->GetWindowRect(&rc);
        ScreenToClient(&rc);
        m_wv2.MoveWindow(rc);
    }
}
```

---

## 7. 핵심 설계 개념

### Timer와 Worker Thread를 함께 쓰는 이유

```
Worker Thread = 데이터 생산자 (큐에 넣기만)
UI Thread Timer = 데이터 전달자 (큐에서 꺼내 JS로 전송)

이유 3가지:
1. COM STA 규칙
   WebView2는 생성한 스레드(UI 스레드)에서만 호출 가능
   Worker Thread에서 ExecuteScript 직접 호출 → 즉시 크래시

2. IPC 최적화
   1건씩 N번 호출 → 프로세스 간 통신 N번 (느림)
   N건 배치 1번   → 프로세스 간 통신 1번 (빠름)

3. 렌더 동기화
   모니터 60Hz = 16ms 주기 → Timer 16ms로 프레임에 정렬
```

### UI 스레드란

```
프로그램 시작 시 자동 생성되는 기본 스레드.
모든 윈도우 메시지를 처리하는 메시지 루프를 가짐.

자동 호출되는 것 (UI 스레드):
  OnTimer(), OnSize(), OnDestroy(), OnWebMessage()

직접 만든 별도 스레드 (Worker Thread):
  WorkerProc() → 윈도우 메시지 처리 안 함, UI 객체 접근 금지
```

### 원자적(Atomic) 연산

```
일반 ++ 연산은 내부적으로 3단계:
  1. READ  → 메모리에서 읽기
  2. ADD   → +1 계산
  3. WRITE → 메모리에 쓰기

두 스레드가 동시에 실행하면 결과가 틀림 → Race Condition

InterlockedIncrement: CPU 버스 수준에서 3단계를 원자적으로 처리
volatile: 컴파일러가 레지스터에 캐싱하지 못하도록 강제

둘 다 필요:
  volatile    = 컴파일러 최적화 방지
  Interlocked = CPU 하드웨어 수준 원자성 보장
```

### CCriticalSection vs InterlockedXxx

| 상황 | 사용 |
|------|------|
| 정수 1개 증가/감소/교체 | `InterlockedIncrement / Exchange` |
| 자료구조(CStringList) 또는 복수 줄 코드 보호 | `CCriticalSection + CSingleLock` |

```cpp
LONG id = InterlockedIncrement(&m_nCount);  // 정수 1개 → Interlocked
{
    CSingleLock lock(&m_csQueue, TRUE);      // 자료구조 → CriticalSection
    m_queue.AddTail(GenerateRow(id));
}  // 블록 종료 시 자동 해제 (RAII)
```

### 람다(Lambda)

```
형식: [캡처 목록](인자) { 본문 }

[this, hWndHost]                  → 바깥 변수를 안으로 가져옴 (캡처)
(ICoreWebView2Environment* env)   → 호출될 때 외부에서 받는 값
{ ... }                           → 실행할 코드

람다        = C++ 11 언어 문법. std:: 가 아님. MFC에서도 자유롭게 사용 가능.
std::function = 람다를 멤버변수에 저장할 때 필요한 STL 컨테이너.
```

### using 타입 별명

```cpp
// 긴 타입 이름에 짧은 별명 부여
using EnvReadyCallback = std::function<void(ICoreWebView2Environment*)>;

// 이후 EnvReadyCallback을 타입처럼 사용
void GetEnvironment(EnvReadyCallback callback);
```

---

## 8. 반드시 지켜야 할 규칙

| # | 규칙 | 위반 시 결과 |
|---|------|------------|
| 1 | `InitWebView2()`는 `Create()` 이후에 호출 | Controller 생성 실패 |
| 2 | `InitWebView2()`는 `PostMessage`로 지연 실행 | 초기화 타이밍 오류 |
| 3 | `ExecuteScript()`는 UI 스레드에서만 호출 | 크래시 (COM STA 위반) |
| 4 | Worker Thread에서 `m_wv2` 직접 접근 금지 | 크래시 |
| 5 | `OnDestroy()`에서 `m_stream.Stop()` 먼저, `m_wv2.Close()` 나중 | 메모리 누수 |
| 6 | `CWinThread`는 `m_bAutoDelete = FALSE` 후 `WaitForSingleObject` | 핸들 누수 |

---

## 9. 향후 확장 방안

### 웹 코드 React + TypeScript 전환

```
현재: assets/index.html (HTML + CSS + JS 단일 파일)
권장: webui/ (React + TypeScript + Vite 별도 프로젝트)

장점:
  - 타입 안전성: JS-C++ 메시지 프로토콜을 컴파일 타임에 검증
  - 컴포넌트 재사용: 여러 화면에서 같은 컴포넌트 활용
  - Mock 모드: C++ 없이 브라우저에서만 UI 개발 가능
  - 번들링: 오프라인 동작, Tree-shaking으로 용량 최적화

메시지 타입 정의 예시 (webview2.d.ts):
  export type HostMessage =
    | { type: 'STREAM_START'; rate: number }
    | { type: 'STREAM_STOP' }
    | { type: 'STREAM_RATE'; rate: number };
```

### C++ 추가 개선 방향

```
현재 완료:
  [v] CWebView2EnvManager - Environment 싱글톤
  [v] CDataStreamManager  - Worker Thread 분리
  [v] CWebView2           - 재사용 가능한 컨트롤
  [v] --disable-web-security 제거

추가 가능:
  [ ] CWebView2Bridge     - JS/C++ 메시지 타입 안전 래퍼
  [ ] ExitInstance에서 EnvManager::Shutdown() 호출
  [ ] ExecuteScript 결과 비동기 수신 (필요 시)
```

---

*최종 업데이트: 2026-04-22*
