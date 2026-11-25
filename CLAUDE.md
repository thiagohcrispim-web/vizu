# Publicação de versões do Vizu

A publicação é feita pelo workflow `.github/workflows/release.yml`.

Quando o usuário pedir uma nova versão:

1. Atualize a versão nos fontes e recursos e gere `dist/Vizu_Setup_<versão>_x64.exe` (por exemplo `dist/Vizu_Setup_19.5.5_x64.exe`). O nome do instalador sempre leva a versão. `build.sh` já gera esse arquivo.
2. Atualize `latest.json` (versão, link e o SHA-256 exato do instalador com o nome da versão) e o histórico no README, incluindo os links de download.
3. Execute `python3 .github/scripts/release.py check` antes de enviar.
4. Envie o código pela branch de trabalho e integre à `main` conforme a autorização do usuário.
5. Aguarde Build, Bundled vector engine tests e Vector conversion tests do mesmo commit da `main`.
6. O GitHub cria a tag `vX.Y.Z`, anexa o instalador verificado e publica a release automaticamente.
7. Confira o workflow Publish Vizu release e o anexo antes de dizer que a publicação terminou.

Não peça ao usuário para criar tags ou anexar instaladores manualmente. Não tente enviar tags pelo proxy do Claude na nuvem. Se a automação falhar, leia o erro e corrija a causa. Nunca reutilize uma versão publicada para um commit diferente: incremente a versão. Não force tags, não sobrescreva anexos diferentes e não adicione tokens pessoais ao repositório.


# Pré-teste (obrigatório para qualquer IA ou pessoa que mexer no código)

Antes de enviar qualquer mudança, rode `python3 .github/scripts/precheck.py`. O Build do GitHub roda o mesmo pré-teste primeiro e para se houver erro.

- **Só existe o que compila.** O programa é `Vizu.cpp` (com os `.h`/`.inc.h` que ele inclui), a DLL `DesignThumbs.cpp` e os testes em `tests/`, compilados pelo `build.sh` com MinGW. Não há vcpkg nem CMake no build oficial.
- **Arquivos em `.github/precheck/fora-do-build.txt` não entram no programa.** Editar `Config.h`, `ConfigModern.h`, `ThemeManager.cpp`, os `engines/` etc. não muda nada. Configurações ficam na `struct Config` do `Vizu.cpp`, com leitura e gravação em `LoadConfig`/`SaveConfig`.
- **Não suponha que algo existe.** Antes de usar um campo, função, token ou arquivo, confirme que está declarado *antes* do ponto de uso, no código que compila. Não invente hashes de commit, versões de ações ou URLs.
- **Não substitua código testado por código novo sem teste.** Mantenha o caminho antigo até o novo passar nos testes do CI.
- **Compile de verdade antes de dizer que terminou.** Se não puder compilar localmente, envie e espere o Build passar.
- **Cada bug corrigido vira uma regra.** Ao corrigir um erro que poderia voltar, acrescente um bloco em `.github/precheck/regras.txt` (id, arquivos, proibido/exigido, motivo, origem) e confirme que o pré-teste o pegaria na versão com o bug.
