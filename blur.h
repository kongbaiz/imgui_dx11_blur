#pragma once
#include <array>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <imgui.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace Blur {

    struct BlurConstants {
        float pixelSize;
        float radius;
        float direction;
        float pad;
    };

    struct DeviceStateBackup {
        D3D11_VIEWPORT viewports[16];
        UINT viewportCount{};
        ComPtr<ID3D11DepthStencilState> depthStencilState;
        UINT stencilRef{};
        ComPtr<ID3D11BlendState> blendState;
        FLOAT blendFactor[4]{};
        UINT sampleMask{};
    };

    // Global static resources
    static ComPtr<ID3D11Device> g_Device;
    static ComPtr<ID3D11DeviceContext> g_Context;

    static ComPtr<ID3D11PixelShader> g_BlurPixelShader;
    static ComPtr<ID3D11SamplerState> g_Sampler;
    static std::array<ComPtr<ID3D11Texture2D>, 3> g_BlurTextures;
    static std::array<ComPtr<ID3D11ShaderResourceView>, 3> g_BlurSRVs;
    static std::array<ComPtr<ID3D11RenderTargetView>, 3> g_BlurRTVs;

    static ComPtr<ID3D11Buffer> g_ConstantBuffer;
    static ComPtr<ID3D11Buffer> g_QuadVertexBuffer;
    static ComPtr<ID3D11VertexShader> g_QuadVertexShader;
    static ComPtr<ID3D11InputLayout> g_QuadInputLayout;
    static ComPtr<ID3D11RenderTargetView> g_BackupRenderTarget;

    static DeviceStateBackup g_BackupState;
    static int g_Width = 0;
    static int g_Height = 0;
    static DXGI_FORMAT g_Format = DXGI_FORMAT_UNKNOWN;
    static bool g_Initialized = false;

    static constexpr const char* BLUR_SHADER_HLSL = R"(
        Texture2D tex : t0;
        SamplerState samp : s0;
        cbuffer C : register(b0) { float pixelSize; float radius; float direction; float pad; }
        float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
            float4 color = 0;
            float weightSum = 0;
            for (float i = -16; i <= 16; i++) {
                float w = exp(-(i * i) / 128);
                color += tex.Sample(samp, uv + float2(i * pixelSize * radius * direction, (1 - direction) * i * pixelSize * radius)) * w;
                weightSum += w;
            }
            return float4(color.rgb / max(weightSum, 1e-6), 1);
        })";

    bool CreatePixelShaderFromSource(const char* src, ComPtr<ID3D11PixelShader>& out) {
        ComPtr<ID3DBlob> blob;
        if (FAILED(D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &blob, nullptr))) return false;
        return SUCCEEDED(g_Device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &out));
    }

    bool CreateBlurTextures(int width, int height, DXGI_FORMAT format) {
        for (int i = 0; i < 3; i++) {
            D3D11_TEXTURE2D_DESC desc = { (UINT)width, (UINT)height, 1, 1, format, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE };
            if (FAILED(g_Device->CreateTexture2D(&desc, nullptr, &g_BlurTextures[i]))) return false;
            if (FAILED(g_Device->CreateShaderResourceView(g_BlurTextures[i].Get(), nullptr, &g_BlurSRVs[i]))) return false;
            if (FAILED(g_Device->CreateRenderTargetView(g_BlurTextures[i].Get(), nullptr, &g_BlurRTVs[i]))) return false;
        }
        return true;
    }

    bool CreateFullscreenQuad() {
        struct Vertex { float pos[2]; } vertices[4] = { {-1, 1}, {1, 1}, {-1, -1}, {1, -1} };
        D3D11_BUFFER_DESC bufferDesc = { sizeof(vertices), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER };
        D3D11_SUBRESOURCE_DATA initData = { vertices };
        if (FAILED(g_Device->CreateBuffer(&bufferDesc, &initData, &g_QuadVertexBuffer))) return false;

        const char* vsSource = R"(
            struct Output { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
            Output main(float2 pos : POSITION) {
                Output o;
                o.pos = float4(pos, 0, 1);
                o.uv = float2((pos.x + 1) * 0.5, 1 - (pos.y + 1) * 0.5);
                return o;
            })";
        ComPtr<ID3DBlob> blob;
        if (FAILED(D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &blob, nullptr))) return false;
        if (FAILED(g_Device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_QuadVertexShader))) return false;

        D3D11_INPUT_ELEMENT_DESC layout = { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
        return SUCCEEDED(g_Device->CreateInputLayout(&layout, 1, blob->GetBufferPointer(), blob->GetBufferSize(), &g_QuadInputLayout));
    }

    void DrawFullscreenQuad() {
        UINT stride = sizeof(float) * 2, offset = 0;
        g_Context->IASetVertexBuffers(0, 1, g_QuadVertexBuffer.GetAddressOf(), &stride, &offset);
        g_Context->IASetInputLayout(g_QuadInputLayout.Get());
        g_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        g_Context->VSSetShader(g_QuadVertexShader.Get(), nullptr, 0);
        g_Context->Draw(4, 0);
    }

    void UpdateBlurConstants(float pixelSize, float radius, float direction) {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        if (SUCCEEDED(g_Context->Map(g_ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
            auto* constants = reinterpret_cast<BlurConstants*>(mappedResource.pData);
            constants->pixelSize = pixelSize;
            constants->radius = radius;
            constants->direction = direction;
            g_Context->Unmap(g_ConstantBuffer.Get(), 0);
        }
    }

    void BackupDeviceState() {
        g_BackupState.viewportCount = 16;
        g_Context->RSGetViewports(&g_BackupState.viewportCount, g_BackupState.viewports);
        g_Context->OMGetDepthStencilState(&g_BackupState.depthStencilState, &g_BackupState.stencilRef);
        g_Context->OMGetBlendState(&g_BackupState.blendState, g_BackupState.blendFactor, &g_BackupState.sampleMask);
    }

    void RestoreDeviceState() {
        g_Context->RSSetViewports(g_BackupState.viewportCount, g_BackupState.viewports);
        g_Context->OMSetDepthStencilState(g_BackupState.depthStencilState.Get(), g_BackupState.stencilRef);
        g_Context->OMSetBlendState(g_BackupState.blendState.Get(), g_BackupState.blendFactor, g_BackupState.sampleMask);
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        g_Device = device;
        g_Context = context;

        D3D11_BUFFER_DESC constantBufferDesc = { sizeof(BlurConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE };
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

        return (g_Initialized = CreatePixelShaderFromSource(BLUR_SHADER_HLSL, g_BlurPixelShader) &&
            SUCCEEDED(g_Device->CreateSamplerState(&samplerDesc, &g_Sampler)) &&
            CreateBlurTextures(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM) &&
            CreateFullscreenQuad() &&
            SUCCEEDED(g_Device->CreateBuffer(&constantBufferDesc, nullptr, &g_ConstantBuffer)));
    }

    void Begin() {
        ComPtr<ID3D11RenderTargetView> currentRTV;
        g_Context->OMGetRenderTargets(1, &currentRTV, nullptr);
        if (!currentRTV) return;

        ComPtr<ID3D11Texture2D> sourceTexture;
        currentRTV->GetResource(reinterpret_cast<ID3D11Resource**>(sourceTexture.GetAddressOf()));

        D3D11_TEXTURE2D_DESC desc;
        sourceTexture->GetDesc(&desc);

        if (g_Width != static_cast<int>(desc.Width) || g_Height != static_cast<int>(desc.Height) || g_Format != desc.Format) {
            CreateBlurTextures(desc.Width, desc.Height, desc.Format);
            g_Width = desc.Width;
            g_Height = desc.Height;
            g_Format = desc.Format;
        }

        g_Context->CopyResource(g_BlurTextures[0].Get(), sourceTexture.Get());
        g_Context->OMGetRenderTargets(1, &g_BackupRenderTarget, nullptr);
    }

    void Apply(ImDrawList* drawList, const ImVec2& position, const ImVec2& size, float radius, float rounding = 0, ImDrawFlags flags = 0) {
        BackupDeviceState();
        D3D11_VIEWPORT viewport = { 0, 0, static_cast<float>(g_Width), static_cast<float>(g_Height), 0, 1 };
        g_Context->RSSetViewports(1, &viewport);
        g_Context->OMSetDepthStencilState(nullptr, 0);
        g_Context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        g_Context->PSSetConstantBuffers(0, 1, g_ConstantBuffer.GetAddressOf());
        g_Context->PSSetSamplers(0, 1, g_Sampler.GetAddressOf());
        g_Context->PSSetShader(g_BlurPixelShader.Get(), nullptr, 0);

        auto BlurPass = [&](int srcIndex, int dstIndex, float pixelSize, float direction) {
            ID3D11ShaderResourceView* nullSRV = nullptr;
            g_Context->PSSetShaderResources(0, 1, &nullSRV);
            g_Context->OMSetRenderTargets(1, g_BlurRTVs[dstIndex].GetAddressOf(), nullptr);
            g_Context->PSSetShaderResources(0, 1, g_BlurSRVs[srcIndex].GetAddressOf());
            UpdateBlurConstants(pixelSize, radius, direction);
            DrawFullscreenQuad();
            };

        BlurPass(0, 1, 1.0f / g_Width, 1.0f); // horizontal
        BlurPass(1, 2, 1.0f / g_Height, 0.0f); // vertical

        g_Context->OMSetRenderTargets(1, g_BackupRenderTarget.GetAddressOf(), nullptr);
        RestoreDeviceState();

        ImVec2 uv0(position.x / g_Width, position.y / g_Height);
        ImVec2 uv1((position.x + size.x) / g_Width, (position.y + size.y) / g_Height);
        drawList->AddImageRounded((ImTextureID)g_BlurSRVs[2].Get(), position, { position.x + size.x, position.y + size.y }, uv0, uv1, IM_COL32_WHITE, rounding, flags);
    }

    void End() {
        g_BackupRenderTarget.Reset();
        ID3D11ShaderResourceView* nullSRV = nullptr;
        g_Context->PSSetShaderResources(0, 1, &nullSRV);
        g_Context->PSSetShader(nullptr, nullptr, 0);
    }

}
