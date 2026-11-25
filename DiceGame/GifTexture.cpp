#include "pch.h"
#include "GifTexture.h"
#include <WICTextureLoader.h>

bool GifTexture::LoadFromFile(ID3D11Device* device, const std::wstring& filePath)
{
    m_frames.clear();
    m_currentFrameIndex = 0;
    m_frameTimer = 0.0f;

    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr))
        return false;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = wicFactory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                               &decoder);
    if (FAILED(hr))
        return false;

    UINT frameCount = 0;
    decoder->GetFrameCount(&frameCount);
    if (frameCount == 0)
        return false;

    for (UINT i = 0; i < frameCount; ++i)
    {
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        wicFactory->CreateFormatConverter(&converter);

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frameDecode;
        decoder->GetFrame(i, &frameDecode);

        converter->Initialize(frameDecode.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                              WICBitmapPaletteTypeCustom);

        UINT width = 0, height = 0;
        converter->GetSize(&width, &height);

        std::vector<BYTE> buffer(width * height * 4);
        converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(buffer.size()), buffer.data());

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = buffer.data();
        initData.SysMemPitch = width * 4;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
        hr = device->CreateTexture2D(&desc, &initData, &tex2D);
        if (FAILED(hr))
            continue;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(tex2D.Get(), nullptr, &srv);
        if (FAILED(hr))
            continue;

        // コマごとの Delay 取得（設定がなければ 0.1 秒）
        float delay = 0.1f;
        Microsoft::WRL::ComPtr<IWICMetadataQueryReader> metadataReader;
        if (SUCCEEDED(frameDecode->GetMetadataQueryReader(&metadataReader)))
        {
            PROPVARIANT propValue;
            PropVariantInit(&propValue);
            if (SUCCEEDED(metadataReader->GetMetadataByName(L"/grhctl/Delay", &propValue)))
            {
                if (propValue.vt == VT_UI2)
                {
                    delay = static_cast<float>(propValue.uiVal) * 0.01f;
                    if (delay <= 0.001f)
                        delay = 0.1f;
                }
            }
            PropVariantClear(&propValue);
        }

        m_frames.push_back({srv, delay});
    }

    return !m_frames.empty();
}

void GifTexture::Update(float dt)
{
    if (m_frames.empty())
        return;

    m_frameTimer += dt;
    if (m_frameTimer >= m_frames[m_currentFrameIndex].delaySeconds)
    {
        m_frameTimer -= m_frames[m_currentFrameIndex].delaySeconds;
        m_currentFrameIndex = (m_currentFrameIndex + 1) % m_frames.size();
    }
}

ID3D11ShaderResourceView* GifTexture::GetCurrentTexture() const
{
    if (m_frames.empty())
        return nullptr;
    return m_frames[m_currentFrameIndex].texture.Get();
}