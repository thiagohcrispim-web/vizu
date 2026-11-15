// Game.inc.h - segredo: o rolê do Tobias e do Joaquim. Abre ao clicar em "Primão" no Agradecimento especial.
// Dois cachorrinhos pegam um carro emprestado; pule lombadas, fuja das poças (o carro desliza) e dos buracos
// que os meteoros abrem no asfalto. Ossos devolvem uma vida. 8 bits: tela preta, cenário em linhas cinzas.
// Leve: só existe enquanto está aberto; um retângulo por pixel lógico, 360 x 120, ampliado por um fator inteiro.

#include <wincodec.h>

namespace game {

const int W = 360, H = 120, GROUND = 100, CARX = 44;

// Carro original: esportivo vermelho conversível, de perfil, com um raio amarelo (1 caractere = 1 pixel).
static const char* const kCar[] = {
    "...................................WW...........",
    "..................................WWk...........",
    "rr...............................WWk............",
    "RRr.............................WWk.............",
    "RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRr......",
    "rRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRr...",
    "rRRRRRRRRRRRRRRRRRRRRRRYYYRRRRRRRRRRRRRRRRRRRRRr",
    "TRRRRRRRRRRRRRRRRRRRRRYYYRRRRRRRRRRRRRRRRRRRRRRL",
    "TRRRRRRRRRRRRRRRYYYYYYYYYRRRRRRRRRRRRRRRRRRRRRRL",
    "rRRRRRRRRRRRRRRRRRRRYYYRRRRRRRRRRRRRRRRRRRRRRRRr",
    "rRRRRRRkkkkkkRRRRRRYYYRRRRRRRRRRRRRRRkkkkkkRRRRr",
    ".rrrrrkkkkkkkkrrrrYrrrrrrrrrrrrrrrrkkkkkkkkrrrr.",
    "......kkGGGGkk.....................kkGGGGkk.....",
    "......kkGkkGkk.....................kkGkkGkk.....",
    "......kkGGGGkk.....................kkGGGGkk.....",
    ".......kkkkkk.......................kkkkkk......",
};
const int CARW = 48, CARH = 16;
static UINT CarColor(char c) {
    switch (c) {
    case 'R': return 0xE3262E; case 'r': return 0x9E1A20; case 'Y': return 0xFFD21F; case 'k': return 0x141414;
    case 'G': return 0x7A7A7A; case 'W': return 0x4A6E8C; case 'T': return 0xFF5A3C; case 'L': return 0xFFF2B0;
    }
    return 0;
}

// Fonte 3x5 (bits da esquerda para a direita, linha a linha).
static int Glyph(wchar_t c) {
    static const struct { wchar_t c; int m; } f[] = {
        {L'A', 0x2BED}, {L'B', 0x6BAE}, {L'C', 0x3923}, {L'D', 0x6B6E}, {L'E', 0x79A7}, {L'F', 0x79A4}, {L'G', 0x396B}, {L'H', 0x5BED}, {L'I', 0x7497}, {L'J', 0x126A}, {L'K', 0x5BAD}, {L'L', 0x4927}, {L'M', 0x5FED}, {L'N', 0x6B6D}, {L'O', 0x2B6A}, {L'P', 0x6BA4}, {L'Q', 0x2B73}, {L'R', 0x6BAD}, {L'S', 0x388E}, {L'T', 0x7492}, {L'U', 0x5B6F}, {L'V', 0x5B6A}, {L'W', 0x5BFD}, {L'X', 0x5AAD}, {L'Y', 0x5A92}, {L'Z', 0x72A7}, {L'0', 0x7B6F}, {L'1', 0x2C97}, {L'2', 0x62A7}, {L'3', 0x628E}, {L'4', 0x5BC9}, {L'5', 0x798E}, {L'6', 0x39EF}, {L'7', 0x7292}, {L'8', 0x7BEF}, {L'9', 0x7BCE}, {L'!', 0x2482}, {L':', 0x0410}, {L'.', 0x0002}, {L'-', 0x01C0},
    };
    for (auto& g : f) if (g.c == c) return g.m;
    return 0;
}

struct Ob { int kind; float x, w; bool hit; };       // 0 lombada, 1 poça, 2 buraco, 3 osso
struct Meteor { float tx, k; };                      // tx: ponto do asfalto onde cai; k: 0..1 da queda
struct Part { float x, y, vx, vy, life; UINT c; };

struct State {
    HWND dim = nullptr, wnd = nullptr;
    ui::Surface surf;
    ID2D1Bitmap* heads[4]{};                         // Tobias, Joaquim, nuca do Tobias, nuca do Joaquim
    ID2D1RenderTarget* headsRt = nullptr;
    ID2D1Bitmap* car[2]{};                           // carro pronto (duas posições das rodas)
    ID2D1Bitmap* sky[2]{};                           // prédios: uma faixa de 480 px que se repete, por camada
    ID2D1RenderTarget* cacheRt = nullptr; int cacheSc = 0;
    int phase = 0;                                   // 0 pronto, 1 jogando, 2 pausa, 3 fim do rolê
    float carY = 0, vy = 0, speed = 0, dist = 0, t = 0, gap = 0, meteorIn = 0, boneIn = 0;
    float slide = 0, invuln = 0, hurt = 0, shake = 0, lookIn = 0, lookLeft = 0, dustIn = 0, dimA = 0, countdown = 0;
    int lookWho = -1, lives = 3, best = 0, sc = 3;
    bool holding = false;
    std::vector<Ob> obs; std::vector<Meteor> mets; std::vector<Part> parts;
    LARGE_INTEGER qpf{}, last{};
    unsigned rng = 0x9E3779B9u;
};

} // namespace game

static game::State g_gm;

static float GmRand() { unsigned& x = g_gm.rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return (x & 0xFFFFFF) / 16777216.0f; }
static float GmRand(float a, float b) { return a + (b - a) * GmRand(); }

static void GmReset() {
    using namespace game;
    g_gm.carY = 0; g_gm.vy = 0; g_gm.speed = 95; g_gm.dist = 0; g_gm.t = 0; g_gm.gap = 120;
    g_gm.meteorIn = GmRand(9, 13); g_gm.boneIn = GmRand(14, 20); g_gm.slide = g_gm.invuln = g_gm.hurt = g_gm.shake = 0;
    g_gm.lookIn = GmRand(4, 8); g_gm.lookLeft = 0; g_gm.lookWho = -1; g_gm.lives = 3; g_gm.holding = false;
    g_gm.obs.clear(); g_gm.mets.clear(); g_gm.parts.clear();
}

static void GmBurst(float x, float y, int n, UINT c, float spread, float up) {
    for (int i = 0; i < n; ++i) g_gm.parts.push_back({x, y, GmRand(-spread, spread), -GmRand(up * 0.4f, up), GmRand(0.3f, 0.7f), c});
}

static void GmDamage() {
    if (g_gm.invuln > 0 || g_gm.phase != 1) return;
    g_gm.lives--; g_gm.invuln = 1.4f; g_gm.hurt = 0.7f; g_gm.shake = 0.25f;
    GmBurst(game::CARX + 40, (float)game::GROUND - 8 - g_gm.carY, 8, 0xFFD21F, 50, 60);
    if (g_gm.lives <= 0) {
        g_gm.phase = 3;
        int m = (int)(g_gm.dist / 10);
        if (m > g_gm.best) { g_gm.best = m; WriteDword(L"GameBest", (DWORD)m); }
    }
}

static void GmJump() {
    if (g_gm.phase == 0 || g_gm.phase == 3) { GmReset(); g_gm.phase = 1; return; }
    if (g_gm.phase == 2) { g_gm.phase = 1; return; }
    if (g_gm.carY <= 0 && g_gm.slide <= 0) { g_gm.vy = 150; g_gm.holding = true; }
}

static void GmUpdate(float dt) {
    using namespace game;
    if (g_gm.dimA < 1) g_gm.dimA = min(1.0f, g_gm.dimA + dt * 4);
    for (auto& p : g_gm.parts) { p.x += p.vx * dt; p.y += p.vy * dt; p.vy += 220 * dt; p.life -= dt; }
    g_gm.parts.erase(std::remove_if(g_gm.parts.begin(), g_gm.parts.end(), [](const Part& p) { return p.life <= 0; }), g_gm.parts.end());
    if (g_gm.phase == 0 && (g_gm.countdown -= dt) <= 0) { GmReset(); g_gm.phase = 1; }
    if (g_gm.phase != 1) return;
    g_gm.t += dt;
    g_gm.speed = min(230.0f, 95 + g_gm.t * 2.2f);
    float dx = g_gm.speed * dt;
    g_gm.dist += dx;
    // pulo: segurar deixa mais alto
    if (g_gm.carY > 0 || g_gm.vy > 0) {
        float g = g_gm.holding && g_gm.vy > 0 ? 300.0f : 600.0f;
        g_gm.vy -= g * dt; g_gm.carY += g_gm.vy * dt;
        if (g_gm.carY <= 0) { g_gm.carY = 0; g_gm.vy = 0; GmBurst(CARX + 10, GROUND - 1, 3, 0x55555A, 25, 25); }
    }
    bool ground = g_gm.carY <= 0;
    g_gm.slide = max(0.0f, g_gm.slide - dt); g_gm.invuln = max(0.0f, g_gm.invuln - dt);
    g_gm.hurt = max(0.0f, g_gm.hurt - dt); g_gm.shake = max(0.0f, g_gm.shake - dt);
    // poeira atrás do carro
    if (ground && (g_gm.dustIn -= dt) <= 0) { g_gm.dustIn = 0.09f; g_gm.parts.push_back({(float)CARX + 2, GROUND - 2.0f, -GmRand(20, 40), -GmRand(5, 15), 0.35f, 0x3A3A40}); }
    // um dos dois olha para trás de vez em quando
    if (g_gm.lookLeft > 0) g_gm.lookLeft -= dt;
    else if ((g_gm.lookIn -= dt) <= 0) { g_gm.lookWho = GmRand() < 0.5f ? 0 : 1; g_gm.lookLeft = 0.5f; g_gm.lookIn = GmRand(4, 9); }
    // obstáculos andam com o asfalto
    for (auto& o : g_gm.obs) o.x -= dx;
    for (auto& m : g_gm.mets) { m.tx -= dx; m.k += dt / 1.3f; }
    g_gm.obs.erase(std::remove_if(g_gm.obs.begin(), g_gm.obs.end(), [](const Ob& o) { return o.x + o.w < -10; }), g_gm.obs.end());
    // meteoros caem e abrem buracos
    for (auto& m : g_gm.mets) if (m.k >= 1) {
        g_gm.obs.push_back({2, m.tx - 10, 20, false});
        GmBurst(m.tx, GROUND - 1, 10, 0x6A6A70, 60, 70);
        if (m.tx > CARX && m.tx < CARX + CARW && g_gm.carY < 10) GmDamage(); // caiu em cima do carro
    }
    g_gm.mets.erase(std::remove_if(g_gm.mets.begin(), g_gm.mets.end(), [](const Meteor& m) { return m.k >= 1; }), g_gm.mets.end());
    // novos obstáculos (distância entre eles cresce com a velocidade)
    float lastX = -1000; for (auto& o : g_gm.obs) if (o.kind != 3) lastX = max(lastX, o.x + o.w);
    if (W - lastX > g_gm.gap) {
        bool puddle = g_gm.t > 6 && GmRand() < 0.35f;
        g_gm.obs.push_back({puddle ? 1 : 0, (float)W + 4, puddle ? 24.0f : 14.0f, false});
        g_gm.gap = GmRand(110, 210) + g_gm.speed * 0.35f;
    }
    if (g_gm.t > 10 && (g_gm.meteorIn -= dt) <= 0) {
        g_gm.mets.push_back({CARX + CARW + 60 + g_gm.speed * 1.3f + GmRand(0, 40), 0}); // cai bem à frente: dá para ver o aviso e pular
        g_gm.meteorIn = GmRand(5, 9);
    }
    if ((g_gm.boneIn -= dt) <= 0) { g_gm.obs.push_back({3, (float)W + 4, 9, false}); g_gm.boneIn = GmRand(16, 26); }
    // colisões
    float w1 = CARX + 7, w2 = CARX + 40; // centro das rodas
    for (auto& o : g_gm.obs) {
        if (o.hit) continue;
        bool overBody = o.x < CARX + CARW - 2 && o.x + o.w > CARX + 2;
        if (o.kind == 0 && overBody && g_gm.carY < 4) { o.hit = true; GmDamage(); }
        else if (o.kind == 1 && ground && ((w1 > o.x && w1 < o.x + o.w) || (w2 > o.x && w2 < o.x + o.w))) { // poça: desliza, sem pular
            o.hit = true; g_gm.slide = 0.8f; GmBurst(w2, GROUND - 1, 8, 0x4B6F8F, 40, 50);
        } else if (o.kind == 2 && ground && ((w1 > o.x + 3 && w1 < o.x + o.w - 3) || (w2 > o.x + 3 && w2 < o.x + o.w - 3))) { o.hit = true; GmDamage(); }
        else if (o.kind == 3 && overBody && GROUND - g_gm.carY - 26 < GROUND - 20) { // osso: precisa pular para pegar
            o.hit = true; if (g_gm.lives < 3) g_gm.lives++; else g_gm.dist += 300;
            GmBurst(o.x + 4, GROUND - 22, 8, 0xFFD21F, 40, 50);
        }
    }
    g_gm.obs.erase(std::remove_if(g_gm.obs.begin(), g_gm.obs.end(), [](const Ob& o) { return o.kind == 3 && o.hit; }), g_gm.obs.end());
}

// Carrega as cabeças (PNG embutidos) como bitmaps do alvo atual.
static void GmLoadHeads(ID2D1RenderTarget* rt) {
    if (g_gm.headsRt == rt) return;
    for (auto& b : g_gm.heads) if (b) { b->Release(); b = nullptr; }
    g_gm.headsRt = rt;
    IWICImagingFactory* wic = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) return;
    for (int i = 0; i < 4; ++i) {
        HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(310 + i), RT_RCDATA);
        HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
        if (!g) continue;
        IStream* st = SHCreateMemStream((const BYTE*)LockResource(g), SizeofResource(nullptr, r));
        IWICBitmapDecoder* dec = nullptr; IWICBitmapFrameDecode* fr = nullptr; IWICFormatConverter* cv = nullptr;
        if (st && SUCCEEDED(wic->CreateDecoderFromStream(st, nullptr, WICDecodeMetadataCacheOnLoad, &dec)) && SUCCEEDED(dec->GetFrame(0, &fr)) &&
            SUCCEEDED(wic->CreateFormatConverter(&cv)) && SUCCEEDED(cv->Initialize(fr, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
            rt->CreateBitmapFromWicBitmap(cv, nullptr, &g_gm.heads[i]);
        if (cv) cv->Release(); if (fr) fr->Release(); if (dec) dec->Release(); if (st) st->Release();
    }
    wic->Release();
}

const int SKYW = 480, SKYH = 64; // faixa dos prédios: de GROUND - 64 até GROUND

static void GmFreeCache() {
    for (auto& b : g_gm.car) if (b) { b->Release(); b = nullptr; }
    for (auto& b : g_gm.sky) if (b) { b->Release(); b = nullptr; }
    g_gm.cacheRt = nullptr;
}

// Desenha uma vez o carro e os prédios em bitmaps; cada quadro só copia.
static void GmBuildCache(ID2D1RenderTarget* rt, int sc) {
    using namespace game;
    if (g_gm.cacheRt == rt && g_gm.cacheSc == sc) return;
    GmFreeCache();
    g_gm.cacheRt = rt; g_gm.cacheSc = sc;
    auto make = [&](float w, float h, const std::function<void(const std::function<void(float, float, float, float, UINT)>&)>& draw) -> ID2D1Bitmap* {
        ID2D1BitmapRenderTarget* bt = nullptr; ID2D1Bitmap* out = nullptr;
        if (FAILED(rt->CreateCompatibleRenderTarget(D2D1::SizeF(w * sc, h * sc), &bt))) return nullptr;
        ID2D1SolidColorBrush* br = nullptr;
        bt->BeginDraw(); bt->Clear(D2D1::ColorF(0, 0)); bt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        if (SUCCEEDED(bt->CreateSolidColorBrush(D2D1::ColorF(0), &br))) {
            draw([&](float x, float y, float ww, float hh, UINT c) { br->SetColor(D2D1::ColorF(c)); bt->FillRectangle(D2D1::RectF(floorf(x) * sc, floorf(y) * sc, (floorf(x) + ww) * sc, (floorf(y) + hh) * sc), br); });
            br->Release();
        }
        bt->EndDraw(); bt->GetBitmap(&out); bt->Release();
        return out;
    };
    for (int f = 0; f < 2; ++f)
        g_gm.car[f] = make((float)CARW, (float)CARH, [&](const std::function<void(float, float, float, float, UINT)>& px) {
            for (int r = 0; r < CARH; ++r) for (int c = 0; c < CARW; ++c) {
                char ch = kCar[r][c];
                if (r == 13 && f && (c == 9 || c == 10 || c == 37 || c == 38)) ch = ch == 'k' ? 'G' : 'k'; // rodas girando
                if (UINT col = CarColor(ch)) px((float)c, (float)r, 1, 1, col);
            }
        });
    for (int layer = 0; layer < 2; ++layer) // prédios: só o contorno, em cinza de pouco contraste
        g_gm.sky[layer] = make((float)SKYW, (float)SKYH, [&](const std::function<void(float, float, float, float, UINT)>& px) {
            UINT col = layer ? 0x34343A : 0x232327; float base = SKYH - (layer ? 4.0f : 10.0f);
            unsigned r = layer ? 77 : 31; float bx = 0, prevH = 0, firstH = -1;
            while (bx < SKYW) {
                r = r * 1103515245u + 12345u; float bw = 14.0f + (r >> 16) % 22, bh = 8.0f + ((r >> 8) % (layer ? 18 : 30));
                if (bx + bw > SKYW - 10) bw = SKYW - bx; // a faixa fecha certinho e se repete sem emenda
                if (firstH < 0) firstH = bh;
                px(bx, base - bh, bw, 1, col);
                if (bx > 0) px(bx, base - max(bh, prevH), 1, fabsf(bh - prevH) + 1, col);
                if (bh > 14) for (float wy = base - bh + 4; wy < base - 3; wy += 5) for (float wx = bx + 3; wx < bx + bw - 2; wx += 5) px(wx, wy, 1, 1, col);
                if (!layer && ((r >> 4) % 9) == 0 && bw > 12) { px(bx + bw / 2 - 3, base - bh - 9, 6, 4, col); px(bx + bw / 2 - 2, base - bh - 5, 1, 5, col); px(bx + bw / 2 + 1, base - bh - 5, 1, 5, col); } // caixa d'água
                prevH = bh; bx += bw;
            }
            px(SKYW - 1, base - max(prevH, firstH), 1, fabsf(prevH - firstH) + 1, col); // parede da emenda
        });
}

static void GmPaint(HWND h) {
    using namespace game;
    g_gm.surf.hwnd = h;
    if (!g_gm.surf.Begin(g_d2d)) return;
    ID2D1RenderTarget* rt = g_gm.surf.rt;
    rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    GmLoadHeads(rt);
    GmBuildCache(rt, g_gm.sc);
    rt->Clear(D2D1::ColorF(0x000000));
    int sc = g_gm.sc;
    float ox = 0, oy = 0;
    if (g_gm.shake > 0) { ox = (float)((int)GmRand(-2, 3) * sc / 2); oy = (float)((int)GmRand(-1, 2) * sc / 2); }
    ID2D1SolidColorBrush* br = nullptr;
    if (FAILED(rt->CreateSolidColorBrush(D2D1::ColorF(0), &br))) { g_gm.surf.End(); return; }
    auto px = [&](float x, float y, float w, float hh, UINT c, float a = 1) {
        br->SetColor(D2D1::ColorF(c, a));
        float X = ox + floorf(x) * sc, Y = oy + floorf(y) * sc;
        rt->FillRectangle(D2D1::RectF(X, Y, X + w * sc, Y + hh * sc), br);
    };
    auto text = [&](const std::wstring& s0, float x, float y, int z, UINT c, bool center = false) {
        std::wstring s = s0; for (auto& ch : s) ch = (wchar_t)towupper(ch);
        if (center) x -= (s.size() * 4 - 1) * z / 2.0f;
        for (wchar_t ch : s) {
            wchar_t b = ch, acc = 0;
            if (ch == L'Ç') { b = L'C'; acc = 1; } else if (ch == L'Ê' || ch == L'É' || ch == L'Ô' || ch == L'Ó' || ch == L'Á' || ch == L'Ã' || ch == L'Í' || ch == L'Ú') { b = ch == L'Ê' || ch == L'É' ? L'E' : ch == L'Ô' || ch == L'Ó' ? L'O' : ch == L'Í' ? L'I' : ch == L'Ú' ? L'U' : L'A'; acc = 2; }
            int m = Glyph(b);
            for (int r = 0; r < 5; ++r) for (int k = 0; k < 3; ++k) if (m >> (14 - r * 3 - k) & 1) px(x + k * z, y + r * z, (float)z, (float)z, c);
            if (acc == 1) px(x + 1 * z, y + 5 * z, (float)z, (float)z, c);
            if (acc == 2) px(x + 1 * z, y - 2 * z, (float)z, (float)z, c);
            x += 4 * z;
        }
    };
    float farOff = g_gm.dist * 0.15f, midOff = g_gm.dist * 0.35f;
    // estrelas e nuvens
    for (int i = 0; i < 22; ++i) { float sx = fmodf(i * 97.3f - farOff * 0.2f + 4000, (float)W), sy = 6 + fmodf(i * 37.7f, 52.0f); px(sx, sy, 1, 1, 0x3A3A40, (i + (int)(g_gm.t * 2)) % 7 ? 1.0f : 0.4f); }
    for (int i = 0; i < 3; ++i) {
        float cx = fmodf(i * 150 + 40 - farOff * 0.5f + 6000, (float)W + 60) - 30, cy = 16.0f + i * 11;
        px(cx, cy + 4, 26, 1, 0x26262A); px(cx + 4, cy + 1, 1, 3, 0x26262A); px(cx + 4, cy + 1, 7, 1, 0x26262A); px(cx + 11, cy - 1, 1, 2, 0x26262A);
        px(cx + 11, cy - 1, 7, 1, 0x26262A); px(cx + 18, cy - 1, 1, 3, 0x26262A); px(cx + 18, cy + 2, 4, 1, 0x26262A); px(cx + 22, cy + 2, 1, 2, 0x26262A);
    }
    // prédios (do cache), duas camadas em velocidades diferentes
    for (int layer = 0; layer < 2; ++layer) {
        if (!g_gm.sky[layer]) continue;
        float off = fmodf(layer ? midOff : farOff, (float)SKYW);
        for (int rep2 = 0; rep2 < 2; ++rep2) {
            float X = ox + floorf(rep2 * SKYW - off) * sc, Y = oy + (GROUND - SKYH) * sc;
            rt->DrawBitmap(g_gm.sky[layer], D2D1::RectF(X, Y, X + SKYW * sc, Y + SKYH * sc), 1, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        }
    }
    // asfalto
    px(0, GROUND, W, 1, 0x4A4A50);
    for (float x = -fmodf(g_gm.dist, 16.0f); x < W; x += 16) px(x, GROUND + 7, 8, 1, 0x2E2E33);
    px(0, GROUND + 14, W, 1, 0x232327);
    // obstáculos
    for (auto& o : g_gm.obs) {
        if (o.kind == 0) { // lombada com faixas
            for (int r = 0; r < 4; ++r) px(o.x + 4 - r, (float)GROUND - 4 + r, o.w - 8 + 2 * r, 1, 0x6A6A70);
            for (int k = 0; k < 4; ++k) px(o.x + 3 + k * 3, (float)GROUND - 3, 1, 2, 0x8C7A2E);
        } else if (o.kind == 1) { // poça
            px(o.x + 3, GROUND - 1, o.w - 6, 1, 0x3A6284); px(o.x, GROUND, o.w, 2, 0x3A6284); px(o.x + 5, GROUND, 4, 1, 0x7FA6C8); px(o.x + 15, GROUND - 1, 3, 1, 0x7FA6C8);
        } else if (o.kind == 2) { // buraco de meteoro
            px(o.x, GROUND, o.w, 4, 0x000000); px(o.x + 2, GROUND + 4, o.w - 4, 1, 0x000000);           // o buraco corta o asfalto
            px(o.x - 2, GROUND - 1, 4, 1, 0x6A6A70); px(o.x + o.w - 2, GROUND - 1, 4, 1, 0x6A6A70);        // borda levantada
            px(o.x - 3, GROUND - 2, 1, 1, 0x55555A); px(o.x + o.w + 2, GROUND - 2, 1, 1, 0x55555A);
            px(o.x + 3, GROUND - 2, 1, 1, 0x55555A); px(o.x + o.w - 4, GROUND - 2, 1, 1, 0x55555A);        // lascas
            px(o.x - 5, GROUND - 3, 1, 1, 0x3A3A40); px(o.x + o.w + 4, GROUND - 3, 1, 1, 0x3A3A40);
        } else { // osso flutuando
            float by = GROUND - 22 + (sinf(g_gm.t * 6) > 0 ? 0.0f : 1.0f);
            px(o.x + 1, by, 7, 1, 0xCFC7B6); px(o.x, by - 1, 2, 1, 0xCFC7B6); px(o.x, by + 1, 2, 1, 0xCFC7B6); px(o.x + 7, by - 1, 2, 1, 0xCFC7B6); px(o.x + 7, by + 1, 2, 1, 0xCFC7B6);
        }
    }
    // meteoros: aviso piscando no asfalto e a pedra caindo com rastro
    for (auto& m : g_gm.mets) {
        if (((int)(m.k * 10)) % 2 == 0) { px(m.tx - 2, GROUND - 3, 1, 1, 0x7A3A3A); px(m.tx + 2, GROUND - 3, 1, 1, 0x7A3A3A); px(m.tx, GROUND - 2, 1, 1, 0x7A3A3A); px(m.tx - 2, GROUND - 1, 1, 1, 0x7A3A3A); px(m.tx + 2, GROUND - 1, 1, 1, 0x7A3A3A); }
        float mx = m.tx + 90 * (1 - m.k), my = -6 + (GROUND + 6) * m.k;
        for (int k = 1; k < 12; ++k) px(mx + k * 2.0f, my - k * 2.2f, 2, 2, 0x6A6A70, 1 - k / 13.0f); // rastro
        px(mx - 2, my - 1, 5, 3, 0x8A8A90); px(mx - 1, my - 2, 3, 5, 0x8A8A90); px(mx - 1, my - 1, 2, 2, 0xD0D0D4); // pedra
    }
    // carro + cabeças
    bool blink = g_gm.phase == 1 && g_gm.invuln > 0 && ((int)(g_gm.invuln * 12)) % 2 == 0; // pisca só jogando (no fim, o carro fica)
    if (!blink) {
        float wob = g_gm.slide > 0 ? (((int)(g_gm.slide * 20)) % 2 ? 1.0f : -1.0f) : 0;
        float bump = g_gm.carY <= 0 && g_gm.phase == 1 && ((int)(g_gm.dist / 6)) % 5 == 0 ? 1.0f : 0; // suspensão
        float cx = CARX + wob + (g_gm.slide > 0 ? 3 : 0), cy = GROUND - CARH - g_gm.carY - bump + 1;
        // cabeças (meia escala do mundo, para os rostos aparecerem); balançam fora de fase e atrasam no pulo
        for (int i = 0; i < 2; ++i) {
            bool back = g_gm.hurt > 0 || (g_gm.lookLeft > 0 && g_gm.lookWho == i);
            ID2D1Bitmap* b = g_gm.heads[back ? 2 + i : i];
            if (!b) continue;
            D2D1_SIZE_F bs = b->GetSize();
            float hw = bs.width * sc / 2, hh = bs.height * sc / 2;
            float bob = (g_gm.phase == 1 ? (sinf(g_gm.t * 13 + i * 2.1f) > 0.3f ? 1.0f : 0) : 0) + (g_gm.vy > 30 ? 1 : g_gm.vy < -30 ? -1 : 0);
            float hx = ox + (cx + (i ? 19 : 7)) * sc, hy = oy + (cy + 5 + bob) * sc - hh;
            rt->DrawBitmap(b, D2D1::RectF(floorf(hx), floorf(hy), floorf(hx + hw), floorf(hy + hh)), 1, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        }
        if (ID2D1Bitmap* cb = g_gm.car[((int)(g_gm.dist / 4)) % 2]) {
            float X = ox + floorf(cx) * sc, Y = oy + floorf(cy) * sc;
            rt->DrawBitmap(cb, D2D1::RectF(X, Y, X + CARW * sc, Y + CARH * sc), 1, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        }
    }
    for (auto& p : g_gm.parts) px(p.x, p.y, 1, 1, p.c, min(1.0f, p.life * 2));
    // placar: vidas e distância
    for (int i = 0; i < 3; ++i) {
        UINT c = i < g_gm.lives ? 0xE3262E : 0x2A2A2E; float hx = 8.0f + i * 10, hy = 7;
        px(hx + 1, hy, 2, 1, c); px(hx + 4, hy, 2, 1, c); px(hx, hy + 1, 7, 2, c); px(hx + 1, hy + 3, 5, 1, c); px(hx + 2, hy + 4, 3, 1, c); px(hx + 3, hy + 5, 1, 1, c);
    }
    wchar_t sb[32]; swprintf(sb, 32, L"%06d M", (int)(g_gm.dist / 10));
    text(sb, W - 8.0f - (wcslen(sb) * 4 - 1) * 2, 6, 2, 0xD8D8DC);
    if (g_gm.best) { swprintf(sb, 32, L"%ls %06d", T(L"REC", L"BEST", L"REC"), g_gm.best); text(sb, W - 8.0f - (wcslen(sb) * 4 - 1), 19, 1, 0x55555A); }
    if (g_gm.phase == 0) {
        text(T(L"O ROLÊ DO TOBIAS E DO JOAQUIM", L"TOBIAS AND JOAQUIM GO FOR A RIDE", L"EL PASEO DE TOBIAS Y JOAQUIM"), W / 2.0f, 30, 1, 0xD8D8DC, true);
        wchar_t cd[4]; swprintf(cd, 4, L"%d", max(1, (int)ceilf(g_gm.countdown)));
        text(cd, W / 2.0f, 40, 2, 0xD8D8DC, true);
        text(T(L"ESPAÇO OU CLIQUE PULA · SEGURE PARA PULAR MAIS · ESC SAI", L"SPACE OR CLICK TO JUMP · HOLD TO JUMP HIGHER · ESC QUITS", L"ESPACIO O CLIC SALTA · MANTÉN PARA SALTAR MÁS · ESC SALE"), W / 2.0f, 54, 1, 0x55555A, true);
    } else if (g_gm.phase == 2) text(T(L"PAUSA", L"PAUSED", L"PAUSA"), W / 2.0f, 46, 2, 0xD8D8DC, true);
    else if (g_gm.phase == 3) {
        text(T(L"FIM DO ROLÊ", L"END OF THE RIDE", L"FIN DEL PASEO"), W / 2.0f, 40, 2, 0xD8D8DC, true);
        text(T(L"ESPAÇO PARA OUTRA VOLTA", L"SPACE FOR ANOTHER LAP", L"ESPACIO PARA OTRA VUELTA"), W / 2.0f, 58, 1, 0x8A8A90, true);
    }
    br->Release();
    if (!g_gm.surf.End()) { for (auto& b : g_gm.heads) if (b) { b->Release(); b = nullptr; } g_gm.headsRt = nullptr; GmFreeCache(); } // alvo perdido: refaz tudo
}

static void GmClose() {
    HWND w = g_gm.wnd, d = g_gm.dim;
    g_gm.wnd = g_gm.dim = nullptr;
    if (w) DestroyWindow(w);
    if (d) DestroyWindow(d);
}

static LRESULT CALLBACK GmProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER: {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - g_gm.last.QuadPart) / g_gm.qpf.QuadPart;
        g_gm.last = now;
        GmUpdate(min(dt, 0.05f));
        if (g_gm.dim) SetLayeredWindowAttributes(g_gm.dim, 0, (BYTE)(200 * g_gm.dimA), LWA_ALPHA); // a tela escurece aos poucos
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { GmClose(); return 0; }
        if ((wp == VK_SPACE || wp == VK_UP || wp == 'W' || wp == VK_RETURN) && !(lp & 0x40000000)) GmJump(); // sem repetição de tecla
        if (wp == 'P' && (g_gm.phase == 1 || g_gm.phase == 2)) g_gm.phase = g_gm.phase == 1 ? 2 : 1;
        return 0;
    case WM_KEYUP: if (wp == VK_SPACE || wp == VK_UP || wp == 'W') g_gm.holding = false; return 0;
    case WM_LBUTTONDOWN: GmJump(); SetCapture(h); return 0;
    case WM_LBUTTONUP: g_gm.holding = false; ReleaseCapture(); return 0;
    case WM_ACTIVATE: if (LOWORD(wp) == WA_INACTIVE && g_gm.phase == 1) g_gm.phase = 2; return 0; // saiu da janela: pausa
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(h, &ps); GmPaint(h); EndPaint(h, &ps); return 0; }
    case WM_DESTROY:
        KillTimer(h, 1);
        for (auto& b : g_gm.heads) if (b) { b->Release(); b = nullptr; }
        g_gm.headsRt = nullptr; GmFreeCache(); g_gm.surf.Release();
        g_gm.obs.clear(); g_gm.obs.shrink_to_fit(); g_gm.parts.clear(); g_gm.parts.shrink_to_fit(); g_gm.mets.clear();
        if (g_gm.wnd == h) { g_gm.wnd = nullptr; if (g_gm.dim) { DestroyWindow(g_gm.dim); g_gm.dim = nullptr; } }
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static LRESULT CALLBACK GmDimProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_LBUTTONDOWN) { GmClose(); return 0; } // clicar fora fecha
    if (msg == WM_ERASEBKGND) { RECT r; GetClientRect(h, &r); FillRect((HDC)wp, &r, (HBRUSH)GetStockObject(BLACK_BRUSH)); return 1; }
    return DefWindowProcW(h, msg, wp, lp);
}

static void StartGame() {
    if (g_gm.wnd) { SetForegroundWindow(g_gm.wnd); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {}; wc.hInstance = g_inst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpfnWndProc = GmDimProc; wc.lpszClassName = L"VizuGameDim"; RegisterClassW(&wc);
        wc.lpfnWndProc = GmProc; wc.lpszClassName = L"VizuGame"; RegisterClassW(&wc);
        reg = true;
    }
    POINT pt; GetCursorPos(&pt);
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
    RECT m = mi.rcMonitor;
    int mw = m.right - m.left, mh = m.bottom - m.top;
    g_gm.sc = max(1, min(mw * 8 / 10 / game::W, mh * 6 / 10 / game::H));
    int w = game::W * g_gm.sc, h = game::H * g_gm.sc;
    g_gm.dim = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"VizuGameDim", L"", WS_POPUP, m.left, m.top, mw, mh, nullptr, nullptr, g_inst, nullptr);
    SetLayeredWindowAttributes(g_gm.dim, 0, 0, LWA_ALPHA);
    ShowWindow(g_gm.dim, SW_SHOWNOACTIVATE);
    g_gm.wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"VizuGame", L"Tobias e Joaquim", WS_POPUP, m.left + (mw - w) / 2, m.top + (mh - h) / 2, w, h,
                               g_gm.dim, nullptr, g_inst, nullptr);
    g_gm.best = (int)ReadDword(L"GameBest", 0);
    g_gm.phase = 0; g_gm.countdown = 2.4f; g_gm.dimA = 0; g_gm.rng ^= GetTickCount(); // 3, 2, 1 e começa sozinho
    GmReset();
    QueryPerformanceFrequency(&g_gm.qpf); QueryPerformanceCounter(&g_gm.last);
    ShowWindow(g_gm.wnd, SW_SHOW);
    SetForegroundWindow(g_gm.wnd);
    SetTimer(g_gm.wnd, 1, 15, nullptr);
}
