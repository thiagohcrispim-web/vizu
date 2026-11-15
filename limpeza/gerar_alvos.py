#!/usr/bin/env python3
"""Gera CleanTargets.h a partir de limpeza/targets.json (o mesmo arquivo do Crispy Clean).

Uso: python3 limpeza/gerar_alvos.py
Edite o targets.json e rode de novo; o build não lê JSON em tempo de execução.

Fica de fora, com o motivo no comentário gerado:
  - "admin": o Vizu nunca pede elevação;
  - "recentes": edita arquivos de preferências da Adobe; ainda não portado.
"""
import json, os, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
GRUPOS = {"Sistema": "kGSystem", "Navegadores": "kGBrowsers", "Adobe": "kGAdobe", "Mensageiros": "kGMessengers",
          "Desenvolvimento": "kGDev", "Jogos": "kGGames", "Outros": "kGOther"}
RISCO = {"seguro": "kSafe", "atenção": "kReview", "informar": "kInform"}
TIPO = {"pasta": "kFolder", "photoshop-scratch": "kScratch", "lixeira": "kRecycleBin"}


def c(s):
    if s is None:
        return "nullptr"
    return 'L"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def lista(v):
    if v is None:
        return []
    return v if isinstance(v, list) else [x.strip() for x in str(v).split(";") if x.strip()]


def folha(nome, programa):
    # "Photoshop – cache de fontes e logs" -> "Cache de fontes e logs"
    if " – " in nome:
        f = nome.split(" – ", 1)[1].strip()
        return f[:1].upper() + f[1:]
    return None if nome == programa else nome


def main():
    d = json.load(open(os.path.join(AQUI, "targets.json"), encoding="utf-8-sig"))
    linhas, fora = [], []
    for a in d["alvos"]:
        tipo = a.get("tipo", "pasta")
        if a.get("admin"):
            fora.append(f"{a['id']}: pede administrador")
            continue
        if tipo == "recentes":
            fora.append(f"{a['id']}: tipo \"recentes\" ainda não portado")
            continue
        prog = a.get("programa", a["nome"])
        idade = int(a.get("idade_minima_horas", 0) or 0)
        if a["id"] == "temp-usuario":
            idade = -1  # Configurações › Limpeza › Idade dos temporários
        campos = [
            GRUPOS[a["categoria"]], c(prog), c(folha(a["nome"], prog)), c("|".join(a.get("caminhos", []))),
            RISCO[a["risco"]], str(idade), c("|".join(a.get("processos", []))), c(";".join(lista(a.get("padrao", "*"))) or "*"),
            c("|".join(lista(a.get("excluir")))), "true" if a.get("recursivo", True) else "false", str(int(a.get("manter_mais_recente", 0) or 0)),
            TIPO[tipo], "true" if a.get("marcado_por_padrao") and a["risco"] != "informar" else "false",
            c(a.get("consequencia") or a.get("como_limpar") or a.get("descricao") or ""), c(a["id"]),
        ]
        linhas.append("    {" + ", ".join(campos) + "},")
    det = d.get("deteccao") or {}
    out = ["// CleanTargets.h - GERADO por limpeza/gerar_alvos.py a partir de limpeza/targets.json. Não edite à mão.",
           "// Campos: grupo, programa, pasta, caminhos, risco, idade (h; -1 = Configurações), processos, padrão, excluir,",
           "//         recursivo, manter versões mais novas, tipo, marcado, consequência, id.",
           "// Fora da tabela:"] + [f"//   {x}" for x in fora] + [
           "#pragma once", "", "static const Target kTargets[] = {"] + linhas + ["};", ""]
    out += ["// Detecção automática de caches de apps Chromium/Electron/WebView2 (bloco \"deteccao\").",
            f"static const bool kDetectOn = {'true' if det.get('ativa', False) else 'false'};",
            f"static const int kDetectDepth = {min(6, int(det.get('profundidade', 4)))};",
            "static const wchar_t* const kDetectBases[] = {" + ", ".join(c(x) for x in det.get("bases", [])) + "};",
            "static const wchar_t* const kDetectMarkers[] = {" + ", ".join(c(x) for x in det.get("marcadores", ["Cache", "Code Cache", "GPUCache"])) + "};",
            "static const wchar_t* const kDetectFolders[] = {" + ", ".join(c(x) for x in det.get("pastas", [])) + "};",
            "static const wchar_t* const kDetectParentFolders[] = {" + ", ".join(c(x) for x in det.get("pastas_do_pai", [])) + "};",
            "static const wchar_t* const kDetectNames[][2] = {" + ", ".join("{" + c(k) + ", " + c(v) + "}" for k, v in det.get("nomes", {}).items()) + "};",
            "static const wchar_t* const kDetectGroups[][2] = {" + ", ".join("{" + c(k) + ", " + c(v) + "}" for k, v in det.get("categorias", {}).items()) + "};",
            ""]
    open(os.path.join(RAIZ, "CleanTargets.h"), "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print(f"{len(linhas)} alvos, {len(fora)} fora")


if __name__ == "__main__":
    sys.exit(main())
