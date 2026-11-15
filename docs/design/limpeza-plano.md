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

Composição escolhida: **B · faixa no topo**. A faixa traz o número, a barra do disco e a ação principal; a lista usa
toda a largura. Na largura mínima (560 px) a faixa quebra em duas linhas e o botão continua visível.

| Prancha | Tela |
|---|---|
| `Limpeza-B1` | Analisando: o número sobe e os grupos entram na lista conforme terminam |
| `Limpeza-B2` | Resultado: Seguro vem marcado, Revisar fica de fora, programa aberto é avisado na linha |
| `Limpeza-B3` | Mapa: área proporcional ao tamanho, cor de destaque = marcado |
| `Limpeza-B4` | Confirmar: diálogo do Vizu com o que vai sair |
| `Limpeza-B5` | Fechar programas: pular o programa ou fechar e limpar |
| `Limpeza-B6` | Limpando: a faixa vira progresso, com Parar |
| `Limpeza-B7` | Relatório: quanto saiu, o que ficou e por quê, com Desfazer por 24 h |
| `Limpeza-B8` | Seção Limpeza nas Configurações |
| `Limpeza-B9` | Widget de disco nos 5 temas: normal, pouco espaço e aberto |
| `Limpeza-B10` | Resultado nos 5 temas e na largura mínima |

No tema Escuro o destaque é cinza-claro (sem laranja) e no Padrão Windows é a cor de destaque do sistema, como no
resto do Vizu.

## Versão

Sai como Vizu 9.9.0, pelo fluxo de `CLAUDE.md`.
