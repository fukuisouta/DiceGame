#pragma once

#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <string>

struct GifFrame
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    float delaySeconds = 0.1f;
};

class GifTexture
{
public:
    GifTexture() = default;
    ~GifTexture() = default;

    bool LoadFromFile(ID3D11Device* device, const std::wstring& filePath);
    void Update(float dt);
    ID3D11ShaderResourceView* GetCurrentTexture() const;

private:
    std::vector<GifFrame> m_frames;
    size_t m_currentFrameIndex = 0;
    float m_frameTimer = 0.0f;
};