// D3D9 back end: an SDL3 window and a Direct3D 9 device. On Linux d3d9 is DXVK
// (dxvk-native, which takes the SDL_Window* as its HWND); on Windows it is the system's.

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

#include <memory>

#include "backend.h"
#include "d3d9_api.h"
#include "d3d9_draw.h"
#include "log.h"

namespace render {

namespace {

template <typename T>
void release(T*& object) {
    if (object) object->Release();
    object = nullptr;
}

HWND window_handle(SDL_Window* window) {
#ifdef _WIN32
    return static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
#else
    return reinterpret_cast<HWND>(window);
#endif
}

// BGRX rows -> bottom-up 24-bit BMP.
bool write_bmp(const char* path, const uint8_t* pixels, int32_t pitch, uint32_t width, uint32_t height) {
    std::FILE* file = std::fopen(path, "wb");
    if (!file) return false;
    const uint32_t row = (width * 3 + 3) & ~3u;
    const uint32_t image = row * height;
    uint8_t header[54] = {'B', 'M'};
    auto put32 = [&](int offset, uint32_t v) { std::memcpy(header + offset, &v, 4); };
    put32(2, 54 + image);
    put32(10, 54);
    put32(14, 40);
    put32(18, width);
    put32(22, height);
    header[26] = 1;
    header[28] = 24;
    put32(34, image);
    std::fwrite(header, 1, sizeof(header), file);
    std::vector<uint8_t> line(row, 0);
    for (uint32_t y = height; y-- > 0;) {
        const uint8_t* src = pixels + static_cast<int64_t>(y) * pitch;
        for (uint32_t x = 0; x < width; ++x) std::memcpy(&line[x * 3], src + x * 4, 3);
        std::fwrite(line.data(), 1, row, file);
    }
    return std::fclose(file) == 0;
}

class D3D9Backend final : public Backend {
public:
    ~D3D9Backend() override {
        draw_.reset();
        release(staging_);
        release(device_);
        release(d3d_);
        if (window_) SDL_DestroyWindow(window_);
        if (sdl_started_) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    bool init(const BackendConfig& config) {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            LOG_ERROR("render", "SDL video: %s", SDL_GetError());
            return false;
        }
        sdl_started_ = true;
        const uint32_t scale = config.window_scale ? config.window_scale : 1;
        window_ = SDL_CreateWindow("Call of Duty 2: Big Red One", static_cast<int>(config.width * scale),
                                   static_cast<int>(config.height * scale), SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (!window_) {
            LOG_ERROR("render", "window: %s", SDL_GetError());
            return false;
        }
#ifndef _WIN32
        // dxvk-native supports several window systems; ours is SDL3.
        setenv("DXVK_WSI_DRIVER", "SDL3", 0);
#endif
        d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d_) {
            LOG_ERROR("render", "Direct3DCreate9 failed");
            return false;
        }
        D3DADAPTER_IDENTIFIER9 id{};
        if (SUCCEEDED(d3d_->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id)))
            LOG_INFO("render", "D3D9 adapter: %s", id.Description);

        D3DPRESENT_PARAMETERS pp{};
        pp.BackBufferWidth = config.width;
        pp.BackBufferHeight = config.height;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferCount = 1;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = window_handle(window_);
        pp.Windowed = TRUE;
        pp.EnableAutoDepthStencil = TRUE;
        pp.AutoDepthStencilFormat = D3DFMT_D24S8;
        pp.PresentationInterval = config.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
        // The title calls D3D from more than one thread (its vertical blank callback
        // and loading screens), so let the runtime serialize.
        const HRESULT hr = d3d_->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pp.hDeviceWindow,
                                              D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED |
                                                  D3DCREATE_FPU_PRESERVE,
                                              &pp, &device_);
        if (FAILED(hr)) {
            LOG_ERROR("render", "CreateDevice failed: %08x", static_cast<unsigned>(hr));
            return false;
        }
        width_ = config.width;
        height_ = config.height;
        draw_ = std::make_unique<D3D9Draw>(device_, width_, height_);
        LOG_INFO("render", "D3D9 device: %ux%u", config.width, config.height);
        return true;
    }

    void clear(uint32_t flags, uint32_t color, float depth, uint32_t stencil) override {
        DWORD d3d_flags = 0;
        if (flags & kClearColor) d3d_flags |= D3DCLEAR_TARGET;
        if (flags & kClearDepth) d3d_flags |= D3DCLEAR_ZBUFFER;
        if (flags & kClearStencil) d3d_flags |= D3DCLEAR_STENCIL;
        if (d3d_flags) device_->Clear(0, nullptr, d3d_flags, color, depth, stencil);
    }

    void write_back_buffer(const void* pixels, uint32_t pitch) override {
        if (!staging_ &&
            FAILED(device_->CreateOffscreenPlainSurface(width_, height_, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                        &staging_, nullptr)))
            return;
        D3DLOCKED_RECT locked;
        if (FAILED(staging_->LockRect(&locked, nullptr, 0))) return;
        for (uint32_t y = 0; y < height_; ++y)
            std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch,
                        static_cast<const uint8_t*>(pixels) + y * pitch, width_ * 4);
        staging_->UnlockRect();
        IDirect3DSurface9* back_buffer = nullptr;
        if (SUCCEEDED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer))) {
            device_->UpdateSurface(staging_, nullptr, back_buffer, nullptr);
            back_buffer->Release();
        }
    }

    bool save_back_buffer(const char* path) override {
        IDirect3DSurface9* back_buffer = nullptr;
        IDirect3DSurface9* copy = nullptr;
        bool ok = SUCCEEDED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)) &&
                  SUCCEEDED(device_->CreateOffscreenPlainSurface(width_, height_, D3DFMT_X8R8G8B8,
                                                                 D3DPOOL_SYSTEMMEM, &copy, nullptr)) &&
                  SUCCEEDED(device_->GetRenderTargetData(back_buffer, copy));
        D3DLOCKED_RECT locked{};
        if (ok) ok = SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY));
        if (ok) {
            ok = write_bmp(path, static_cast<const uint8_t*>(locked.pBits), locked.Pitch, width_, height_);
            copy->UnlockRect();
        }
        release(copy);
        release(back_buffer);
        if (!ok) LOG_WARN("render", "cannot save the back buffer to %s", path);
        return ok;
    }

    void present() override {
        device_->Present(nullptr, nullptr, nullptr, nullptr);
        draw_->end_frame();
    }

    void draw(const DrawInput& input) override {
        device_->BeginScene();
        draw_->draw(input);
        device_->EndScene();
    }

    bool pump_events() override {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) return false;
        return true;
    }

private:
    bool sdl_started_ = false;
    SDL_Window* window_ = nullptr;
    IDirect3D9* d3d_ = nullptr;
    IDirect3DDevice9* device_ = nullptr;
    IDirect3DSurface9* staging_ = nullptr;  // CPU-written back buffer upload
    std::unique_ptr<D3D9Draw> draw_;
    uint32_t width_ = 0, height_ = 0;
};

}  // namespace

std::unique_ptr<Backend> create_d3d9_backend(const BackendConfig& config) {
    auto backend = std::make_unique<D3D9Backend>();
    if (!backend->init(config)) return nullptr;
    return backend;
}

}  // namespace render
