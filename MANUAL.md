# MFC + WebView2 연동 가이드

본 문서는 MFC 애플리케이션에 WebView2를 안정적으로 연동하고 운영하기 위한 가이드입니다. 
기존의 스레드 충돌, 메모리 누수, 크래시 이슈를 해결하기 위해 개선된 아키텍처를 다루며, 새로운 윈도우에 웹뷰를 추가할 때 참고해야 할 표준 패턴을 제공합니다.

---

## 1. 아키텍처 개요

핵심 설계 사상은 **"Edge 렌더러 프로세스를 앱 전체에서 1개만 생성하여 공유한다"**는 점입니다. 다이얼로그마다 WebView2 프로세스를 독립적으로 띄우면 메모리 점유율이 급증하므로 싱글톤을 활용합니다.

- **`CWebView2EnvManager` (Singleton)**: 앱 전체에서 Edge 렌더러 프로세스를 1개만 생성하고 공유를 관리합니다.
- **`CWebView2` (CWnd 상속)**: 각 다이얼로그에 부착되는 웹뷰 컨트롤입니다. 화면(UI)과 JS 컨텍스트는 완전히 독립적으로 동작하지만, 내부 Environment는 공유합니다.
- **`CDataStreamManager`**: 실시간 데이터 스트리밍이 필요한 경우에만 사용하는 Worker Thread 매니저입니다. 데이터의 생산(Worker)과 소비(UI)를 철저히 분리합니다.

결과적으로, 웹뷰 창을 다수 생성해도 프로세스 오버헤드가 적으며, 창 간의 상호 간섭이 발생하지 않습니다.

---

## 2. 파일별 역할

| 모듈/파일 | 레이어 | 역할 및 특징 | 수정 시점 |
|---|---|---|---|
| `WebView2EnvManager.cpp` | 앱 인프라 | Environment 싱글톤. 단일 렌더러 프로세스 보장. | 거의 없음 |
| `WebView2.cpp` | UI 컨트롤 | `CWnd` 기반 WebView2 래퍼. | 신규 브라우저 API 필요 시 |
| `DataStreamManager.cpp` | 비즈니스 | Worker Thread 및 스레드 안전한 큐 캡슐화. | 생성되는 데이터 구조 변경 시 |
| `WebView2Dlg.cpp` | 애플리케이션 | 웹뷰 및 스트리밍을 구현한 템플릿 다이얼로그. | 신규 화면 개발 시 참고 |
| `assets/index.html` | 프론트엔드 | 웹 UI 렌더링 영역 (HTML/CSS/JS). | 화면 UI 변경 시 |

---

## 3. 신규 다이얼로그 적용 가이드

새로운 다이얼로그(`CMyDlg`)에 웹뷰를 부착하는 표준 절차입니다.

**1) 멤버 변수 선언**
헤더 파일에 컨트롤을 추가합니다.
```cpp
#include "WebView2.h"

class CMyDlg : public CDialogEx
{
    // ...
private:
    CWebView2 m_wv2;
};
```

**2) 컨트롤 생성 및 지연 초기화 (권장 패턴)**
초기화(`InitWebView2`)는 화면 렌더링이 안정화된 후 진행되도록 `PostMessage`를 이용해 지연 실행합니다.

```cpp
BOOL CMyDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 1. 웹뷰 윈도우 생성
    CRect rcClient;
    GetClientRect(&rcClient);
    m_wv2.Create(rcClient, this, 10001);

    // 2. JS → C++ 메시지 수신 콜백 등록
    m_wv2.SetOnWebMessageReceivedCallback([this](const std::wstring& msg) {
        // msg 파싱 및 처리 로직
    });

    // 3. 지연 초기화 시작
    PostMessage(WM_USER + 100);
    return TRUE;
}

// WM_USER + 100 메시지 핸들러
LRESULT CMyDlg::OnInitWebView2(WPARAM, LPARAM)
{
    m_wv2.InitWebView2();
    m_wv2.SetInitialUrl(L"http://app.local/index.html"); // 로컬/원격 페이지 로드
    return 0;
}
```

**3) 리사이즈 및 리소스 정리**
리사이즈 처리와 더불어 종료 시 명시적 리소스 해제가 필수입니다.

```cpp
void CMyDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (m_wv2.GetSafeHwnd())
        m_wv2.MoveWindow(0, 0, cx, cy);
}

void CMyDlg::OnDestroy()
{
    m_wv2.Close(); // 명시적 웹뷰 해제
    CDialogEx::OnDestroy();
}
```

---

## 4. C++ ↔ JS 통신 프로토콜

### 기본적인 통신 방법
- **C++ → JS**: 스크립트 문자열을 구성하여 UI 스레드에서 `ExecuteScript` 호출.
- **JS → C++**: 웹 환경에서 `window.chrome.webview.postMessage("명령어:페이로드")` 호출 시 C++의 콜백으로 수신.

### 현재 구현된 스트리밍 프로토콜 명세

**JS → C++ 명령 (postMessage)**
| 메시지 | 기능 설명 |
|---|---|
| `STREAM_START:100` | 100건/초 속도로 백그라운드 스트리밍 시작 |
| `STREAM_STOP` | 스트리밍 즉시 중지 |
| `STREAM_RATE:500` | 스레드 재시작 없이 속도만 즉시 변경 |
| `STREAM_START:-1` | Chaos 모드 (10~1000건/초 가변 랜덤 속도) |

**C++ → JS 명령 (ExecuteScript)**
| 함수 | 기능 설명 |
|---|---|
| `window.appendGridData([JSON배열]);` | 그리드에 JSON 배열 형식의 데이터를 일괄 추가 |

---

## 5. 실시간 데이터 스트리밍 처리 (`CDataStreamManager`)

초당 수백~수천 건의 데이터가 발생할 때 C++에서 건별로 JS를 호출하면 IPC 병목 및 UI 스레드 블로킹으로 앱이 멈춥니다. 이를 방지하기 위해 **생산자(Producer)** 와 **소비자(Consumer)** 패턴으로 철저히 분리합니다.

- **생산자 (Worker Thread)**: 지속적으로 데이터를 생성하여 내부 큐(Queue)에 적재. UI 접근은 절대 금지됩니다.
- **소비자 (UI Timer)**: 다이얼로그의 `OnTimer`에서 16ms(약 60FPS) 주기로 큐의 전체 데이터를 한 번에 꺼내어 배열(Batch)로 구성 후 전송합니다.

### DataStreamManager 제어 API
```cpp
m_stream.Start(100);        // 100건/초 스트리밍 스레드 구동
m_stream.SetRate(500);      // 속도 변경
m_stream.Stop();            // 스레드 종료 및 내부 큐 비우기
m_stream.DrainQueue(list);  // 큐의 모든 데이터를 list로 안전하게 이동 (OnTimer 전용)
m_stream.IsRunning();       // 동작 상태 확인
```

### 데이터 생산 패턴 (Worker Thread)
백그라운드 스레드에서는 `CCriticalSection`을 이용해 큐(Queue)에 데이터를 안전하게 추가합니다.
이 과정에서 UI 관련 객체(`CWnd`)나 `ExecuteScript`에는 절대 접근하지 않습니다.

```cpp
UINT CDataStreamManager::WorkerProc(LPVOID pParam)
{
    CDataStreamManager* pThis = (CDataStreamManager*)pParam;
    
    while (!pThis->m_bStop)
    {
        LONG id = InterlockedIncrement(&pThis->m_nCount); // 스레드 안전한 카운터 증가
        CString rowData = pThis->GenerateRow(id);
        
        {
            // 큐에 데이터를 넣을 때만 락(Lock)을 획득
            CSingleLock lock(&pThis->m_csQueue, TRUE);
            pThis->m_queue.AddTail(rowData);
        } // 스코프 종료 시 자동 해제(Unlock)

        Sleep(pThis->m_nSleepMs); // 설정된 속도에 따라 대기
    }
    return 0;
}
```

### 배치 처리 패턴 (OnTimer)
```cpp
void CMyDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == BATCH_TIMER_ID)
    {
        CStringList batch;
        m_stream.DrainQueue(batch);      // 1. 큐 데이터 일괄 회수

        if (!batch.IsEmpty())
        {
            // 2. JSON 배열 문자열 구성
            CString json = _T("[");
            BOOL bFirst = TRUE;
            while (!batch.IsEmpty())
            {
                if (!bFirst) json += _T(",");
                json += batch.RemoveHead();
                bFirst = FALSE;
            }
            json += _T("]");

            // 3. UI 스레드에서 안전하게 JS로 일괄 전송
            CString script = _T("window.appendGridData(") + json + _T(");");
            m_wv2.ExecuteScript(script.GetString());
        }
        return;
    }
    CDialogEx::OnTimer(nIDEvent);
}
```

### 데이터 구조 변경법
데이터 형식을 변경해야 할 경우 `DataStreamManager.cpp`의 `GenerateRow` 메서드를 수정합니다.
```cpp
CString CDataStreamManager::GenerateRow(LONG id)
{
    CString row;
    row.Format(_T("{\"id\":%d,\"name\":\"Item %d\",\"value\":%.2f}"), id, id, id * 1.5);
    return row;
}
```

---

## 6. 다중 창 운영 상세

### 리소스 에디터 활용 (Static Control에 붙이기)
다이얼로그의 특정 영역에 웹뷰를 고정하려면 Static Control(예: `IDC_WEB_PLACEHOLDER`)을 활용합니다.

```cpp
// OnInitDialog 내
CRect rc;
GetDlgItem(IDC_WEB_PLACEHOLDER)->GetWindowRect(&rc);
ScreenToClient(&rc);
m_wv2.Create(rc, this, IDC_WEB_PLACEHOLDER);

// OnSize 내
if (m_wv2.GetSafeHwnd() && GetDlgItem(IDC_WEB_PLACEHOLDER))
{
    CRect rc;
    GetDlgItem(IDC_WEB_PLACEHOLDER)->GetWindowRect(&rc);
    ScreenToClient(&rc);
    m_wv2.MoveWindow(rc);
}
```

### 앱 종료 시 자원 완전 정리 (선택)
`CWinApp` 상속 클래스에서 EnvManager를 명시적으로 정리할 수 있습니다.
```cpp
// App 클래스의 ExitInstance 오버라이드
int CWebView2App::ExitInstance()
{
    CWebView2EnvManager::Instance().Shutdown();
    return CWinApp::ExitInstance();
}
```

---

## 7. 핵심 설계 개념 및 C++ 기능 이해

### Timer와 Worker Thread를 분리하는 이유 3가지
1. **COM STA 제약**: WebView2 컨트롤은 생성된 스레드(UI 스레드)에서만 제어 가능합니다. Worker Thread에서 `ExecuteScript` 호출 시 애플리케이션이 크래시됩니다.
2. **IPC(프로세스 간 통신) 최적화**: 1건씩 1,000번 호출하는 것보다 배열로 묶어 1번 호출하는 것이 통신 부하를 극적으로 낮춥니다.
3. **렌더링 동기화**: 일반적인 모니터 주사율(60Hz, 약 16ms)에 맞춰 타이머를 동작시킴으로써 불필요한 JS 실행 오버헤드를 줄입니다.

### UI 스레드 vs Worker Thread
- **UI 스레드**: 프로그램 시작 시 자동 생성되며 윈도우 메시지 루프를 가집니다. (`OnTimer`, `OnSize`, `OnWebMessage` 등은 모두 이곳에서 실행됨)
- **Worker Thread**: 백그라운드 데이터 처리 전용으로 생성한 스레드로 윈도우 메시지를 받지 않으며, UI 객체(CWnd)에 직접 접근할 수 없습니다.

### 동기화 객체: Interlocked vs CriticalSection
멀티스레딩 환경에서 Race Condition(경합 조건)을 방지하기 위한 용도 분리입니다.
- **`InterlockedIncrement`**: 단순 정수 변수(카운트, 플래그 등)를 1 증가/감소시킬 때 CPU 수준에서 원자성(Atomic)을 보장하기 위해 사용합니다. 변수에는 컴파일러 최적화 방지를 위해 `volatile` 키워드를 동반합니다.
- **`CCriticalSection`**: `CStringList`나 복잡한 자료구조에 접근할 때는 반드시 락을 걸어야 합니다. `CSingleLock` 객체를 이용하면 스코프 종료 시 자동으로 락이 해제(RAII)되어 안전합니다.

### 람다(Lambda) 및 using 키워드
- **람다**: `[캡처](인자){본문}` 형태의 C++11 문법으로, 클래스 멤버 함수를 굳이 구현하지 않고 콜백 로직을 직관적으로 작성할 때 사용합니다.
- **using**: `typedef`의 모던 C++ 버전으로, 길고 복잡한 콜백 타입(예: `std::function`)을 `EnvReadyCallback`처럼 간결한 별칭으로 정의하여 가독성을 높입니다.

---

## 8. 🚨 필수 주의사항 (크래시 방지 룰)

1. **`ExecuteScript()`는 반드시 UI쓰레드에서만 호출** 
   Worker Thread에서 호출 시 COM STA 규칙 위반으로 앱이 즉시 크래시됩니다.
2. **`InitWebView2()`는 `Create()` 이후 호출 및 `PostMessage` 지연 실행 권장**
3. **종료 시 해제 순서 엄수**
   `OnDestroy`에서 스레드(`m_stream.Stop()`)를 먼저 종료해 큐 접근을 완전히 차단한 뒤, 웹뷰(`m_wv2.Close()`)를 닫아야 메모리/핸들 누수가 없습니다.
4. **멀티스레드 환경 변수 동기화 주의**
   공유 데이터 접근 시 반드시 `CCriticalSection` (리스트/큐) 또는 `Interlocked` (단순 정수)를 이용해 동기화를 수행해야 합니다.

---

## 9. 향후 계획 (Next Steps)

현재는 단일 `index.html` 기반으로 동작 중이나, 추후 프론트엔드 환경을 **React + TypeScript + Vite** 기반으로 고도화할 예정입니다.
이를 통해 C++과 JS 간의 메시지 프로토콜을 타입으로 강제하여 런타임 오류를 예방하고, 컴포넌트 재사용성을 높이며, C++ 앱 없이 웹 브라우저만으로 UI 독립 개발이 가능한 Mocking 구조를 도입할 계획입니다.
