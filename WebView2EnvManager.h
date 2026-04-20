// ============================================================================
// WebView2EnvManager.h
//
// WebView2 Environment 싱글톤 관리자
//
// [목적]
//   CreateCoreWebView2EnvironmentWithOptions는 Edge 렌더러 프로세스를 1개 생성하는
//   매우 무거운 작업입니다. CWebView2 인스턴스마다 호출하면 창 N개 = 프로세스 N개.
//   이 클래스는 앱 전체에서 Environment를 1개만 생성하여 공유합니다.
//   (컨트롤러는 각 CWebView2가 독립적으로 생성 → JS 컨텍스트 완전 격리 유지)
//
// [사용법]
//   CWebView2EnvManager::Instance().GetEnvironment([](ICoreWebView2Environment* env) {
//       // env로 Controller 생성
//   });
// ============================================================================
#pragma once

#include <functional>
#include <vector>
#include <wrl.h>
#include <wil/com.h>

// ICoreWebView2Environment 타입 전방 참조
struct ICoreWebView2Environment;

class CWebView2EnvManager
{
public:
    static CWebView2EnvManager& Instance();

    // 환경이 준비되면 콜백을 즉시 호출, 준비 중이면 완료 후 모든 대기 콜백 일괄 호출
    using EnvReadyCallback = std::function<void(ICoreWebView2Environment*)>;
    void GetEnvironment(EnvReadyCallback callback);

    // 앱 종료 시 명시적 정리 (선택)
    void Shutdown();

private:
    CWebView2EnvManager()  = default;
    ~CWebView2EnvManager() = default;

    // 복사/이동 금지
    CWebView2EnvManager(const CWebView2EnvManager&)            = delete;
    CWebView2EnvManager& operator=(const CWebView2EnvManager&) = delete;

    wil::com_ptr<ICoreWebView2Environment> m_env;
    bool                                   m_isCreating { false };
    std::vector<EnvReadyCallback>          m_pendingCallbacks;
};
