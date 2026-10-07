#pragma once
#include <SimpleMath.h>
#include <Keyboard.h>

class CameraController
{
public:
    void LoadConfig(const std::string& filePath);
    void SaveConfig(const std::string& filePath) const;

    void Update(float dt, const DirectX::Keyboard::State& kbState, bool isCtrlPressed);

    DirectX::SimpleMath::Matrix GetViewMatrix() const;
    DirectX::SimpleMath::Matrix GetProjectionMatrix(float aspectRatio) const;

private:
    DirectX::SimpleMath::Vector3 m_eye = {0.0f, 22.0f, -20.0f};
    DirectX::SimpleMath::Vector3 m_target = {0.0f, 0.0f, 0.0f};
    float m_pitch = 48.0f;
    float m_distance = 30.0f;
};