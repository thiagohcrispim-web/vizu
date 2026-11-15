// Limpeza: o que pode sair, o que fica, e que nada fora das pastas permitidas é tocado.
// Mesmos casos dos testes de segurança do Crispy Clean, numa pasta de teste isolada.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include "../Clean.h"
#include <iostream>
#include <stdexcept>

static int checks = 0;
static void Check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }

static bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
static void Dir(const std::wstring& p) { CreateDirectoryW(p.c_str(), nullptr); }
static void File(const std::wstring& p, size_t bytes) {
    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    std::string data(bytes, 'x'); DWORD w = 0;
    if (h != INVALID_HANDLE_VALUE) { WriteFile(h, data.data(), (DWORD)bytes, &w, nullptr); CloseHandle(h); }
}
// Deixa arquivo ou pasta com data de "days" dias atrás.
static void Age(const std::wstring& p, int days) {
    HANDLE h = CreateFileW(p.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    ULONGLONG t = clean::Now() - (ULONGLONG)days * clean::kDay;
    FILETIME ft = {(DWORD)t, (DWORD)(t >> 32)};
    if (h != INVALID_HANDLE_VALUE) { SetFileTime(h, &ft, &ft, &ft); CloseHandle(h); }
}
static void RmTree(const std::wstring& p) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((p + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            std::wstring c = p + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { RemoveDirectoryW(c.c_str()) || DeleteFileW(c.c_str()); }
            else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RmTree(c);
            else { SetFileAttributesW(c.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileW(c.c_str()); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(p.c_str());
}
static clean::Target T(const std::wstring& paths, int ageHours = 0, const wchar_t* pattern = L"*", const wchar_t* exclude = L"", bool recursive = true, int keep = 0) {
    static std::vector<std::wstring> keepAlive; // a tabela guarda ponteiros
    keepAlive.push_back(paths);
    return {clean::kGSystem, L"teste", nullptr, keepAlive.back().c_str(), clean::kSafe, ageHours, L"", pattern, exclude, recursive, keep, clean::kFolder, true, L"", L"teste"};
}
static clean::Outcome Run(const clean::Target& t) {
    clean::Leaf l; l.custom = &t; l.on = true;
    std::atomic<bool> stop(false);
    return clean::CleanLeaf(l, 1, true, stop);
}
static clean::Leaf Measure(const clean::Target& t) { clean::Leaf l; l.custom = &t; clean::MeasureLeaf(l, 1, nullptr); return l; }

int wmain() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        std::wstring base = std::wstring(tmp) + L"vizu-clean-tests-" + std::to_wstring(GetCurrentProcessId());
        RmTree(clean::Lp(base)); // sobra de uma execução que falhou (o Wine repete o número do processo)
        base = clean::RealPath((Dir(base), base));
        std::wstring allowed = base + L"\\perfil", outside = base + L"\\fora", personal = allowed + L"\\Documentos", own = allowed + L"\\Vizu";
        Dir(allowed); Dir(outside); Dir(personal); Dir(own);
        clean::Context& ctx = clean::Ctx();
        std::wstring appdata = allowed + L"\\AppData", sys = base + L"\\Windows";
        Dir(appdata); Dir(sys);
        ctx.allowed = {allowed, sys}; ctx.temp.clear(); ctx.personal = {personal}; ctx.own = {own}; ctx.ancestors = {allowed, appdata}; ctx.system = {sys};

        // --- utilidades ---
        Check(clean::Match(L"*.nupkg", L"app-1.2.nupkg") && !clean::Match(L"*.nupkg", L"app.nupkg.txt"), "curinga de nome");
        Check(clean::Match(L"photoshop temp*", L"Photoshop Temp12345"), "curinga sem diferenciar maiúsculas");
        Check(clean::VersionLess(L"app-1.0.9", L"app-1.0.10") && !clean::VersionLess(L"app-2.0", L"app-1.9"), "versão numérica");
        Check(clean::BlockedExt(L"C:\\x\\arte.PSD") && clean::BlockedExt(L"C:\\x\\a.indd") && !clean::BlockedExt(L"C:\\x\\cache.tmp"), "arquivos de trabalho");
        Check(clean::Inside(L"C:\\a\\b", L"C:\\A\\B\\c") && !clean::Inside(L"C:\\a\\b", L"C:\\a\\bc") && !clean::Inside(L"C:\\a\\b", L"C:\\a\\b"), "dentro de");

        // --- SafeRoot ---
        std::wstring cache = allowed + L"\\App\\Cache";
        Dir(allowed + L"\\App"); Dir(cache);
        Check(clean::SafeRoot(cache), "pasta dentro do perfil é permitida");
        Check(!clean::SafeRoot(allowed), "a raiz permitida em si não é limpa");
        Check(!clean::SafeRoot(outside), "pasta fora do perfil é recusada");
        Check(!clean::SafeRoot(L"C:\\"), "raiz do disco é recusada");
        Check(!clean::SafeRoot(allowed + L"\\App\\..\\..\\fora"), "caminho com .. é recusado");
        Check(!clean::SafeRoot(allowed + L"\\naoexiste"), "pasta inexistente é recusada");
        Check(!clean::SafeRoot(L"perfil\\App"), "caminho relativo é recusado");
        Dir(personal + L"\\Cache");
        Check(!clean::SafeRoot(personal) && !clean::SafeRoot(personal + L"\\Cache"), "nada dentro de pasta pessoal");
        Check(!clean::SafeRoot(own), "pasta do próprio Vizu é recusada");
        Check(!clean::SafeRoot(appdata), "AppData em si nunca é alvo");
        Dir(appdata + L"\\App"); Check(clean::SafeRoot(appdata + L"\\App"), "pasta dentro de AppData pode ser alvo");
        Dir(sys + L"\\Temp"); Check(!clean::SafeRoot(sys + L"\\Temp"), "nada dentro do Windows (pede administrador)");

        // --- curingas e versões ---
        Dir(allowed + L"\\Nav"); Dir(allowed + L"\\Nav\\Default"); Dir(allowed + L"\\Nav\\Profile 1"); Dir(allowed + L"\\Nav\\Vazio");
        Dir(allowed + L"\\Nav\\Default\\Cache"); Dir(allowed + L"\\Nav\\Profile 1\\Cache");
        Check(clean::Roots(T(allowed + L"\\Nav\\*\\Cache")).size() == 2, "um Cache por perfil, sem inventar pasta");
        Dir(allowed + L"\\Up"); for (auto v : {L"app-1.0.9", L"app-1.0.10", L"app-1.0.2"}) Dir(allowed + L"\\Up\\" + v);
        auto old = clean::Roots(T(allowed + L"\\Up\\app-*", 0, L"*", L"", true, 1));
        Check(old.size() == 2, "manter a versão mais nova");
        for (auto& r : old) Check(r.find(L"1.0.10") == std::wstring::npos, "a mais nova (1.0.10) nunca é raiz");

        // --- idade, padrão e exclusões ---
        File(cache + L"\\velho.tmp", 1000); Age(cache + L"\\velho.tmp", 3);
        File(cache + L"\\novo.tmp", 500);
        Dir(cache + L"\\sub"); File(cache + L"\\sub\\a.bin", 200); Age(cache + L"\\sub\\a.bin", 5); Age(cache + L"\\sub", 5); // pasta criada há pouco fica (pode ser de um programa rodando)
        File(cache + L"\\arte.psd", 4000); Age(cache + L"\\arte.psd", 9);
        File(cache + L"\\manter.lock", 50); Age(cache + L"\\manter.lock", 9);
        auto m = Measure(T(cache, 24, L"*", L"*.lock"));
        Check(m.bytes == 1200 && m.files == 2, "com idade mínima: só os velhos; .psd e exclusões fora");
        Check(m.protectedBytes == 4000 && m.protectedFiles == 1, ".psd contado como protegido");
        Check(Measure(T(cache, 0, L"*.tmp")).bytes == 1500, "padrão de nome");
        Check(Measure(T(cache, 0, L"*", L"", false)).bytes == 1550, "sem recursão: só a primeira pasta");

        // --- limpeza ---
        auto o = Run(T(cache, 24, L"*", L"*.lock"));
        Check(!Exists(cache + L"\\velho.tmp") && !Exists(cache + L"\\sub\\a.bin"), "o que é velho saiu");
        Check(!Exists(cache + L"\\sub"), "subpasta vazia saiu");
        Check(Exists(cache + L"\\novo.tmp"), "o que é novo ficou");
        Check(Exists(cache + L"\\arte.psd"), "arquivo de trabalho nunca sai");
        Check(Exists(cache + L"\\manter.lock"), "exclusão respeitada");
        Check(Exists(cache), "a pasta-raiz nunca sai");
        Check(o.removed == 1200 && o.filesRemoved == 2, "conta o que saiu");

        // --- arquivo mexido depois da análise fica (a limpeza refaz a varredura) ---
        File(cache + L"\\mudou.tmp", 400); Age(cache + L"\\mudou.tmp", 3);
        Check(Measure(T(cache, 24)).bytes >= 400, "entrou na análise");
        File(cache + L"\\mudou.tmp", 400); // reescrito agora
        Run(T(cache, 24));
        Check(Exists(cache + L"\\mudou.tmp"), "arquivo que voltou a ser usado depois da análise fica");

        // --- arquivo em uso fica ---
        File(cache + L"\\preso.tmp", 700);
        HANDLE lock = CreateFileW((cache + L"\\preso.tmp").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        auto o3 = Run(T(cache, 0, L"preso.tmp"));
        if (lock != INVALID_HANDLE_VALUE) {
            Check(Exists(cache + L"\\preso.tmp"), "arquivo aberto por outro programa fica");
            Check(o3.kept == 700 && o3.removed == 0, "e é contado como 'ficou'");
            CloseHandle(lock);
        }

        // --- somente leitura fica (como no Crispy Clean) ---
        File(cache + L"\\ro.tmp", 10); SetFileAttributesW((cache + L"\\ro.tmp").c_str(), FILE_ATTRIBUTE_READONLY);
        Run(T(cache, 0, L"ro.tmp"));
        Check(Exists(cache + L"\\ro.tmp"), "somente leitura não é forçado");

        // --- links: nunca seguidos ---
        File(outside + L"\\importante.txt", 100);
        std::wstring link = cache + L"\\atalho";
        if (CreateSymbolicLinkW(link.c_str(), outside.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | 0x2 /* sem privilégio */)) {
            Check(Measure(T(cache, 0, L"importante.txt")).bytes == 0, "a análise não entra no link");
            Run(T(cache));
            Check(Exists(outside + L"\\importante.txt"), "o destino de um link nunca é apagado");
            Check(!clean::SafeRoot(link), "link não vale como raiz");
            Check(!clean::SafeItem(cache, link + L"\\importante.txt", false), "arquivo atrás de link é recusado");
        } else std::wcout << L"(link simbólico indisponível aqui: testes de link pulados)\n";

        // --- SafeItem ---
        File(outside + L"\\x.tmp", 1);
        Check(!clean::SafeItem(cache, outside + L"\\x.tmp", false), "fora da raiz é recusado");
        Check(!clean::SafeItem(cache, cache, true), "a raiz em si é recusada");
        File(cache + L"\\y.tmp", 1);
        Check(clean::SafeItem(cache, cache + L"\\y.tmp", false), "arquivo comum dentro da raiz é aceito");
        Check(!clean::SafeItem(cache, cache + L"\\y.tmp", true), "tipo trocado (arquivo x pasta) é recusado");

        // --- caminho com mais de 260 caracteres (caches do Service Worker) ---
        {
            std::wstring deep = cache;
            while (deep.size() < 300) { deep += L"\\pasta_bem_comprida_de_cache"; CreateDirectoryW(clean::Lp(deep).c_str(), nullptr); }
            std::wstring f = deep + L"\\fundo.bin";
            HANDLE hf = CreateFileW(clean::Lp(f).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
            if (hf != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(hf, "x", 1, &w, nullptr); CloseHandle(hf); }
            Check(GetFileAttributesW(clean::Lp(f).c_str()) != INVALID_FILE_ATTRIBUTES, "arquivo de teste no caminho longo");
            Check(Measure(T(cache, 0, L"fundo.bin")).files == 1, "a análise enxerga arquivo em caminho longo");
            Run(T(cache, 0, L"fundo.bin"));
            Check(GetFileAttributesW(clean::Lp(f).c_str()) == INVALID_FILE_ATTRIBUTES, "e a limpeza apaga");
        }

        // --- "só informa" nunca é limpo ---
        {
            File(cache + L"\\info.tmp", 300);
            clean::Target ti = T(cache, 0, L"info.tmp"); ti.risk = clean::kInform;
            clean::Leaf li; li.custom = &ti; li.on = true;
            std::atomic<bool> stop(false);
            clean::CleanLeaf(li, 1, true, stop);
            Check(Exists(cache + L"\\info.tmp") && !clean::Actionable(li), "alvo que só informa nunca apaga");
        }

        // --- detecção automática: app Electron com Cache + Code Cache ---
        {
            std::wstring roaming = appdata + L"\\Roaming";
            Dir(roaming); Dir(roaming + L"\\MeuApp"); Dir(roaming + L"\\MeuApp\\Cache"); Dir(roaming + L"\\MeuApp\\Code Cache"); Dir(roaming + L"\\MeuApp\\Outra");
            File(roaming + L"\\MeuApp\\Cache\\data_1", 4096);
            Dir(roaming + L"\\SoUmCache"); Dir(roaming + L"\\SoUmCache\\Cache"); // um marcador só: não é perfil
            SetEnvironmentVariableW(L"APPDATA", roaming.c_str());
            SetEnvironmentVariableW(L"LOCALAPPDATA", (appdata + L"\\NaoExiste").c_str());
            SetEnvironmentVariableW(L"USERPROFILE", (appdata + L"\\NaoExiste").c_str());
            auto found = clean::DetectApps();
            bool meu = false, so = false;
            for (auto* t : found) { if (!wcscmp(t->item, L"MeuApp")) { meu = true; Check(std::wstring(t->paths).find(L"Outra") == std::wstring::npos, "só as pastas de cache do perfil"); } if (!wcscmp(t->item, L"SoUmCache")) so = true; }
            Check(meu, "perfil com dois marcadores é detectado");
            Check(!so, "pasta com um marcador só não é perfil");
            Check(clean::DetectApps().size() == found.size() && clean::Detected().size() == found.size(), "detecção repetida reaproveita os mesmos alvos");
        }

        // --- tabela gerada do targets.json ---
        for (int i = 0; i < clean::kTargetCount; ++i) {
            const clean::Target& t = clean::kTargets[i];
            Check(t.id && *t.id && t.item && *t.item, "todo alvo tem id e programa");
            Check(t.risk != clean::kInform || !t.on, "o que só informa nunca vem marcado");
            for (auto& p : clean::Split(t.paths)) {
                Check(p.find(L"%SystemRoot%") == std::wstring::npos && p.find(L"%ProgramFiles") == std::wstring::npos, "nada que peça administrador");
                Check(p.size() > 2 && p[0] == L'%', "todo caminho começa numa pasta conhecida (%VAR%)");
            }
        }

        RmTree(clean::Lp(base));
    } catch (const std::exception& e) {
        std::cout << "FALHOU: " << e.what() << " (depois de " << checks << " verificações)\n";
        return 1;
    }
    std::cout << "ok: " << checks << " verificações da Limpeza\n";
    return 0;
}
