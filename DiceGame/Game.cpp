// Game.cpp
#include "pch.h"
#include "Game.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Scene/SceneId.h"
#include "Scene/BaseScene/BaseScene.h"
#include "Scene/ModelTestScene/ModelTestScene.h"
#include "Scene/EditorScene/EditorScene.h"
// #include "Scene/GamePlayScene/GamePlayScene.h"
#include "Scene/TitleScene/TitleScene.h"
#include "Scene/SelectScene/SelectScene.h"
#include "AdvanceGamePlayScene.h"
#include "OriginalGamePlayScene.h"
#include "Scene/Lobby/LobbyScene.h"

extern void ExitGame() noexcept;

using namespace DirectX;
using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
    : m_keyboardTracker{}, m_mouseButtonTracker{}, m_states{}, m_debugRenderer{}, m_gameContext{}, m_sceneManager{}
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
    //  終了時に安全に強制切断してソケット破棄
    m_networkManager.ForceDisconnect();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Game::Initialize(HWND window, int width, int height)
{
    SoundManager::GetInstance().Initialize();

    m_networkManager.Initialize();

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_sceneManager.RegisterScene<BaseScene>(SceneId::BaseScene);
    m_sceneManager.RegisterScene<ModelTestScene>(SceneId::ModelTestScene);
    m_sceneManager.RegisterScene<EditorScene>(SceneId::EditorScene);
    m_sceneManager.RegisterScene<TitleScene>(SceneId::TitleScene);
    m_sceneManager.RegisterScene<SelectScene>(SceneId::SelectScene);
    m_sceneManager.RegisterScene<OriginalGamePlayScene>(SceneId::OriginalGamePlayScene);
    m_sceneManager.RegisterScene<AdvanceGamePlayScene>(SceneId::AdvanceGamePlayScene);
    m_sceneManager.RegisterScene<LobbyScene>(SceneId::LobbyScene);

    m_gameContext.emplace(m_timer, *m_deviceResources, m_keyboardTracker, m_mouseButtonTracker, *m_states,
                          *m_debugRenderer, m_networkManager);

    m_sceneManager.SetFirstScene(SceneId::TitleScene, *m_gameContext);
}

#pragma region Frame Update
void Game::Tick()
{
    m_timer.Tick([&]() { Update(m_timer); });

    Render();
}

void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());
    elapsedTime;

    auto keyboard = Keyboard::Get().GetState();
    m_keyboardTracker.Update(keyboard);

    auto mouse = Mouse::Get().GetState();
    m_mouseButtonTracker.Update(mouse);

    m_networkManager.Update();

    m_sceneManager.Update(*m_gameContext);
}
#pragma endregion

#pragma region Frame Render
void Game::Render()
{
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    m_sceneManager.Render(*m_gameContext);

    m_debugRenderer->Render(context, m_states.get());

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    m_deviceResources->PIXEndEvent();

    m_deviceResources->Present();
}

void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    const auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}
#pragma endregion

#pragma region Message Handlers
void Game::OnActivated()
{
}
void Game::OnDeactivated()
{
}
void Game::OnSuspending()
{
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();
}

void Game::OnWindowMoved()
{
    const auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    m_debugRenderer =
        std::make_unique<Imase::DebugRenderer>(device, context, L"Resources/Font/JapaneseFont_18.spritefont");

    m_states = std::make_unique<CommonStates>(device);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

  
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF("Resources/Font/LightNovelPOPv2.otf", 20.0f, nullptr,
                                                io.Fonts->GetGlyphRangesJapanese());
    if (!font)
    {
        // 読み込めなかった場合のフォールバック（メイリオ）
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 18.0f, nullptr,
                                     io.Fonts->GetGlyphRangesJapanese());
    }

    ImGui_ImplWin32_Init(m_deviceResources->GetWindow());
    ImGui_ImplDX11_Init(device, context);
}
void Game::CreateWindowSizeDependentResources()
{
}

void Game::OnDeviceLost()
{
    m_debugRenderer.reset();
    m_states.reset();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
    m_sceneManager.OnDeviceRestored(*m_gameContext);
}

void Game::OnFileDropped(const std::wstring& filePath)
{
    if (!m_gameContext.has_value())
        return;

    m_sceneManager.OnFileDropped(*m_gameContext, filePath);
}
#pragma endregion