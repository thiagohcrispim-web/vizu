# Plano: Limpeza (CrispyClean dentro do Vizu)

O CrispyClean deixa de ser um app separado e vira a **Limpeza**, um recurso do Vizu. Ela substitui o item atual
"Limpar arquivos temporários", que hoje usa duas caixas de mensagem e só olha a pasta TEMP (`Vizu.cpp`, `CleanTemp`).

## Decisões

| Ponto | Decisão |
|---|---|
| Visual | Segue o tema escolhido no Vizu (Claro, Escuro, Windows, Vizu Classic, Vizu Crispy), com Inter e Space Grotesk. Da direção "Alumínio" do CrispyClean ficam só a hierarquia, os estados e o movimento. |
| Lugar | Janela própria "Limpeza", como a Agenda. |
| Edições | Vizu e Ultra. A Lite continua só com prévia e miniaturas. |
| Monitor de disco | Widget flutuante separado, como no CrispyClean. |

## Como abre

- Botão direito no fundo da pasta ou na área de trabalho › **Limpeza** (substitui "Limpar arquivos temporários"; `/cleantemp` passa a abrir a janela).
- Menu da bandeja › **Limpeza**.
- Configurações › Outros › Limpeza › **Abrir**.
- Clique no widget de disco.

## Telas (do CrispyClean para o Vizu)

| CrispyClean | No Vizu |
|---|---|
| Início · Analisando · Resultado (3 níveis) · Mapa | Uma janela. A análise começa ao abrir e o resultado se monta no lugar. "Lista \| Mapa" alterna a vista. |
| Confirmação · Limpeza · Relatório | Na própria janela, sem `MessageBox`. Por padrão vai para a Lixeira, com desfazer. |
| Fechar programas | `VizuDialog`, listando os programas abertos que travam arquivos. |
| Configurações | Seção "Outros › Limpeza" das Configurações do Vizu (já tem "excluir sem Lixeira"; ganha grupos, idade mínima e widget). |
| Monitor flutuante | Widget próprio, `WS_EX_TOOLWINDOW` e `WS_EX_LAYERED`, no mesmo padrão do relógio. |

## Código

- `Clean.h`: varredura, regras de segurança e limpeza, portadas de `main/` do CrispyClean, no estilo de `Ops.h` e `Jobs.h`.
- `targets.json` do CrispyClean embutido como recurso (`Vizu.rc`).
- `CleanUi.inc.h`: janela Limpeza, desenhada em Direct2D com `ui::Theme`.
- `DiskWidget.inc.h`: widget de disco.
- `tests/clean_tests.cpp`: os 21 testes do CrispyClean, rodando no workflow de build.
- I18n: pt, en e es, como o resto do Vizu.

Depende do código do CrispyClean (`main/` e `targets.json`), que ainda não está no GitHub.

## Protótipo

`prototipo/Limpeza-A.dc.html`, `Limpeza-B.dc.html` e `Limpeza-C.dc.html`: três composições do resumo, nos temas Vizu
Classic e Claro. Janela de 960 × 640.

- **A · Coluna à esquerda:** resumo e botão fixos numa coluna de 280 px.
- **B · Faixa no topo:** resumo, barra do disco e botão numa faixa; a lista usa toda a largura.
- **C · Cabeçalho que encolhe:** número grande ao abrir; ao rolar, o cabeçalho vira a barra de título com o botão.

Próximo passo: escolher uma composição e então desenhar as demais telas (análise, mapa, confirmação, fechar
programas, relatório, widget e a seção nas Configurações) nos 5 temas.

## Versão

Sai como Vizu 9.9.0, pelo fluxo de `CLAUDE.md`.
