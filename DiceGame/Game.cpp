//// Game.cpp
#include "pch.h"
#include "Game.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Scene/SceneId.h"
#include "Scene/BaseScene/BaseScene.h"
#include "Scene/ModelTestScene/ModelTestScene.h"
#include "Scene/EditorScene/EditorScene.h"
//#include "Scene/GamePlayScene/GamePlayScene.h"
#include "Scene/TitleScene/TitleScene.h"
#include"Scene/SelectScene/SelectScene.h"
#include "Scene/GamePlayScene/AdvanceGamePlayScene.h"
#include "Scene/GamePlayScene/OriginalGamePlayScene.h"

extern void ExitGame() noexcept;

using namespace DirectX;
using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
    : m_keyboardTracker{}
    , m_mouseButtonTracker{}
    , m_states{}
    , m_debugRenderer{}
    , m_gameContext{}
    , m_sceneManager{}
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    // TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
    //   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
    //   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*    m_timer.SetFixedTimeStep(true);    m_timer.SetTargetElapsedSeconds(1.0 / 60);    */

    // シーンの登録
    m_sceneManager.RegisterScene<BaseScene>(SceneId::BaseScene);
    m_sceneManager.RegisterScene<ModelTestScene>(SceneId::ModelTestScene);
    m_sceneManager.RegisterScene<EditorScene>(SceneId::EditorScene);
    m_sceneManager.RegisterScene<TitleScene>(SceneId::TitleScene);
    m_sceneManager.RegisterScene<SelectScene>(SceneId::SelectScene);
    //m_sceneManager.RegisterScene<GamePlayScene>(SceneId::GamePlayScene);
    m_sceneManager.RegisterScene<OriginalGamePlayScene>(SceneId::OriginalGamePlayScene);
    m_sceneManager.RegisterScene<AdvanceGamePlayScene>(SceneId::AdvanceGamePlayScene);

    // ゲームコンテキストの設定
    m_gameContext.emplace(
        m_timer,                 // <- StepTimer
        *m_deviceResources,      // <- DeviceResources
        m_keyboardTracker,       // <- KeyboardTracker
        m_mouseButtonTracker,    // <- MouseButtonTracker
        *m_states,               // <- CommonStates
        *m_debugRenderer         // <- DebugRenderer
    );

    // 起動シーンの設定
    m_sceneManager.SetFirstScene(SceneId::EditorScene, *m_gameContext);
}

#pragma region Frame Update

// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    elapsedTime;

    // キーボードトラッカーの更新
    auto keyboard = Keyboard::Get().GetState();
    m_keyboardTracker.Update(keyboard);

    // マウスボタントラッカーの更新
    auto mouse = Mouse::Get().GetState();
    m_mouseButtonTracker.Update(mouse);

    // シーンの更新
    m_sceneManager.Update(*m_gameContext);
}

#pragma endregion

#pragma region Frame Render

// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();

    // TODO: Add your rendering code here.
    //context;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // シーンの描画
    m_sceneManager.Render(*m_gameContext);

    // デバッグ関連の文字列などを描画
    m_debugRenderer->Render(context, m_states.get());

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    m_deviceResources->PIXEndEvent();

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    const auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}

#pragma endregion

#pragma region Message Handlers

// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
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

    // TODO: Game window is being resized.
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 1280;
    height = 720;
}

#pragma endregion

#pragma region Direct3D Resources

// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    // TODO: Initialize device dependent objects here (independent of window size).
    
    // デバッグ用の描画セットの作成
    m_debugRenderer = std::make_unique<Imase::DebugRenderer>(device, context, L"Resources/Font/myfile.spritefont");

    // コモンステートの作成
    m_states = std::make_unique<CommonStates>(device);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark(); 

    ImGui_ImplWin32_Init(m_deviceResources->GetWindow());
    ImGui_ImplDX11_Init(device, context);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    //// TODO: Initialize windows-size dependent objects here.
    //if (!m_gameContext)
    //{
    //    return;
    //}
    //m_sceneManager.OnWindowSizeChanged(*m_gameContext);
}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
    //m_sceneManager.OnDeviceLost(*m_gameContext);

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

// ドラッグ＆ドロップされたファイルのハンドリング
void Game::OnFileDropped(const std::wstring& filePath)
{
    if (!m_gameContext.has_value())
        return;

    // SceneManager に OnFileDropped イベントを転送
    m_sceneManager.OnFileDropped(*m_gameContext, filePath);
}

#pragma endregion