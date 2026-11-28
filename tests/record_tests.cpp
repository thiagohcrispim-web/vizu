// Gravar tela: relógio com pausa, áudio alinhado na linha do tempo (silêncio do loopback vira zeros),
// mixagem sem estouro, nome do arquivo e recorte em tamanho par. A captura real não roda no CI.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include "../Record.h"
#include <iostream>
#include <stdexcept>

static int checks = 0;
static void Check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }

int wmain() {
    try {
        // relógio: a pausa não conta
        rec::MediaClock c; c.Start(1000);
        Check(c.At(1500) == 500, "relógio anda");
        c.Pause(2000);
        Check(c.At(9000) == 1000, "pausado fica parado");
        c.Resume(5000);
        Check(c.At(6000) == 2000, "depois da pausa desconta o tempo pausado");
        Check(c.At(0) == 0, "antes do início é zero");

        // trilha: pacote em 100, buraco de 1 s (loopback em silêncio) vira zeros e o som volta no lugar certo
        rec::Track t;
        float one[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        t.Push(100, one, 4);
        t.Push(100 + rec::kRate, one, 4);
        std::vector<float> acc(2 * 2, 0.0f);
        t.Take(100 + rec::kRate, 2, acc.data());
        Check(acc[0] == 0.5f && acc[3] == 0.5f, "som depois do silêncio fica no instante certo");
        std::vector<float> early(2 * 2, 0.0f);
        t.Take(100 + rec::kRate / 2, 2, early.data());
        Check(early[0] == 0.0f, "o que já passou foi descartado");

        // pequenos atrasos (< 20 ms) não abrem buraco: o pacote encosta no anterior
        rec::Track m;
        m.Push(0, one, 4);
        m.Push(10, one, 4);
        Check(m.End() == 8, "jitter pequeno é contínuo");

        // mixagem soma as duas fontes e limita em 16 bits
        rec::Track two[2];
        float loud[2] = {0.8f, -0.8f};
        two[0].Push(0, loud, 1); two[1].Push(0, loud, 1);
        int16_t out[4] = {};
        rec::Mix(two, 2, 0, 2, out);
        Check(out[0] == 32767 && out[1] == -32767, "soma limitada sem estourar");
        Check(out[2] == 0 && out[3] == 0, "sem amostra vira silêncio");

        // fonte vazia (microfone desmarcado) não atrapalha
        rec::Track solo[2];
        float half[2] = {0.25f, 0.25f};
        solo[0].Push(0, half, 1);
        rec::Mix(solo, 2, 0, 1, out);
        Check(out[0] == (int16_t)(0.25f * 32767.0f), "uma fonte só");

        // conversões de tempo
        Check(rec::ToSamples(10000000) == rec::kRate, "1 s em amostras");
        Check(rec::FromSamples(rec::kRate) == 10000000, "amostras em 1 s");

        // H.264: tamanho par
        RECT r = {0, 0, 1366, 767};
        RECT e = rec::EvenRect(r);
        Check(e.right == 1366 && e.bottom == 766, "altura ímpar vira par");
        RECT r2 = {-1921, 0, 0, 1080};
        Check(((rec::EvenRect(r2).right - rec::EvenRect(r2).left) & 1) == 0, "monitor à esquerda");

        // nome do arquivo
        SYSTEMTIME st = {}; st.wYear = 2026; st.wMonth = 11; st.wDay = 28; st.wHour = 14; st.wMinute = 32; st.wSecond = 5;
        Check(rec::FileName(st) == L"Vizu Gravação 2026-11-28 14-32-05.mp4", "nome com data e hora");
    } catch (const std::exception& e) {
        std::cerr << "FALHOU: " << e.what() << " (" << checks << " verificações antes)\n";
        return 1;
    }
    std::cout << "record: " << checks << " verificações OK\n";
    return 0;
}
