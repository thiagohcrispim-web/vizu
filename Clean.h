// Clean.h - Limpeza (antigo Crispy Clean): encontra e remove arquivos descartáveis.
// Sem interface: a janela fica em CleanUi.inc.h. Testado em tests/clean_tests.cpp.
//
// Regras de segurança, portadas do safety.js do Crispy Clean e conferidas de novo a cada arquivo, na hora de apagar:
//  - só raízes da tabela kTargets, dentro de %LOCALAPPDATA%, %APPDATA% ou %TEMP%; nunca a raiz em si;
//  - nada dentro de Documentos, Área de Trabalho, Downloads, Imagens, Vídeos, Músicas e OneDrive;
//  - arquivos de trabalho (.psd, .ai, .indd, .prproj, .aep, .pdf…) nunca saem, mesmo dentro de um alvo;
//  - links (junções, links simbólicos) nunca são seguidos, nem como raiz nem no meio do caminho;
//  - as pastas do próprio Vizu ficam de fora (o instalador e as atualizações passam pela TEMP);
//  - respeita a idade mínima de cada alvo (o que foi mexido há pouco pode estar em uso);
//  - o que estiver em uso fica: o Windows recusa e a Limpeza só conta como "ficou".
// Risco: Seguro sai de vez; Revisar vai para a Lixeira; Informar só mede.
#pragma once
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace clean {

enum Group { kGSystem, kGBrowsers, kGAdobe, kGMessengers, kGDev, kGGames, kGOther, kGroupCount }; // categorias do Crispy Clean
enum Risk { kSafe, kReview, kInform };
enum Kind { kFolder, kScratch /* "Photoshop Temp<n>" na raiz de cada disco */, kRecycleBin };

// Mesmos campos do targets.json do Crispy Clean. Listas separadas por '|'.
struct Target {
    int group;
    const wchar_t* item;     // programa ou local ("Google Chrome")
    const wchar_t* leaf;     // pasta dentro dele ("Cache"); nullptr = o item é a própria pasta
    const wchar_t* paths;    // variáveis de ambiente; "*" e "?" casam com uma pasta por nível
    int risk;
    int ageHours;            // idade mínima; -1 = a das Configurações (temporários)
    const wchar_t* procs;    // programas que usam a pasta (precisam estar fechados para limpar tudo)
    const wchar_t* pattern;  // nomes de arquivo ("*" = todos)
    const wchar_t* exclude;  // nomes a pular (arquivo ou pasta)
    bool recursive;
    int keepNewest;          // entre pastas irmãs de versão (app-1.0.9, app-1.0.10), poupa as N mais novas
    int kind;
    bool on;                 // marcado por padrão
    const wchar_t* note;     // consequência (ou como limpar, nos que só informam)
    const wchar_t* id;
};

#include "CleanTargets.h" // gerado de limpeza/targets.json
static const int kTargetCount = (int)(sizeof(kTargets) / sizeof(kTargets[0]));


// ---------- utilidades ----------

inline ULONGLONG Ft(const FILETIME& f) { return (ULONGLONG)f.dwHighDateTime << 32 | f.dwLowDateTime; }
inline ULONGLONG Now() { FILETIME f; GetSystemTimeAsFileTime(&f); return Ft(f); }
static const ULONGLONG kHour = 3600ull * 10000000, kDay = 24 * kHour;

inline std::wstring Expand(const std::wstring& s) {
    if (s.empty()) return s;
    wchar_t b[2048]; DWORD n = ExpandEnvironmentStringsW(s.c_str(), b, 2048);
    std::wstring r = n && n <= 2048 ? b : L"";
    if (r.find(L'%') != std::wstring::npos) return L""; // variável que não existe
    while (r.size() > 3 && (r.back() == L'\\' || r.back() == L'/')) r.pop_back();
    if (r.find(L'~') != std::wstring::npos && r.find_first_of(L"*?") == std::wstring::npos) { // nome curto 8.3 -> nome longo
        wchar_t lb[2048]; DWORD n2 = GetLongPathNameW(r.c_str(), lb, 2048);
        if (n2 && n2 < 2048) r = lb;
    }
    return r;
}
inline std::wstring LowerPath(std::wstring s) { for (auto& c : s) { if (c == L'/') c = L'\\'; c = (wchar_t)towlower(c); } return s; }
inline std::vector<std::wstring> Split(const wchar_t* s, bool semicolon = true) { // ';' só separa padrões de nome, nunca caminhos
    std::vector<std::wstring> out; std::wstring cur;
    for (const wchar_t* p = s ? s : L""; ; ++p) { if (!*p || *p == L'|' || (semicolon && *p == L';')) { if (!cur.empty()) out.push_back(cur); cur.clear(); if (!*p) break; } else cur += *p; }
    return out;
}
// Curinga de nome de arquivo (* e ?), sem diferenciar maiúsculas.
inline bool Match(const wchar_t* pat, const wchar_t* s) {
    if (!*pat) return !*s;
    if (*pat == L'*') { while (*pat == L'*') ++pat; if (!*pat) return true; for (; *s; ++s) if (Match(pat, s)) return true; return false; }
    if (!*s) return false;
    return (*pat == L'?' || towlower(*pat) == towlower(*s)) && Match(pat + 1, s + 1);
}
inline bool MatchAny(const std::vector<std::wstring>& pats, const wchar_t* name) { for (auto& p : pats) if (p == L"*" || Match(p.c_str(), name)) return true; return false; }

// "filho" está estritamente dentro de "pai"?
inline bool Inside(const std::wstring& parent, const std::wstring& child) {
    std::wstring p = LowerPath(parent), c = LowerPath(child);
    while (!p.empty() && p.back() == L'\\') p.pop_back();
    return c.size() > p.size() + 1 && c.compare(0, p.size(), p) == 0 && c[p.size()] == L'\\';
}
inline bool SameOrInside(const std::wstring& a, const std::wstring& b) { return LowerPath(a) == LowerPath(b) || Inside(a, b); }

// Caminhos com mais de 248 caracteres (caches do Service Worker passam fácil) só funcionam com o prefixo \\?\\.
inline std::wstring Lp(const std::wstring& p) {
    if (p.size() < 240 || p.compare(0, 4, L"\\\\?\\") == 0 || p.size() < 3 || p[1] != L':') return p;
    return L"\\\\?\\" + p;
}

// Caminho real (resolve links no meio do caminho). Vazio se não existir.
inline std::wstring RealPath(const std::wstring& p) {
    HANDLE h = CreateFileW(Lp(p).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    wchar_t b[4096]; DWORD n = GetFinalPathNameByHandleW(h, b, 4096, 0);
    CloseHandle(h);
    if (!n || n >= 4096) return L"";
    std::wstring r(b, n);
    if (r.compare(0, 8, L"\\\\?\\UNC\\") == 0) r = L"\\\\" + r.substr(8);
    else if (r.compare(0, 4, L"\\\\?\\") == 0) r = r.substr(4);
    return r;
}

// Arquivos de trabalho: nunca saem, mesmo dentro de um alvo.
inline bool BlockedExt(const std::wstring& path) {
    static const wchar_t* ext[] = {L".psd", L".psb", L".psdt", L".ai", L".ait", L".indd", L".indt", L".idml", L".prproj", L".aep", L".aet", L".pdf",
                                   L".cdr", L".af", L".afdesign", L".afphoto", L".afpub", L".sketch", L".fig", L".xd"};
    size_t dot = path.find_last_of(L".\\");
    if (dot == std::wstring::npos || path[dot] != L'.') return false;
    for (auto e : ext) if (!_wcsicmp(path.c_str() + dot, e)) return true;
    return false;
}

// ---------- contexto de proteção ----------

struct Context {
    std::vector<std::wstring> allowed;   // raízes onde um alvo pode estar (estritamente dentro)
    std::vector<std::wstring> personal;  // nada dentro delas
    std::vector<std::wstring> ancestors; // um alvo nunca pode ser igual a uma destas nem conter uma delas
    std::vector<std::wstring> system;    // Windows e Arquivos de Programas: proibidos (o Vizu não pede administrador)
    std::vector<std::wstring> own;       // pastas e arquivos do próprio Vizu
    std::wstring temp;                   // única raiz permitida que pode ser limpa por inteiro
};

inline Context MakeContext() {
    Context c;
    {
        for (auto v : {L"%USERPROFILE%", L"%ProgramData%", L"%LOCALAPPDATA%", L"%APPDATA%", L"%TEMP%"}) { std::wstring e = Expand(v); if (e.size() > 3) c.allowed.push_back(e); }
        for (auto v : {L"%USERPROFILE%", L"%APPDATA%", L"%LOCALAPPDATA%", L"%USERPROFILE%\\AppData\\LocalLow", L"%ProgramData%"}) { std::wstring e = Expand(v); if (e.size() > 3) c.ancestors.push_back(e); }
        for (auto v : {L"%SystemRoot%", L"%ProgramFiles%", L"%ProgramFiles(x86)%"}) { std::wstring e = Expand(v); if (e.size() > 3) c.system.push_back(e); }
        c.temp = Expand(L"%TEMP%");
        std::wstring home = Expand(L"%USERPROFILE%");
        if (home.size() > 3) {
            for (auto n : {L"Documents", L"Desktop", L"Downloads", L"Pictures", L"Videos", L"Music"}) c.personal.push_back(home + L"\\" + n);
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileExW(Lp(home + L"\\OneDrive*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, 0);
            if (h != INVALID_HANDLE_VALUE) { do c.personal.push_back(home + L"\\" + fd.cFileName); while (FindNextFileW(h, &fd)); FindClose(h); }
        }
        for (auto id : {&FOLDERID_Documents, &FOLDERID_Desktop, &FOLDERID_Downloads, &FOLDERID_Pictures, &FOLDERID_Videos, &FOLDERID_Music}) {
            PWSTR p = nullptr; // pastas redirecionadas (OneDrive, outro disco)
            if (SUCCEEDED(SHGetKnownFolderPath(*id, 0, nullptr, &p)) && p) c.personal.push_back(p);
            if (p) CoTaskMemFree(p);
        }
        std::wstring local = Expand(L"%LOCALAPPDATA%");
        if (local.size() > 3) { c.own.push_back(local + L"\\Programs\\DesignThumbs"); c.own.push_back(local + L"\\DesignThumbs"); }
        wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH); c.own.push_back(self);
    }
    return c;
}
inline Context& Ctx() { static Context c = MakeContext(); return c; } // estática local: inicialização segura entre threads

// Pode ser a raiz de um alvo? Pasta real, sem link no caminho, dentro de uma raiz permitida, fora das pessoais.
inline bool SafeRoot(const std::wstring& path) {
    const Context& c = Ctx();
    if (path.size() < 4 || path[1] != L':' || path[2] != L'\\') return false;
    if (path.find(L"\\..") != std::wstring::npos || path.find(L"..\\") != std::wstring::npos) return false;
    DWORD a = GetFileAttributesW(Lp(path).c_str());
    if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY) || (a & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    std::wstring real = RealPath(path);
    if (real.empty() || LowerPath(real) != LowerPath(path)) return false; // algum trecho do caminho é link
    bool inside = !c.temp.empty() && LowerPath(path) == LowerPath(c.temp);
    for (auto& r : c.allowed) if (Inside(r, path)) inside = true;
    if (!inside) return false;
    for (auto& p : c.personal) if (SameOrInside(p, path) || Inside(path, p)) return false;
    for (auto& a2 : c.ancestors) if (SameOrInside(path, a2)) return false;
    for (auto& sy : c.system) if (SameOrInside(path, sy) || Inside(sy, path)) return false;
    for (auto& o : c.own) if (SameOrInside(o, path) || Inside(path, o)) return false;
    return true;
}

// Confere um arquivo ou pasta imediatamente antes de apagar (validarItem do Crispy Clean).
inline bool SafeItem(const std::wstring& root, const std::wstring& path, bool isDir, std::vector<std::wstring>* parentsOk = nullptr) {
    const Context& c = Ctx();
    if (!Inside(root, path) || path.find(L"\\..") != std::wstring::npos) return false;
    if (!isDir && BlockedExt(path)) return false;
    for (auto& p : c.personal) if (SameOrInside(p, path)) return false;
    for (auto& o : c.own) if (SameOrInside(o, path)) return false;
    DWORD a = GetFileAttributesW(Lp(path).c_str());
    if (a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    if (((a & FILE_ATTRIBUTE_DIRECTORY) != 0) != isDir) return false;
    std::wstring parent = path.substr(0, path.find_last_of(L'\\'));
    if (parentsOk && !parentsOk->empty() && parentsOk->back() == parent) return true; // mesma pasta do arquivo anterior
    // Nenhuma pasta entre a raiz (já conferida por SafeRoot) e o arquivo pode ser link: um link trocado depois da
    // análise apontaria para fora do alvo. Olha pasta por pasta (funciona com caminhos longos, ao contrário do caminho real).
    for (size_t i = root.size(); i < parent.size();) {
        size_t j = parent.find(L'\\', i + 1);
        if (j == std::wstring::npos) j = parent.size();
        DWORD pa = GetFileAttributesW(Lp(parent.substr(0, j)).c_str());
        if (pa == INVALID_FILE_ATTRIBUTES || !(pa & FILE_ATTRIBUTE_DIRECTORY) || (pa & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
        i = j;
    }
    if (parentsOk) parentsOk->push_back(parent);
    return true;
}

// ---------- raízes ----------

// Expande curingas (uma pasta por nível, nunca seguindo links) e devolve as pastas que existem.
inline std::vector<std::wstring> ExpandWildcards(const std::wstring& path) {
    std::vector<std::wstring> cur;
    if (path.size() < 3 || path[1] != L':') return cur;
    cur.push_back(path.substr(0, 2));
    size_t i = 3;
    while (i <= path.size()) {
        size_t j = path.find(L'\\', i); if (j == std::wstring::npos) j = path.size();
        std::wstring seg = path.substr(i, j - i);
        i = j + 1;
        if (seg.empty()) continue;
        std::vector<std::wstring> next;
        if (seg.find_first_of(L"*?") == std::wstring::npos) { for (auto& b : cur) next.push_back(b + L"\\" + seg); }
        else for (auto& b : cur) {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileExW(Lp(b + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, 0);
            if (h == INVALID_HANDLE_VALUE) continue;
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
                if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..") || !Match(seg.c_str(), fd.cFileName)) continue;
                next.push_back(b + L"\\" + fd.cFileName);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        cur.swap(next);
        if (cur.empty()) break;
    }
    std::vector<std::wstring> out;
    for (auto& p : cur) { DWORD a = GetFileAttributesW(Lp(p).c_str()); if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(p); }
    return out;
}

// "app-1.0.10" > "app-1.0.9"
inline bool VersionLess(const std::wstring& a, const std::wstring& b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (iswdigit(a[i]) && iswdigit(b[j])) {
            unsigned long long x = 0, y = 0;
            while (i < a.size() && iswdigit(a[i])) x = x * 10 + (a[i++] - L'0');
            while (j < b.size() && iswdigit(b[j])) y = y * 10 + (b[j++] - L'0');
            if (x != y) return x < y;
        } else { wchar_t x = towlower(a[i++]), y = towlower(b[j++]); if (x != y) return x < y; }
    }
    return a.size() - i < b.size() - j;
}

inline std::wstring ParentOf(const std::wstring& p) { size_t k = p.find_last_of(L'\\'); return k == std::wstring::npos ? L"" : p.substr(0, k); }

inline std::vector<std::wstring> Roots(const Target& t) {
    std::vector<std::wstring> out;
    for (auto& raw : Split(t.paths, false)) {
        std::wstring exp = Expand(raw);
        if (exp.empty()) continue;
        std::vector<std::wstring> cand = ExpandWildcards(exp);
        if (t.keepNewest > 0) { // poupa as N versões mais novas entre pastas irmãs
            std::sort(cand.begin(), cand.end(), [](const std::wstring& a, const std::wstring& b) {
                std::wstring pa = LowerPath(ParentOf(a)), pb = LowerPath(ParentOf(b));
                return pa != pb ? pa < pb : VersionLess(a.substr(a.find_last_of(L'\\') + 1), b.substr(b.find_last_of(L'\\') + 1));
            });
            std::vector<std::wstring> keep;
            for (size_t i = 0; i < cand.size();) {
                size_t k = i; std::wstring pi = LowerPath(ParentOf(cand[i]));
                while (k < cand.size() && LowerPath(ParentOf(cand[k])) == pi) ++k;
                for (size_t m = i; m + t.keepNewest < k; ++m) keep.push_back(cand[m]);
                i = k;
            }
            cand.swap(keep);
        }
        for (auto& c : cand) {
            if (!SafeRoot(c)) continue;
            bool dup = false; for (auto& o : out) if (LowerPath(o) == LowerPath(c)) dup = true;
            if (!dup) out.push_back(c);
        }
    }
    return out;
}

// Raízes dos discos fixos ("C:\") para os arquivos de rascunho do Photoshop.
inline std::vector<std::wstring> DiskRoots() {
    std::vector<std::wstring> out; DWORD mask = GetLogicalDrives();
    for (int i = 2; i < 26; ++i) { wchar_t r[] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if ((mask & (1u << i)) && GetDriveTypeW(r) == DRIVE_FIXED) out.push_back(r); }
    return out;
}

// ---------- percorrer ----------

struct Counters { ULONGLONG bytes = 0, inUse = 0, protectedBytes = 0; size_t files = 0, recent = 0, links = 0, noAccess = 0, protectedFiles = 0; };

inline ULONGLONG Cutoff(const Target& t, int ageDays, ULONGLONG now) {
    ULONGLONG h = t.ageHours < 0 ? (ULONGLONG)ageDays * 24 : (ULONGLONG)t.ageHours;
    return h ? now - h * kHour : 0;
}

// Percorre o conteúdo de uma raiz sem seguir links. onFile(caminho, tamanho) para cada arquivo que pode sair.
// dirs recebe as subpastas com a data de criação lida antes de apagar (para tirar as vazias depois).
inline void Walk(const std::wstring& root, const Target& t, ULONGLONG cutoff, Counters& c, const std::function<void(const std::wstring&, ULONGLONG)>& onFile,
                 std::vector<std::pair<std::wstring, ULONGLONG>>* dirs, const std::atomic<bool>* stop) {
    std::vector<std::wstring> pats = Split(t.pattern), excl = Split(t.exclude);
    if (pats.empty()) pats.push_back(L"*");
    const Context& ctx = Ctx();
    std::vector<std::wstring> stack = {root};
    while (!stack.empty()) {
        if (stop && *stop) return;
        std::wstring dir = stack.back(); stack.pop_back();
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW(Lp(dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) { if (GetLastError() == ERROR_ACCESS_DENIED) ++c.noAccess; continue; }
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            if (!excl.empty() && MatchAny(excl, fd.cFileName)) continue;
            std::wstring full = dir + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { ++c.links; continue; }
            bool own = false; for (auto& o : ctx.own) if (SameOrInside(o, full)) own = true;
            if (own) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (t.recursive && stack.size() < 100000) { stack.push_back(full); if (dirs) dirs->push_back({full, Ft(fd.ftCreationTime)}); }
                continue;
            }
            if (!MatchAny(pats, fd.cFileName)) continue;
            ULONGLONG size = (ULONGLONG)fd.nFileSizeHigh << 32 | fd.nFileSizeLow;
            if (cutoff && Ft(fd.ftLastWriteTime) > cutoff) { ++c.recent; continue; }
            if (BlockedExt(full)) { ++c.protectedFiles; c.protectedBytes += size; continue; }
            onFile(full, size);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
}

// Arquivos de rascunho do Photoshop: só "Photoshop Temp<números>", direto na raiz do disco, sem subpastas.
inline void Scratch(const std::wstring& disk, const std::function<void(const std::wstring&, ULONGLONG)>& onFile) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(Lp(disk + L"Photoshop Temp*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        if (_wcsnicmp(fd.cFileName, L"Photoshop Temp", 14)) continue;
        const wchar_t* n = fd.cFileName + 14; bool digits = *n != 0;
        for (; *n; ++n) if (!iswdigit(*n)) digits = false;
        if (!digits) continue;
        onFile(disk + fd.cFileName, (ULONGLONG)fd.nFileSizeHigh << 32 | fd.nFileSizeLow);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

inline bool FileInUse(const std::wstring& p) {
    HANDLE h = CreateFileW(Lp(p).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); return false; }
    DWORD e = GetLastError();
    return e == ERROR_SHARING_VIOLATION || e == ERROR_LOCK_VIOLATION || e == ERROR_ACCESS_DENIED;
}

// ---------- programas abertos ----------

inline std::vector<std::wstring> RunningExes() {
    std::vector<std::wstring> out;
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe = {sizeof(pe)};
    for (BOOL ok = Process32FirstW(s, &pe); ok; ok = Process32NextW(s, &pe)) out.push_back(LowerPath(pe.szExeFile));
    CloseHandle(s);
    return out;
}
inline bool ProcessRunning(const std::wstring& exe) {
    if (exe.empty()) return false;
    std::wstring l = LowerPath(exe);
    for (auto& e : RunningExes()) if (e == l) return true;
    return false;
}
inline bool AnyRunning(const std::vector<std::wstring>& exes) { for (auto& e : exes) if (ProcessRunning(e)) return true; return false; }

// Pede para o programa fechar como um clique no X: quem tem trabalho não salvo pergunta antes. Nunca força.
inline bool CloseProgram(const std::wstring& exe, DWORD timeoutMs = 10000) {
    std::vector<DWORD> pids;
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = {sizeof(pe)};
        for (BOOL ok = Process32FirstW(s, &pe); ok; ok = Process32NextW(s, &pe)) if (!_wcsicmp(pe.szExeFile, exe.c_str())) pids.push_back(pe.th32ProcessID);
        CloseHandle(s);
    }
    EnumWindows([](HWND w, LPARAM lp) -> BOOL {
        DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
        for (DWORD p : *(std::vector<DWORD>*)lp) if (p == pid && IsWindowVisible(w) && !GetWindow(w, GW_OWNER)) PostMessageW(w, WM_CLOSE, 0, 0);
        return TRUE;
    }, (LPARAM)&pids);
    for (DWORD waited = 0; waited < timeoutMs; waited += 250) { if (!ProcessRunning(exe)) return true; Sleep(250); }
    return !ProcessRunning(exe);
}

// ---------- análise ----------

struct Leaf {
    int target = -1;                 // índice em kTargets
    const Target* custom = nullptr;  // alvo fora da tabela (testes)
    std::wstring name;
    std::vector<std::wstring> roots;
    ULONGLONG bytes = 0, inUse = 0, protectedBytes = 0;
    size_t files = 0, protectedFiles = 0;
    bool on = false;
};
struct Item { std::wstring name; std::vector<std::wstring> procs; std::vector<Leaf> leaves; ULONGLONG bytes = 0; int risk = kSafe; };
struct GroupRes { int id = 0; std::vector<Item> items; ULONGLONG bytes = 0; bool done = false; };
struct Result { GroupRes groups[kGroupCount]; ULONGLONG total = 0; };

inline const Target& TargetOf(const Leaf& l) { return l.custom ? *l.custom : kTargets[l.target]; }
inline bool Actionable(const Leaf& l) { return TargetOf(l).risk != kInform; }

inline ULONGLONG Selected(const Result& r) {
    ULONGLONG b = 0;
    for (auto& g : r.groups) for (auto& it : g.items) for (auto& l : it.leaves) if (l.on) b += l.bytes;
    return b;
}

// Mede uma folha. Com o programa aberto, arquivos grandes em uso ficam fora do total (como no Crispy Clean).
inline void MeasureLeaf(Leaf& l, int ageDays, const std::atomic<bool>* stop) {
    const Target& t = TargetOf(l);
    Counters c; ULONGLONG cutoff = Cutoff(t, ageDays, Now());
    bool test = t.kind == kScratch || AnyRunning(Split(t.procs));
    auto onFile = [&](const std::wstring& p, ULONGLONG size) {
        if (test && size >= (1u << 20) && FileInUse(p)) { c.inUse += size; return; }
        c.bytes += size; ++c.files;
    };
    l.roots.clear();
    if (t.kind == kScratch) { for (auto& d : DiskRoots()) { size_t before = c.files; Scratch(d, onFile); if (c.files > before) l.roots.push_back(d); } }
    else for (auto& r : Roots(t)) { l.roots.push_back(r); Walk(r, t, cutoff, c, onFile, nullptr, stop); }
    l.bytes = c.bytes; l.files = c.files; l.inUse = c.inUse; l.protectedBytes = c.protectedBytes; l.protectedFiles = c.protectedFiles;
}

// Analisa todos os grupos. progress(grupo) ao terminar cada um (chamado da thread da análise).
inline std::vector<const Target*> DetectApps();
inline void Scan(Result& r, int ageDays, bool includeRecycle, const std::atomic<bool>& stop, const std::function<void(int)>& progress) {
    for (int g = 0; g < kGroupCount; ++g) { r.groups[g] = GroupRes(); r.groups[g].id = g; }
    r.total = 0;
    std::vector<const Target*> all;
    for (int i = 0; i < kTargetCount; ++i) all.push_back(&kTargets[i]);
    for (auto* t : DetectApps()) all.push_back(t);
    for (int g = 0; g < kGroupCount && !stop; ++g) {
        GroupRes& gr = r.groups[g];
        for (size_t i = 0; i < all.size() && !stop; ++i) {
            const Target& t = *all[i];
            if (t.group != g) continue;
            Leaf l; l.custom = &t; l.target = i < (size_t)kTargetCount ? (int)i : -1; l.name = t.leaf ? t.leaf : t.item;
            if (t.kind == kRecycleBin) {
                if (!includeRecycle) continue;
                SHQUERYRBINFO q = {sizeof(q)};
                if (FAILED(SHQueryRecycleBinW(nullptr, &q)) || q.i64Size <= 0) continue;
                l.bytes = (ULONGLONG)q.i64Size; l.files = (size_t)q.i64NumItems;
            } else MeasureLeaf(l, ageDays, &stop);
            if (!l.bytes) continue;
            l.on = t.on && t.risk != kInform;
            Item* it = nullptr;
            for (auto& x : gr.items) if (x.name == t.item) it = &x;
            if (!it) { gr.items.push_back(Item()); it = &gr.items.back(); it->name = t.item; it->procs = Split(t.procs); it->risk = t.risk; }
            if (t.risk > it->risk) it->risk = t.risk;
            it->bytes += l.bytes; it->leaves.push_back(l);
        }
        std::sort(gr.items.begin(), gr.items.end(), [](const Item& a, const Item& b) { return a.bytes > b.bytes; });
        for (auto& it : gr.items) gr.bytes += it.bytes;
        r.total += gr.bytes;
        gr.done = true;
        if (progress) progress(g);
    }
}

// ---------- detecção automática (detect.js do Crispy Clean) ----------
// Um "perfil" é uma pasta com pelo menos duas pastas-marcador (Cache, Code Cache, GPUCache). Só entram as pastas da
// lista kDetectFolders (e kDetectParentFolders, ao lado do perfil); SafeRoot continua valendo para cada uma.
struct DetectedTarget { Target t; std::wstring item, paths; };
inline std::deque<DetectedTarget>& Detected() { static std::deque<DetectedTarget> d; return d; } // endereços estáveis entre análises

inline std::vector<std::wstring> SubDirs(const std::wstring& dir) {
    std::vector<std::wstring> out; WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(Lp(dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return out;
    do if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L".."))
        out.push_back(fd.cFileName);
    while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}
inline bool RealDir(const std::wstring& p) { DWORD a = GetFileAttributesW(Lp(p).c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT); }

inline const wchar_t* DetectLookup(const wchar_t* const (*map)[2], size_t n, const std::wstring& key) {
    for (size_t i = 0; i < n; ++i) if (!_wcsicmp(map[i][0], key.c_str())) return map[i][1];
    return nullptr;
}

inline std::vector<const Target*> DetectApps() {
    std::vector<const Target*> out;
    if (!kDetectOn) return out;
    std::vector<std::wstring> covered; // raízes que a tabela já cobre
    for (int i = 0; i < kTargetCount; ++i) for (auto& r : Roots(kTargets[i])) covered.push_back(r);
    auto isCovered = [&](const std::wstring& r) { for (auto& c : covered) if (SameOrInside(c, r) || SameOrInside(r, c)) return true; return false; };
    struct App { std::wstring name; std::vector<std::wstring> roots; bool browser = false; };
    std::vector<App> apps;
    static const wchar_t* skip[] = {L"temp", L"node_modules", L".git", L"packages", L"programs", L"microsoft"};
    for (auto base0 : kDetectBases) {
        std::wstring exp = Expand(base0);
        if (exp.empty()) continue;
        for (auto& base : ExpandWildcards(exp)) {
            std::vector<std::pair<std::wstring, int>> stack = {{base, 0}};
            while (!stack.empty()) {
                auto cur = stack.back(); stack.pop_back();
                std::vector<std::wstring> kids = SubDirs(cur.first);
                int marks = 0;
                for (auto m : kDetectMarkers) for (auto& k : kids) if (!_wcsicmp(k.c_str(), m)) { ++marks; break; }
                bool profile = cur.first != base && marks >= 2;
                if (profile) {
                    std::vector<std::wstring> roots;
                    for (auto f : kDetectFolders) { std::wstring p = cur.first + L"\\" + f; if (RealDir(p) && !isCovered(p)) roots.push_back(p); }
                    for (auto f : kDetectParentFolders) { std::wstring p = ParentOf(cur.first) + L"\\" + f; if (RealDir(p) && !isCovered(p)) roots.push_back(p); }
                    if (!roots.empty()) {
                        std::wstring name, lp = cur.first + L"\\";
                        size_t pk = LowerPath(lp).find(L"\\packages\\");
                        if (pk != std::wstring::npos) { // "5319275A.WhatsAppDesktop_cv1g1gvanyjgm" -> "WhatsAppDesktop"
                            std::wstring pkg = lp.substr(pk + 10); pkg = pkg.substr(0, pkg.find(L'\\'));
                            size_t us = pkg.find_last_of(L'_'); if (us != std::wstring::npos) pkg.resize(us);
                            size_t dot = pkg.find_last_of(L'.'); name = dot == std::wstring::npos ? pkg : pkg.substr(dot + 1);
                        } else {
                            std::wstring rel = cur.first.substr(base.size() + 1);
                            name = rel.substr(0, rel.find(L'\\'));
                        }
                        App* a = nullptr;
                        for (auto& x : apps) if (!_wcsicmp(x.name.c_str(), name.c_str())) a = &x;
                        if (!a) { apps.push_back(App()); a = &apps.back(); a->name = name; }
                        for (auto& r : roots) { bool dup = false; for (auto& q : a->roots) if (LowerPath(q) == LowerPath(r)) dup = true; if (!dup) a->roots.push_back(r); }
                        if (LowerPath(lp).find(L"\\user data\\") != std::wstring::npos) a->browser = true;
                    }
                }
                if (cur.second >= kDetectDepth) continue;
                for (auto& k : kids) {
                    bool sk = false;
                    for (auto x : skip) if (!_wcsicmp(k.c_str(), x)) sk = true;
                    if (profile) for (auto f : kDetectFolders) { std::wstring first = f; first = first.substr(0, first.find(L'\\')); if (!_wcsicmp(k.c_str(), first.c_str())) sk = true; }
                    if (!sk) stack.push_back({cur.first + L"\\" + k, cur.second + 1});
                }
            }
        }
    }
    std::sort(apps.begin(), apps.end(), [](const App& a, const App& b) { return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });
    for (auto& a : apps) {
        std::wstring paths; for (auto& r : a.roots) paths += (paths.empty() ? L"" : L"|") + r;
        DetectedTarget* d = nullptr;
        for (auto& x : Detected()) if (x.paths == paths) d = &x; // a mesma de uma análise anterior: reaproveita
        if (!d) {
            Detected().push_back(DetectedTarget()); d = &Detected().back();
            const wchar_t* shown = DetectLookup(kDetectNames, sizeof(kDetectNames) / sizeof(kDetectNames[0]), a.name);
            const wchar_t* cat = DetectLookup(kDetectGroups, sizeof(kDetectGroups) / sizeof(kDetectGroups[0]), a.name);
            std::wstring cs = cat ? cat : a.browser ? L"Navegadores" : L"Outros";
            int g = cs == L"Navegadores" ? kGBrowsers : cs == L"Jogos" ? kGGames : cs == L"Mensageiros" ? kGMessengers : cs == L"Desenvolvimento" ? kGDev
                  : cs == L"Adobe" ? kGAdobe : cs == L"Sistema" ? kGSystem : kGOther;
            d->item = shown ? shown : a.name; d->paths = paths;
            for (int i = 0; i < kTargetCount; ++i) if (!_wcsicmp(kTargets[i].item, d->item.c_str())) { g = kTargets[i].group; break; } // "Discord" detectado fica junto do Discord da tabela
            d->t = {g, nullptr, nullptr, nullptr, kSafe, 0, L"", L"*", L"", true, 0, kFolder, true,
                    L"Cache encontrado automaticamente. O app recria esses arquivos sozinho; a primeira abertura depois da limpeza pode ser um pouco mais lenta.", L"auto"};
            d->t.item = d->item.c_str(); d->t.paths = d->paths.c_str();
        }
        out.push_back(&d->t);
    }
    return out;
}

// ---------- limpeza ----------

struct Outcome { ULONGLONG removed = 0, kept = 0; size_t filesRemoved = 0, filesKept = 0; bool recycled = false; };

// Limpa uma folha marcada. Refaz a varredura com as mesmas regras (o que mudou desde a análise fica)
// e confere cada arquivo de novo. Seguro: apaga de vez. Revisar: Lixeira (ou de vez, se "Excluir direto").
inline Outcome CleanLeaf(const Leaf& l, int ageDays, bool permanent, const std::atomic<bool>& stop, const std::function<void(ULONGLONG)>& tick = nullptr) {
    Outcome o;
    const Target& t = TargetOf(l);
    if (t.risk == kInform) return o;
    if (t.kind == kRecycleBin) {
        SHQUERYRBINFO q = {sizeof(q)}; SHQueryRecycleBinW(nullptr, &q); ULONGLONG before = (ULONGLONG)q.i64Size;
        SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        q = {sizeof(q)}; SHQueryRecycleBinW(nullptr, &q);
        o.removed = before > (ULONGLONG)q.i64Size ? before - (ULONGLONG)q.i64Size : 0; o.kept = before - o.removed;
        if (tick) tick(o.removed);
        return o;
    }
    bool recycle = t.risk == kReview && !permanent;
    o.recycled = recycle;
    ULONGLONG cutoff = Cutoff(t, ageDays, Now());
    std::vector<std::pair<std::wstring, ULONGLONG>> toRecycle;
    auto run = [&](const std::wstring& root, bool scratch) {
        Counters c; std::vector<std::pair<std::wstring, ULONGLONG>> dirs; std::vector<std::wstring> parentsOk;
        auto onFile = [&](const std::wstring& p, ULONGLONG size) {
            if (stop) return;
            bool ok = scratch ? (p.size() > 3 && p.find(L'\\', 3) == std::wstring::npos && !FileInUse(p)) : SafeItem(root, p, false, &parentsOk);
            if (!ok) { o.kept += size; ++o.filesKept; return; }
            if (recycle) { toRecycle.push_back({p, size}); return; }
            if (DeleteFileW(Lp(p).c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) { o.removed += size; ++o.filesRemoved; if (tick) tick(size); }
            else { o.kept += size; ++o.filesKept; }
        };
        if (scratch) Scratch(root, onFile); else Walk(root, t, cutoff, c, onFile, &dirs, &stop);
        if (!recycle && !scratch && !stop) { // subpastas que ficaram vazias (nunca a raiz); RemoveDirectory falha se houver conteúdo
            std::sort(dirs.begin(), dirs.end(), [](const std::pair<std::wstring, ULONGLONG>& a, const std::pair<std::wstring, ULONGLONG>& b) { return a.first.size() > b.first.size(); });
            for (auto& d : dirs) {
                if (cutoff && d.second > cutoff) continue; // criada há pouco: pode ser de um programa rodando
                if (SafeItem(root, d.first, true)) RemoveDirectoryW(Lp(d.first).c_str());
            }
        }
    };
    if (t.kind == kScratch) { for (auto& d : DiskRoots()) run(d, true); }
    else for (auto& r : Roots(t)) { if (stop) break; run(r, false); }
    if (!toRecycle.empty() && !stop) {
        IFileOperation* op = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&op)))) {
            op->SetOperationFlags(FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOFX_RECYCLEONDELETE | FOFX_ADDUNDORECORD);
            for (auto& f : toRecycle) {
                IShellItem* si = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(f.first.c_str(), nullptr, IID_PPV_ARGS(&si)))) { op->DeleteItem(si, nullptr); si->Release(); }
            }
            op->PerformOperations(); // o que estiver em uso é pulado
            op->Release();
        }
    }
    for (auto& f : toRecycle) {
        if (GetFileAttributesW(Lp(f.first).c_str()) == INVALID_FILE_ATTRIBUTES) { o.removed += f.second; ++o.filesRemoved; if (tick) tick(f.second); }
        else { o.kept += f.second; ++o.filesKept; }
    }
    return o;
}

// ---------- disco ----------

struct Disk { wchar_t letter = L'C'; ULONGLONG total = 0, free = 0; };
inline std::vector<Disk> FixedDisks() {
    std::vector<Disk> out;
    for (auto& r : DiskRoots()) {
        ULARGE_INTEGER fr, tot;
        if (GetDiskFreeSpaceExW(r.c_str(), &fr, &tot, nullptr)) out.push_back({r[0], tot.QuadPart, fr.QuadPart});
    }
    return out;
}
// Para o widget: o disco do Windows primeiro, depois os outros internos e os externos (USB) com mídia.
inline std::vector<Disk> AllDisks() {
    std::vector<Disk> out;
    wchar_t w[MAX_PATH]; GetWindowsDirectoryW(w, MAX_PATH);
    UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX); // leitor de cartão vazio não abre aviso
    DWORD mask = GetLogicalDrives();
    for (int i = 2; i < 26; ++i) {
        wchar_t r[] = {(wchar_t)(L'A' + i), L':', L'\\', 0};
        if (!(mask & (1u << i))) continue;
        UINT type = GetDriveTypeW(r);
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE) continue; // rede e CD ficam de fora
        ULARGE_INTEGER fr, tot;
        if (!GetDiskFreeSpaceExW(r, &fr, &tot, nullptr) || !tot.QuadPart) continue;
        Disk d = {r[0], tot.QuadPart, fr.QuadPart};
        if (towupper(w[0]) == d.letter) out.insert(out.begin(), d); else out.push_back(d);
    }
    SetErrorMode(oldMode);
    return out;
}
inline Disk SystemDisk() {
    wchar_t w[MAX_PATH]; GetWindowsDirectoryW(w, MAX_PATH);
    for (auto& d : FixedDisks()) if (towupper(w[0]) == d.letter) return d;
    return Disk();
}

} // namespace clean
