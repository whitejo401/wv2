// ============================================================================
// WebView2EnvManager.cpp
// ============================================================================
#include "pch.h"
#include "WebView2EnvManager.h"
#include "WebView2EnvironmentOptions.h"

// ============================================================================
// 싱글톤 인스턴스
// ============================================================================
CWebView2EnvManager& CWebView2EnvManager::Instance()
{
    static CWebView2EnvManager s_instance;
    return s_instance;
}

// ============================================================================
// GetEnvironment
//   - 이미 생성됐으면  → 콜백 즉시 호출
//   - 생성 중이면      → 완료 대기열에 추가
//   - 아직 없으면      → 환경 생성 시작 + 대기열에 추가
// ============================================================================
void CWebView2EnvManager::GetEnvironment(EnvReadyCallback callback)
{
    // ① 이미 준비된 경우
    if (m_env)
    {
        callback(m_env.get());
        return;
    }

    // ② 대기열에 콜백 등록
    m_pendingCallbacks.push_back(std::move(callback));

    // ③ 이미 생성 중이면 대기만
    if (m_isCreating)
        return;

    m_isCreating = true;

    // User Data Folder (앱마다 고유한 폴더명 사용)
    wchar_t userDataPath[MAX_PATH] = {};
    if (SUCCEEDED(::SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, userDataPath)))
        wcscat_s(userDataPath, L"\\WebView2HelloWorld_Data");

    // ★ --disable-web-security 제거: 가상 호스트 매핑으로 CORS 문제 해결
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataPath, options.Get(),
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                m_isCreating = false;

                if (FAILED(result) || !env)
                {
                    TRACE(_T("[EnvManager] 환경 생성 실패: 0x%08X\n"), result);
                    m_pendingCallbacks.clear();
                    return result;
                }

                m_env = env;

                // 대기 중이던 모든 콜백 실행
                auto pending = std::move(m_pendingCallbacks);
                for (auto& cb : pending)
                    cb(m_env.get());

                return S_OK;
            }).Get());

    if (FAILED(hr))
    {
        m_isCreating = false;
        m_pendingCallbacks.clear();
        TRACE(_T("[EnvManager] CreateEnvironmentWithOptions 실패: 0x%08X\n"), hr);
    }
}

// ============================================================================
// Shutdown - 앱 종료 시 호출 (선택)
// ============================================================================
void CWebView2EnvManager::Shutdown()
{
    m_env = nullptr;
}
