// Record.h - Gravar tela: Desktop Duplication (DXGI) + Media Foundation (H.264 e AAC em MP4) + WASAPI
// (som do computador por loopback e microfone). Tudo vem do Windows: nada de biblioteca extra no instalador.
//
// As janelas marcadas com WDA_EXCLUDEFROMCAPTURE (a barra de gravação, e o relógio quando "Esconder" está
// marcado) não aparecem no Desktop Duplication: o Windows as tira da imagem antes de entregá-la.
//
// A parte que não depende do Windows (relógio com pausa, alinhamento do áudio, mixagem, nome do arquivo,
// recorte em tamanho par) é testada em tests/record_tests.cpp.
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <shlobj.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

namespace rec {

constexpr UINT kRate = 48000;                 // áudio: 48 kHz estéreo
constexpr UINT kCh = 2;
constexpr UINT kFps = 30;
constexpr LONGLONG kFrame = 10000000 / kFps;  // duração de um quadro em 100 ns
constexpr LONGLONG kAudioLag = 1500000;       // o áudio sai 150 ms atrás do relógio: dá tempo de os pacotes chegarem
constexpr LONGLONG kGapFrames = kRate / 50;   // buraco maior que 20 ms no loopback (silêncio) vira zeros

// ---------- partes puras (testadas) ----------

// H.264 exige largura e altura pares.
inline RECT EvenRect(RECT r) {
    if ((r.right - r.left) & 1) --r.right;
    if ((r.bottom - r.top) & 1) --r.bottom;
    return r;
}

// "Vizu Gravação 2026-11-28 14-32-05.mp4"
inline std::wstring FileName(const SYSTEMTIME& t) {
    wchar_t b[96];
    swprintf(b, 96, L"Vizu Gravação %04u-%02u-%02u %02u-%02u-%02u.mp4", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}

// Tempo do vídeo (100 ns) a partir de um relógio de parede (100 ns): o tempo pausado não conta.
struct MediaClock {
    LONGLONG start = 0, pausedAt = 0, pausedTotal = 0;
    bool paused = false;
    void Start(LONGLONG now) { start = now; pausedTotal = 0; paused = false; }
    void Pause(LONGLONG now) { if (!paused) { paused = true; pausedAt = now; } }
    void Resume(LONGLONG now) { if (paused) { paused = false; pausedTotal += now - pausedAt; } }
    LONGLONG At(LONGLONG wall) const { // tempo do vídeo de um instante (pausado: fica parado no início da pausa)
        LONGLONG w = paused && wall > pausedAt ? pausedAt : wall;
        LONGLONG t = w - start - pausedTotal;
        return t > 0 ? t : 0;
    }
};

// Uma fonte de áudio (estéreo float) presa à linha do tempo: buf[0] é a amostra "base" do vídeo.
struct Track {
    std::vector<float> buf;
    LONGLONG base = 0;
    bool any = false;
    LONGLONG End() const { return base + (LONGLONG)(buf.size() / kCh); }
    // Pacote de 'frames' amostras capturado na amostra 'at' (p == nullptr: silêncio).
    void Push(LONGLONG at, const float* p, size_t frames) {
        if (!any) { any = true; base = at; buf.clear(); }
        else if (at > End() + kGapFrames) buf.resize(buf.size() + (size_t)(at - End()) * kCh, 0.0f); // silêncio sem pacotes
        size_t n = buf.size();
        buf.resize(n + frames * kCh, 0.0f);
        if (p) for (size_t i = 0; i < frames * kCh; ++i) buf[n + i] = p[i];
    }
    // Soma em acc as amostras [from, from + frames) e descarta o que ficou para trás.
    void Take(LONGLONG from, size_t frames, float* acc) {
        if (!any) return;
        for (size_t i = 0; i < frames; ++i) {
            LONGLONG idx = from + (LONGLONG)i - base;
            if (idx < 0 || (size_t)idx >= buf.size() / kCh) continue;
            acc[i * kCh] += buf[(size_t)idx * kCh];
            acc[i * kCh + 1] += buf[(size_t)idx * kCh + 1];
        }
        LONGLONG keep = from + (LONGLONG)frames, drop = keep - base;
        if (drop <= 0) return;
        if ((size_t)drop >= buf.size() / kCh) { buf.clear(); base = keep; return; }
        buf.erase(buf.begin(), buf.begin() + (size_t)drop * kCh);
        base += drop;
    }
    void Clear() { buf.clear(); any = false; }
};

// Soma as fontes e converte para PCM de 16 bits (com limite, sem estourar).
inline void Mix(Track* tracks, int count, LONGLONG from, size_t frames, int16_t* out) {
    std::vector<float> acc(frames * kCh, 0.0f);
    for (int t = 0; t < count; ++t) tracks[t].Take(from, frames, acc.data());
    for (size_t i = 0; i < acc.size(); ++i) {
        float v = acc[i];
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        out[i] = (int16_t)(v * 32767.0f);
    }
}

inline LONGLONG ToSamples(LONGLONG t100ns) { return t100ns * kRate / 10000000; }
inline LONGLONG FromSamples(LONGLONG s) { return s * 10000000 / kRate; }

// Relógio de parede em 100 ns, na mesma base do qpcPosition do WASAPI.
inline LONGLONG Now100ns() {
    static LARGE_INTEGER f = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x; }();
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (LONGLONG)((double)c.QuadPart * 10000000.0 / (double)f.QuadPart);
}

template <class T> inline void Rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

// ---------- áudio: WASAPI ----------
struct AudioSrc {
    IAudioClient* cl = nullptr;
    IAudioCaptureClient* cap = nullptr;
    bool Open(bool loopback) {
        IMMDeviceEnumerator* en = nullptr; IMMDevice* dev = nullptr;
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) return false;
        HRESULT hr = en->GetDefaultAudioEndpoint(loopback ? eRender : eCapture, eConsole, &dev);
        en->Release();
        if (FAILED(hr)) return false;
        hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&cl);
        dev->Release();
        if (FAILED(hr)) return false;
        WAVEFORMATEX wf = {};
        wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT; wf.nChannels = kCh; wf.nSamplesPerSec = kRate; wf.wBitsPerSample = 32;
        wf.nBlockAlign = kCh * 4; wf.nAvgBytesPerSec = kRate * wf.nBlockAlign;
        DWORD flags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | (loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0);
        if (FAILED(cl->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 2000000, 0, &wf, nullptr)) ||
            FAILED(cl->GetService(__uuidof(IAudioCaptureClient), (void**)&cap)) || FAILED(cl->Start())) { Close(); return false; }
        return true;
    }
    // Lê tudo o que chegou. keep == false (pausado): descarta.
    void Drain(Track& t, const MediaClock& clk, bool keep) {
        if (!cap) return;
        UINT32 next = 0;
        while (SUCCEEDED(cap->GetNextPacketSize(&next)) && next) {
            BYTE* data = nullptr; UINT32 frames = 0; DWORD fl = 0; UINT64 pos = 0, qpc = 0;
            if (FAILED(cap->GetBuffer(&data, &frames, &fl, &pos, &qpc))) return;
            if (keep && frames) {
                LONGLONG wall = (fl & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) || !qpc ? Now100ns() - FromSamples(frames) : (LONGLONG)qpc;
                t.Push(ToSamples(clk.At(wall)), (fl & AUDCLNT_BUFFERFLAGS_SILENT) ? nullptr : (const float*)data, frames);
            }
            cap->ReleaseBuffer(frames);
        }
    }
    void Close() { if (cl) cl->Stop(); Rel(cap); Rel(cl); }
};

// ---------- vídeo: Desktop Duplication ----------
struct Dup {
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGIOutputDuplication* dup = nullptr;
    ID3D11Texture2D* staging = nullptr;
    RECT desk = {};      // área do monitor no desktop
    UINT w = 0, h = 0;   // tamanho da textura
    bool have = false;   // staging já tem uma imagem
    HMONITOR mon = nullptr;

    bool Open(HMONITOR m) {
        Close(); mon = m;
        IDXGIFactory1* fac = nullptr;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&fac))) return false;
        IDXGIAdapter1* ad = nullptr; IDXGIOutput* out = nullptr;
        for (UINT a = 0; !out && fac->EnumAdapters1(a, &ad) != DXGI_ERROR_NOT_FOUND; ++a) {
            IDXGIOutput* o = nullptr;
            for (UINT i = 0; ad->EnumOutputs(i, &o) != DXGI_ERROR_NOT_FOUND; ++i) {
                DXGI_OUTPUT_DESC d; o->GetDesc(&d);
                if (d.Monitor == m) { out = o; desk = d.DesktopCoordinates; break; }
                o->Release();
            }
            if (!out) Rel(ad);
        }
        fac->Release();
        if (!out) return false;
        D3D_FEATURE_LEVEL fl;
        HRESULT hr = D3D11CreateDevice(ad, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
        ad->Release();
        IDXGIOutput1* o1 = nullptr;
        if (SUCCEEDED(hr)) hr = out->QueryInterface(__uuidof(IDXGIOutput1), (void**)&o1);
        out->Release();
        if (SUCCEEDED(hr)) hr = o1->DuplicateOutput(dev, &dup);
        Rel(o1);
        if (FAILED(hr)) { Close(); return false; }
        DXGI_OUTDUPL_DESC dd; dup->GetDesc(&dd);
        w = dd.ModeDesc.Width; h = dd.ModeDesc.Height;
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &staging))) { Close(); return false; }
        return true;
    }
    // Espera até timeout ms por uma imagem nova e a copia para a staging (na GPU). false = perdeu o acesso.
    bool Grab(UINT timeout) {
        if (!dup) return false;
        DXGI_OUTDUPL_FRAME_INFO fi; IDXGIResource* res = nullptr;
        HRESULT hr = dup->AcquireNextFrame(timeout, &fi, &res);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) return true;
        if (FAILED(hr)) return false;
        ID3D11Texture2D* tex = nullptr;
        if (fi.LastPresentTime.QuadPart && SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex))) {
            ctx->CopyResource(staging, tex); have = true; tex->Release();
        }
        res->Release();
        dup->ReleaseFrame();
        return true;
    }
    // Copia o recorte (coordenadas do desktop) para dst (BGRA de cima para baixo, stride cw*4).
    bool Read(const RECT& crop, BYTE* dst) {
        if (!have) return false;
        D3D11_MAPPED_SUBRESOURCE mp;
        if (FAILED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mp))) return false;
        int cw = crop.right - crop.left, ch = crop.bottom - crop.top;
        int ox = crop.left - desk.left, oy = crop.top - desk.top;
        for (int y = 0; y < ch; ++y) {
            BYTE* d = dst + (size_t)y * cw * 4;
            int sy = oy + y;
            if (sy < 0 || sy >= (int)h) { memset(d, 0, (size_t)cw * 4); continue; }
            int x0 = ox < 0 ? -ox : 0, x1 = ox + cw > (int)w ? (int)w - ox : cw;
            if (x0 > 0) memset(d, 0, (size_t)x0 * 4);
            if (x1 > x0) memcpy(d + x0 * 4, (const BYTE*)mp.pData + (size_t)sy * mp.RowPitch + (size_t)(ox + x0) * 4, (size_t)(x1 - x0) * 4);
            if (x1 < cw) memset(d + (x1 > 0 ? x1 : 0) * 4, 0, (size_t)(cw - (x1 > 0 ? x1 : 0)) * 4);
        }
        ctx->Unmap(staging, 0);
        return true;
    }
    void Close() { Rel(staging); Rel(dup); Rel(ctx); Rel(dev); have = false; }
};

// ---------- gravação ----------
struct Options {
    HMONITOR mon = nullptr;
    bool sysAudio = false, mic = false;
    bool hide = false;        // sem cursor e sem a barra de tarefas (recorta na área útil)
    HWND notify = nullptr;    // recebe 'msg' quando terminar (wParam = 1 se salvou)
    UINT msg = 0;
};

class Recorder {
public:
    ~Recorder() { Stop(); Join(); }
    // Abre a captura e o arquivo. Devolve false com a mensagem em *err.
    bool Start(const Options& o, std::wstring* err) {
        if (th_.joinable()) Join();
        opt_ = o; notify_ = o.notify; stop_ = false; paused_ = false; ok_ = false; err_.clear(); warn_.clear();
        HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        bool started = false;
        th_ = std::thread([this, ready, &started] { Run(ready, &started); });
        WaitForSingleObject(ready, INFINITE);
        CloseHandle(ready);
        if (!started) { Join(); if (err) *err = err_; return false; }
        return true;
    }
    void Pause(bool p) { paused_ = p; }
    bool Paused() const { return paused_; }
    void Stop() { stop_ = true; }
    void Notify(HWND h) { notify_ = h; }
    void Join() { if (th_.joinable()) th_.join(); }
    LONGLONG Elapsed() const { return elapsed_; }  // 100 ns, sem as pausas
    const std::wstring& Path() const { return path_; }
    const std::wstring& Error() const { return err_; }
    const std::wstring& Warning() const { return warn_; }
    bool Saved() const { return ok_; }

private:
    Options opt_;
    std::thread th_;
    std::atomic<bool> stop_{false}, paused_{false};
    std::atomic<LONGLONG> elapsed_{0};
    std::atomic<HWND> notify_{nullptr};
    std::wstring path_, err_, warn_;
    bool ok_ = false;

    static std::wstring VideosDir() {
        PWSTR p = nullptr; std::wstring d;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, KF_FLAG_CREATE, nullptr, &p))) d = p;
        CoTaskMemFree(p);
        if (d.empty()) { wchar_t u[MAX_PATH]; if (GetEnvironmentVariableW(L"USERPROFILE", u, MAX_PATH)) d = std::wstring(u) + L"\\Videos"; }
        CreateDirectoryW(d.c_str(), nullptr);
        return d;
    }

    static bool SetVideo(IMFSinkWriter* w, UINT cw, UINT ch, DWORD* stream) {
        IMFMediaType* out = nullptr; IMFMediaType* in = nullptr;
        UINT32 br = (UINT32)((double)cw * ch * kFps * 0.1);
        if (br < 4000000) br = 4000000;
        if (br > 24000000) br = 24000000;
        bool ok = SUCCEEDED(MFCreateMediaType(&out)) &&
                  SUCCEEDED(out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) && SUCCEEDED(out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264)) &&
                  SUCCEEDED(out->SetUINT32(MF_MT_AVG_BITRATE, br)) && SUCCEEDED(out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)) &&
                  SUCCEEDED(MFSetAttributeSize(out, MF_MT_FRAME_SIZE, cw, ch)) && SUCCEEDED(MFSetAttributeRatio(out, MF_MT_FRAME_RATE, kFps, 1)) &&
                  SUCCEEDED(MFSetAttributeRatio(out, MF_MT_PIXEL_ASPECT_RATIO, 1, 1)) && SUCCEEDED(w->AddStream(out, stream)) &&
                  SUCCEEDED(MFCreateMediaType(&in)) &&
                  SUCCEEDED(in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) && SUCCEEDED(in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32)) &&
                  SUCCEEDED(in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)) && SUCCEEDED(MFSetAttributeSize(in, MF_MT_FRAME_SIZE, cw, ch)) &&
                  SUCCEEDED(MFSetAttributeRatio(in, MF_MT_FRAME_RATE, kFps, 1)) && SUCCEEDED(MFSetAttributeRatio(in, MF_MT_PIXEL_ASPECT_RATIO, 1, 1)) &&
                  SUCCEEDED(in->SetUINT32(MF_MT_DEFAULT_STRIDE, cw * 4)) && // positivo: linhas de cima para baixo
                  SUCCEEDED(w->SetInputMediaType(*stream, in, nullptr));
        Rel(out); Rel(in);
        return ok;
    }
    static bool SetAudio(IMFSinkWriter* w, DWORD* stream) {
        IMFMediaType* out = nullptr; IMFMediaType* in = nullptr;
        bool ok = SUCCEEDED(MFCreateMediaType(&out)) &&
                  SUCCEEDED(out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) && SUCCEEDED(out->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC)) &&
                  SUCCEEDED(out->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) && SUCCEEDED(out->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kRate)) &&
                  SUCCEEDED(out->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, kCh)) && SUCCEEDED(out->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000)) && // 192 kbps
                  SUCCEEDED(w->AddStream(out, stream)) &&
                  SUCCEEDED(MFCreateMediaType(&in)) &&
                  SUCCEEDED(in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) && SUCCEEDED(in->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) &&
                  SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) && SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kRate)) &&
                  SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, kCh)) && SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, kCh * 2)) &&
                  SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, kRate * kCh * 2)) &&
                  SUCCEEDED(w->SetInputMediaType(*stream, in, nullptr));
        Rel(out); Rel(in);
        return ok;
    }
    static bool WriteBytes(IMFSinkWriter* w, DWORD stream, const void* data, DWORD size, LONGLONG t, LONGLONG dur) {
        IMFMediaBuffer* buf = nullptr; IMFSample* smp = nullptr; BYTE* p = nullptr;
        bool ok = SUCCEEDED(MFCreateMemoryBuffer(size, &buf)) && SUCCEEDED(buf->Lock(&p, nullptr, nullptr));
        if (ok) { memcpy(p, data, size); buf->Unlock(); }
        ok = ok && SUCCEEDED(buf->SetCurrentLength(size)) && SUCCEEDED(MFCreateSample(&smp)) && SUCCEEDED(smp->AddBuffer(buf)) &&
             SUCCEEDED(smp->SetSampleTime(t)) && SUCCEEDED(smp->SetSampleDuration(dur)) && SUCCEEDED(w->WriteSample(stream, smp));
        Rel(smp); Rel(buf);
        return ok;
    }

    void Run(HANDLE ready, bool* started) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool mf = SUCCEEDED(MFStartup(MF_VERSION));
        Dup dup; AudioSrc src[2]; Track tracks[2];
        IMFSinkWriter* w = nullptr;
        DWORD vs = 0, as = 0;
        bool audio = false, failed = false;
        RECT crop = {};
        UINT cw = 0, ch = 0;
        HDC mem = nullptr; HBITMAP dib = nullptr; BYTE* bits = nullptr;
        std::vector<int16_t> pcm;
        do {
            if (!mf) { err_ = L"Media Foundation indisponível neste Windows"; break; }
            if (!dup.Open(opt_.mon)) { err_ = L"Não foi possível capturar a tela (Desktop Duplication)"; break; }
            MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(opt_.mon, &mi);
            crop = EvenRect(opt_.hide ? mi.rcWork : dup.desk);
            cw = (UINT)(crop.right - crop.left); ch = (UINT)(crop.bottom - crop.top);
            if (cw < 16 || ch < 16) { err_ = L"Monitor pequeno demais"; break; }
            SYSTEMTIME st; GetLocalTime(&st);
            path_ = VideosDir() + L"\\" + FileName(st);
            IMFAttributes* at = nullptr;
            MFCreateAttributes(&at, 1);
            if (at) at->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
            HRESULT hr = MFCreateSinkWriterFromURL(path_.c_str(), nullptr, at, &w);
            Rel(at);
            if (FAILED(hr)) { err_ = L"Não foi possível criar o arquivo na pasta Vídeos"; break; }
            if (!SetVideo(w, cw, ch, &vs)) { err_ = L"O codificador de vídeo (H.264) recusou este tamanho de tela"; break; }
            if (opt_.sysAudio && !src[0].Open(true)) warn_ += L"Som do computador indisponível. ";
            if (opt_.mic && !src[1].Open(false)) warn_ += L"Microfone indisponível. ";
            audio = (src[0].cap || src[1].cap) && SetAudio(w, &as);
            if ((src[0].cap || src[1].cap) && !audio) warn_ += L"Codificador de áudio indisponível. ";
            if (FAILED(w->BeginWriting())) { err_ = L"O Media Foundation não começou a gravar"; break; }
            BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = (LONG)cw; bi.bmiHeader.biHeight = -(LONG)ch;
            bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
            mem = CreateCompatibleDC(nullptr);
            dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
            if (!dib || !bits) { err_ = L"Sem memória para a imagem"; break; }
            SelectObject(mem, dib);
            *started = true;
        } while (false);
        if (!*started) {
            if (w) { w->Release(); w = nullptr; DeleteFileW(path_.c_str()); }
            for (auto& s : src) s.Close();
            dup.Close();
            if (mem) DeleteDC(mem);
            if (dib) DeleteObject(dib);
            if (mf) MFShutdown();
            CoUninitialize();
            SetEvent(ready);
            return;
        }
        SetEvent(ready);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        MediaClock clk; clk.Start(Now100ns());
        LONGLONG nextFrame = 0, audioOut = 0; // próxima amostra de áudio a escrever
        ULONGLONG lostAt = 0;
        bool wasPaused = false;
        const size_t chunk = kRate / 100; // 10 ms
        while (!stop_ && !failed) {
            LONGLONG wall = Now100ns();
            bool p = paused_;
            if (p != wasPaused) {
                if (p) clk.Pause(wall);
                else { clk.Resume(wall); for (auto& t : tracks) t.Clear(); } // o que chegou na pausa não entra
                wasPaused = p;
            }
            LONGLONG now = clk.At(wall);
            elapsed_ = now;
            // imagem nova (espera no máximo até o próximo quadro)
            LONGLONG wait = (nextFrame - now) / 10000;
            UINT to = p ? 20 : (UINT)(wait < 0 ? 0 : wait > 10 ? 10 : wait);
            if (dup.dup) {
                if (!dup.Grab(to)) { dup.Close(); lostAt = GetTickCount64(); }
            } else {
                Sleep(to ? to : 1);
                if (GetTickCount64() - lostAt > 500) { if (!dup.Open(opt_.mon)) lostAt = GetTickCount64(); } // tela segura do UAC, troca de resolução
            }
            for (int i = 0; i < 2; ++i) src[i].Drain(tracks[i], clk, !p);
            if (p) continue;
            now = clk.At(Now100ns());
            if (now >= nextFrame && dup.Read(crop, bits)) {
                if (!opt_.hide) DrawCursor(mem, crop);
                GdiFlush();
                LONGLONG t = now / kFrame * kFrame;
                if (!WriteBytes(w, vs, bits, cw * ch * 4, t, kFrame)) { failed = true; err_ = L"Falha ao gravar o vídeo (disco cheio?)"; break; }
                nextFrame = t + kFrame;
            } else if (now >= nextFrame && !dup.have) nextFrame = now / kFrame * kFrame + kFrame; // ainda sem imagem
            if (audio) {
                LONGLONG target = ToSamples(now - kAudioLag);
                while (audioOut + (LONGLONG)chunk <= target) {
                    pcm.resize(chunk * kCh);
                    Mix(tracks, 2, audioOut, chunk, pcm.data());
                    if (!WriteBytes(w, as, pcm.data(), (DWORD)(chunk * kCh * 2), FromSamples(audioOut), FromSamples(chunk))) { failed = true; err_ = L"Falha ao gravar o áudio (disco cheio?)"; break; }
                    audioOut += chunk;
                }
            }
        }
        // fim: o áudio que falta até o último quadro
        if (audio && !failed) {
            for (int i = 0; i < 2; ++i) src[i].Drain(tracks[i], clk, true);
            LONGLONG target = ToSamples(nextFrame);
            while (audioOut < target) {
                size_t n = (size_t)(target - audioOut < (LONGLONG)chunk ? target - audioOut : (LONGLONG)chunk);
                pcm.resize(n * kCh);
                Mix(tracks, 2, audioOut, n, pcm.data());
                if (!WriteBytes(w, as, pcm.data(), (DWORD)(n * kCh * 2), FromSamples(audioOut), FromSamples(n))) break;
                audioOut += n;
            }
        }
        for (auto& s : src) s.Close();
        dup.Close();
        bool fin = nextFrame > 0 && SUCCEEDED(w->Finalize());
        w->Release();
        DeleteDC(mem); DeleteObject(dib);
        ok_ = fin;
        if (!fin && nextFrame == 0) { DeleteFileW(path_.c_str()); if (err_.empty()) err_ = L"Nada foi gravado"; }
        MFShutdown();
        CoUninitialize();
        if (HWND n = notify_) PostMessageW(n, opt_.msg, ok_ ? 1 : 0, 0);
    }

    static void DrawCursor(HDC dc, const RECT& crop) {
        CURSORINFO ci = {sizeof(ci)};
        if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor) return;
        ICONINFO ii = {};
        if (!GetIconInfo(ci.hCursor, &ii)) return;
        DrawIconEx(dc, ci.ptScreenPos.x - (int)ii.xHotspot - crop.left, ci.ptScreenPos.y - (int)ii.yHotspot - crop.top, ci.hCursor, 0, 0, 0, nullptr, DI_NORMAL);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
    }
};

} // namespace rec
