#include "Skybox.h"
#include <d3dcompiler.h>
#include "DDSTextureLoader.h"

using namespace DirectX;

// スカイボックス構成定数定義
namespace SkyboxStyleConstants
{
    constexpr UINT CUBE_VERTEX_COUNT = 8; // 立方体の頂点数
}

// コンストラクタ
Skybox::Skybox() : m_indexCount(0)
{
}

// 初期化処理
bool Skybox::Initialize(ID3D11Device* device, const wchar_t* cubeMapTexturePath)
{
    Vertex vertices[SkyboxStyleConstants::CUBE_VERTEX_COUNT] = {
        {XMFLOAT3(-1.0f, 1.0f, -1.0f)},  // 0: 左上奥
        {XMFLOAT3(1.0f, 1.0f, -1.0f)},   // 1: 右上奥
        {XMFLOAT3(1.0f, 1.0f, 1.0f)},    // 2: 右上手前
        {XMFLOAT3(-1.0f, 1.0f, 1.0f)},   // 3: 左上手前
        {XMFLOAT3(-1.0f, -1.0f, -1.0f)}, // 4: 左下奥
        {XMFLOAT3(1.0f, -1.0f, -1.0f)},  // 5: 右下奥
        {XMFLOAT3(1.0f, -1.0f, 1.0f)},   // 6: 右下手前
        {XMFLOAT3(-1.0f, -1.0f, 1.0f)},  // 7: 左下手前
    };

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(Vertex) * SkyboxStyleConstants::CUBE_VERTEX_COUNT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vInitData = {};
    vInitData.pSysMem = vertices;
    if (FAILED(device->CreateBuffer(&vbd, &vInitData, m_vertexBuffer.GetAddressOf())))
        return false;

    UINT indices[] = {
        3, 1, 0, 2, 1, 3, // 上面 (Top)
        4, 5, 7, 5, 6, 7, // 下面 (Bottom)
        0, 7, 4, 0, 3, 7, // 左面 (Left)
        1, 2, 5, 2, 6, 5, // 右面 (Right)
        0, 1, 4, 1, 5, 4, // 前面 (Front)
        2, 3, 6, 3, 7, 6  // 背面 (Back)
    };

    m_indexCount = SkyboxConstants::INDEX_COUNT_CUBE;

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(UINT) * m_indexCount;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iInitData = {};
    iInitData.pSysMem = indices;
    if (FAILED(device->CreateBuffer(&ibd, &iInitData, m_indexBuffer.GetAddressOf())))
        return false;

    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device->CreateBuffer(&cbd, nullptr, m_constantBuffer.GetAddressOf())))
        return false;

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;

    if (FAILED(D3DCompileFromFile(L"SkyboxShader.hlsl", nullptr, nullptr, "VS", "vs_5_0", 0, 0, vsBlob.GetAddressOf(),
                                  nullptr)))
        return false;

    if (FAILED(D3DCompileFromFile(L"SkyboxShader.hlsl", nullptr, nullptr, "PS", "ps_5_0", 0, 0, psBlob.GetAddressOf(),
                                  nullptr)))
        return false;

    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr,
                                          m_vertexShader.GetAddressOf())))
        return false;

    if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                         m_pixelShader.GetAddressOf())))
        return false;

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}};

    if (FAILED(device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                         m_inputLayout.GetAddressOf())))
        return false;

    if (FAILED(CreateDDSTextureFromFile(device, cubeMapTexturePath, nullptr, m_textureSRV.GetAddressOf())))
        return false;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(device->CreateSamplerState(&sampDesc, m_samplerState.GetAddressOf())))
        return false;

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(device->CreateDepthStencilState(&dsd, m_depthStencilState.GetAddressOf())))
        return false;

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    if (FAILED(device->CreateRasterizerState(&rd, m_rasterizerState.GetAddressOf())))
        return false;

    return true;
}

// 描画処理
void Skybox::Render(ID3D11DeviceContext* context, const XMFLOAT4X4& viewMatrix, const XMFLOAT4X4& projectionMatrix,
                    const XMFLOAT3& cameraPosition)
{
    XMMATRIX world = XMMatrixTranslation(cameraPosition.x, cameraPosition.y, cameraPosition.z);
    XMMATRIX view = XMLoadFloat4x4(&viewMatrix);
    XMMATRIX proj = XMLoadFloat4x4(&projectionMatrix);

    XMMATRIX wvp = world * view * proj;

    ConstantBuffer cb;
    cb.worldViewProjection = XMMatrixTranspose(wvp);
    context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, m_textureSRV.GetAddressOf());
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    context->RSSetState(m_rasterizerState.Get());

    context->DrawIndexed(m_indexCount, 0, 0);

    context->OMSetDepthStencilState(nullptr, 0);
    context->RSSetState(nullptr);
}