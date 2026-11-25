#!/usr/bin/env python3
"""Pré-teste do Vizu: roda antes de compilar e barra erros que já aconteceram.

Uso:  python3 .github/scripts/precheck.py
Sai com código 1 se achar algum ERRO. AVISOS só são mostrados.

Partes:
  1. Regras de bugs conhecidos  (.github/precheck/regras.txt)  <- adicione uma a cada bug novo
  2. Arquivos fora do build      (.github/precheck/fora-do-build.txt)
  3. Versão igual em todos os lugares
  4. Workflows sem ferramentas inventadas (vcpkg, CMake, commits fictícios)
  5. Sujeira no repositório (tmp.txt, arquivos gigantes fora de dist/)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CFG = ROOT / '.github' / 'precheck'
errors, warnings = [], []


def err(msg): errors.append(msg)
def warn(msg): warnings.append(msg)
def rel(p): return p.relative_to(ROOT).as_posix()
def read(p): return p.read_text(encoding='utf-8', errors='replace')


def sources():
    skip = ('third_party/', 'build/', '.git/', 'dist/')
    for p in ROOT.rglob('*'):
        if p.suffix in ('.h', '.cpp', '.yml', '.sh', '.rc', '.json') and p.is_file() and not rel(p).startswith(skip):
            yield p


# ---------- 1. regras de bugs conhecidos ----------
def check_rules():
    # Formato de cada regra (uma por bloco, separadas por linha em branco):
    #   id: nome-curto
    #   arquivos: *.h *.cpp          (glob, relativo à raiz)
    #   proibido: regex              (não pode aparecer)   -- ou --
    #   exigido: regex               (precisa aparecer em pelo menos um dos arquivos)
    #   nivel: erro | aviso          (padrão: erro)
    #   motivo: explicação para quem for corrigir
    text = read(CFG / 'regras.txt')
    blocks = [b for b in re.split(r'\n\s*\n', text) if b.strip() and not b.strip().startswith('#')]
    for b in blocks:
        r = {}
        for line in b.splitlines():
            if line.strip().startswith('#') or ':' not in line: continue
            k, v = line.split(':', 1); r[k.strip()] = v.strip()
        files = [p for g in r.get('arquivos', '*.h *.cpp').split() for p in ROOT.glob(g) if p.is_file()]
        level = err if r.get('nivel', 'erro') == 'erro' else warn
        tag = f"[{r.get('id', '?')}]"
        if 'proibido' in r:
            rx = re.compile(r['proibido'], re.M)
            for p in files:
                for m in rx.finditer(read(p)):
                    line = read(p).count('\n', 0, m.start()) + 1
                    level(f"{tag} {rel(p)}:{line}: {r.get('motivo', '')}")
        if 'exigido' in r:
            rx = re.compile(r['exigido'], re.M | re.S)
            if not any(rx.search(read(p)) for p in files):
                level(f"{tag} não encontrado em {r.get('arquivos')}: {r.get('motivo', '')}")


# ---------- 2. arquivos fora do build ----------
INC = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def reachable():
    """Arquivos que de fato entram no EXE, na DLL ou nos testes, seguindo os #include."""
    roots = [ROOT / 'Vizu.cpp', ROOT / 'DesignThumbs.cpp'] + sorted((ROOT / 'tests').glob('*.cpp'))
    build = read(ROOT / 'build.sh') + read(ROOT / '.github/workflows/build.yml')
    for p in sorted(ROOT.rglob('*.cpp')):  # .cpp citados na linha de compilação (não só em variáveis)
        r = rel(p)
        if not r.startswith(('third_party/', 'build/')) and re.search(r'(-o \S+|-c)[^\n]*\b' + re.escape(r) + r'\b', build):
            roots.append(p)
    seen, stack = set(), list(roots)
    while stack:
        p = stack.pop()
        if p in seen or not p.exists(): continue
        seen.add(p)
        for inc in INC.findall(read(p)):
            for base in (p.parent, ROOT):
                q = (base / inc).resolve()
                if q.exists(): stack.append(q); break
    return {rel(p) for p in seen}


def check_unbuilt():
    live = reachable()
    listed = {l.split('#')[0].strip() for l in read(CFG / 'fora-do-build.txt').splitlines()} - {''}
    code = {rel(p) for p in ROOT.rglob('*') if p.suffix in ('.h', '.cpp') and p.is_file()
            and not rel(p).startswith(('third_party/', 'build/', 'cdr/', 'tests/fixtures/'))}
    for f in sorted(code - live - listed):
        err(f"[fora-do-build] {f} não entra em nenhuma compilação. Editar este arquivo NÃO muda o Vizu. "
            "Inclua-o no build de verdade ou, se for rascunho, liste em .github/precheck/fora-do-build.txt")
    for f in sorted(listed & live):
        warn(f"[fora-do-build] {f} está listado como fora do build mas agora é compilado: tire da lista")


# ---------- 3. versão ----------
def check_version():
    rc = read(ROOT / 'Vizu.rc')
    m = re.search(r'VALUE\s+"FileVersion",\s*"([0-9.]+)"', rc)
    if not m: return err('[versao] FileVersion não encontrado no Vizu.rc')
    v = m.group(1); a, b, c = v.split('.')
    checks = {
        'Vizu.rc FILEVERSION': (rc, f'FILEVERSION {a},{b},{c},0'),
        'Vizu.rc ProductVersion': (rc, f'"ProductVersion", "{v}"'),
        'DesignThumbs.rc': (read(ROOT / 'DesignThumbs.rc'), f'"FileVersion", "{v}"'),
        'README (histórico)': (read(ROOT / 'README.md'), f'**v{v}**'),
        'README (link de download)': (read(ROOT / 'README.md'), f'Vizu_Setup_{v}_x64.exe'),
    }
    for name, (text, needle) in checks.items():
        if needle not in text: err(f"[versao] {name} não está na versão {v} (procurei: {needle})")
    lj = read(ROOT / 'latest.json')
    if f'"{v}"' not in lj:
        warn(f"[versao] latest.json ainda não aponta para {v} (normal antes de gerar o instalador)")
    for f in ('InstallerUi.h',):
        for old in re.findall(r'L"(\d+\.\d+\.\d+)"', read(ROOT / f)):
            if old != v: err(f"[versao] {f} mostra a versão fixa {old}, mas a versão é {v}")


# ---------- 4. workflows ----------
def check_workflows():
    wf = ROOT / '.github' / 'workflows'
    for p in wf.glob('*.yml'):
        t = read(p)
        if re.search(r'vcpkg|run-vcpkg|builtin-baseline', t):
            err(f"[workflow] {rel(p)} usa vcpkg. O Vizu compila com MinGW pelo build.sh, sem vcpkg")
        if re.search(r'^\s*(run:\s*)?cmake\b', t, re.M):
            err(f"[workflow] {rel(p)} chama cmake. O build oficial é o build.sh")
        if 'tmp-export' in p.name:
            err(f"[workflow] {rel(p)} é temporário e não pode ir para a main")
    rel_yml = read(wf / 'release.yml')
    if 'workflow_run' not in rel_yml or 'release.py publish' not in rel_yml:
        err('[workflow] release.yml precisa publicar sozinho (workflow_run + release.py publish)')
    for p in [wf / 'build.yml'] + list(wf.glob('*.yml')):
        for f in re.findall(r'(?:^|\s)((?:tests|engines)/[\w./-]+\.(?:cpp|h|rc))', read(p)):
            if not (ROOT / f).exists(): err(f"[workflow] {rel(p)} cita {f}, que não existe")
    vj = ROOT / 'vcpkg.json'
    if vj.exists():
        m = re.search(r'"builtin-baseline":\s*"([0-9a-f]+)"', read(vj))
        if m and re.search(r'([0-9a-f])\1{0}(?:d8d9|a1b2|1234|abcd)', m.group(1)):
            warn('[workflow] vcpkg.json tem um builtin-baseline com cara de inventado')


# ---------- 5. sujeira ----------
def check_junk():
    for name in ('tmp.txt', 'temp.txt', 'out.txt', 'nul'):
        if (ROOT / name).exists(): warn(f"[sujeira] {name} na raiz do repositório")
    for p in ROOT.rglob('*'):
        r = rel(p)
        if p.is_file() and not r.startswith(('.git/', 'dist/', 'build/', 'third_party/', 'tests/fixtures/', 'engines/bin')) \
                and p.stat().st_size > 5_000_000:
            warn(f"[sujeira] {r} tem {p.stat().st_size // 1_000_000} MB fora de dist/")
    for p in ROOT.glob('*.h'):
        t = read(p)
        if 'Ã' in t and ('Ã§' in t or 'Ã£' in t or 'Ã©' in t):
            err(f"[acentos] {rel(p)} tem acentos corrompidos (UTF-8 lido como Latin-1)")


# ---------- 6. membros usados que não existem ----------
def members(struct_src):
    body = re.sub(r'\{[^{}]*\}', '{}', struct_src[1:-1])  # tira inicializadores {..} e corpos de funções
    body = re.sub(r'\([^()]*\)', '()', body)
    names = set(re.findall(r'\b(\w+)\s*(?:\{\}|=[^,;]*)?\s*(?=[,;])', body))
    names |= set(re.findall(r'\b(\w+)\s*\(', body))
    return names


def struct_body(text, header):
    i = text.find(header)
    if i < 0: return None
    j = text.index('{', i); d = 0
    for k in range(j, len(text)):
        d += text[k] == '{'; d -= text[k] == '}'
        if d == 0: return text[j:k + 1]


def check_members():
    """g_cfg.x e tk.x precisam existir na struct que o build usa de verdade (a IA costuma supor que existem)."""
    live = reachable()
    code = {f: read(ROOT / f) for f in live if f.endswith(('.h', '.cpp')) and not f.startswith('tests/')
            and f != 'Rain.inc.h'}  # Rain.inc.h tem a própria rain::g_cfg
    cfg = struct_body(read(ROOT / 'Vizu.cpp'), 'struct Config {')
    tok = struct_body(read(ROOT / 'Ui.h'), 'struct Tokens {')
    for name, body, rx in (('Config (Vizu.cpp)', cfg, r'(?<![:\w])g_cfg\.(\w+)'), ('ui::Tokens (Ui.h)', tok, r'\b(?:tk|p\.tk|ui::g_tokens)\.(\w+)')):
        if body is None: err(f"[membros] struct {name} não encontrada"); continue
        have = members(body); seen = {}
        for f, t in code.items():
            for m in re.finditer(rx, t):
                if m.group(1) not in have: seen.setdefault(m.group(1), f"{f}:{t.count(chr(10), 0, m.start()) + 1}")
        for mem, where in sorted(seen.items()):
            err(f"[membros] {where}: {mem} não existe em {name}. Declare o campo (e leia/grave no registro) antes de usar")


def main():
    for f in (check_rules, check_unbuilt, check_version, check_workflows, check_junk, check_members):
        try: f()
        except Exception as e: err(f"[precheck] {f.__name__} falhou: {e}")
    for w in warnings: print('AVISO ', w)
    for e in errors: print('ERRO  ', e)
    if errors and 'GITHUB_ACTIONS' in __import__('os').environ:
        for e in errors: print('::error::' + e)
    print(f"\nPré-teste: {len(errors)} erro(s), {len(warnings)} aviso(s)")
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()
