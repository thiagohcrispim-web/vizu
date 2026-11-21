# Protótipo da interface do Vizu (9.13)

Arquivos de referência das telas aprovadas e dos temas. Não são carregados pelo programa.

- `canvas.json`: índice do canvas (posição, tamanho e título de cada tela).
- `*.dc.html`: uma tela por arquivo.
  - `Config*.dc.html`: Configurações da 9.12 (veja a seção abaixo).
  - `Settings-copia.dc.html`, `Settings-Lite-copia.dc.html`: Configurações até a 9.11, guardadas como registro.
  - `V-*`: tema Vizu Classic. `L-*`: tema Claro. `D-*`: tema Escuro. `CrispyV2-*`: tema Vizu Crispy (visual V2, 9.13; substitui o antigo `C-Settings`). `W-*`: Padrão Windows.
  - `Inst-*-copia`: telas aprovadas do instalador.
  - `Ring`, `Toast`, `Preview`, `Clock`, `Ocr`, `Agenda`, `Extras`, `Picker`: telas de uso e ferramentas.

O `canvas.json` é o índice exportado pela ferramenta de design e lista mais telas do que há aqui. Não estão nesta pasta: as variações de tema (`L-`, `D-`, `W-`, `C-`) de Ring, Preview, Toast, Clock, Ocr e Agenda; `V-Agenda`, `V-Clock`, `V-Ocr`, `V-Toast`, `V-Extras` e `Main`. `V-Settings` era idêntico a `Settings-copia`, e `Settings-Lite` a `Settings-Lite-copia`; ficou só uma cópia de cada.

## Limpeza (9.9)

Janela Limpeza (antigo CrispyClean). Plano em `../limpeza-plano.md`.

- `Limpeza-A`, `Limpeza-B`, `Limpeza-C`: três composições do resumo. **Escolhida: B (faixa no topo).** A e C ficam como registro.
- `Limpeza-B1` a `B8`: Analisando, Resultado, Mapa, Confirmar, Fechar programas, Limpando, Relatório e a seção nas Configurações, em Vizu Classic e Claro.
- `Limpeza-B9`: widget de disco nos 5 temas (normal, pouco espaço, aberto).
- `Limpeza-B10`: resultado nos 5 temas e a janela na largura mínima (560 px).
- `Limpeza-B11`: widget de desempenho nos 5 temas (leve, lento, aberto com o que está pesando).

## Configurações com presets (9.12)

Página "2 · Vizu Classic" do canvas. O programa segue estas telas (`PaintSettings` em `Vizu.cpp`).

- `Config.dc.html`: preset **Design** (padrão), com todos os "Avançado" fechados.
- `Config-Minimal.dc.html`: preset **Minimal**: só Ver com a barra de espaço e OCR ligados.
- `Config-Custom.dc.html`: **Custom**, que entra sozinho quando uma opção muda depois de aplicar Minimal ou Design.
- `Config-Avancado.dc.html`: os quadros de "Avançado" abertos.
- `Config-Lite.dc.html`: edição Lite, sem presets, com Ver, Outros (OCR) e Sistema.

Chaves de registro novas em `HKCU\Software\DesignThumbs`: `SettingsPreset` (0 Minimal, 1 Design, 2 Custom; 100 = aplicar Design na primeira abertura depois de uma instalação nova), `ClockOcr` (botão do OCR no relógio), `ClockChat` (botão do ChatGPT no relógio, 9.12.1; o endereço continua em `ClockApp`), `OcrHotkey` e `CleanHotkey` (modificadores << 16 | tecla virtual; o OCR usa Ctrl+Alt+T por padrão e a Limpeza começa sem atalho).

## Vizu Crispy V2 (9.13)

Página "5.2 · Crispy V2" do canvas, 21 telas. O tema continua se chamando **Vizu Crispy**; só o visual mudou.

- `CrispyV2-Config*.dc.html`: Configurações (Design, Minimal, Custom, Avançado aberto e Lite).
- `CrispyV2-Usar-*`: Espiar, Converter, Aviso e bandeja, Conta-gotas.
- `CrispyV2-Ferr-*`: Relógio e Foco, Ler texto, Agenda, Extras.
- `CrispyV2-Limpeza-B1` a `B8`: a janela Limpeza do analisando às Configurações.

Tokens (em `ui::CrispyTheme` e `ui::Painter::Card` no `Ui.h`): fundo #08080A, cor dominante #FF6633 ("Cor do neon"), base dos cartões #16171B com degradê de 115° (rgba(10,11,14,.94) → rgba(18,19,24,.94) 45% → rgba(22,23,27,.92) 75% → rgba(30,32,40,.90)), borda de 1 px em degradê (branco 10% → laranja 18% em 35% → laranja 55% em 70% → #FF8A5C), filete branco de 6% no topo, divisórias #232326, texto secundário #A8A8AF.

Como o Direct2D reproduz o que é CSS no protótipo:

- `backdrop-filter`: acrílico do DWM (`DWMWA_SYSTEMBACKDROP_TYPE`) com grafite a 55% por cima; sem ele, #08080A com as manchas laranja e azulada (`ui::Backdrop`). Com o acrílico, os cantos são os do Windows 11.
- Neon (`box-shadow` em duas camadas): camadas concêntricas translúcidas (`Painter::Neon`), só nos controles que usam a cor dominante.
- Desde a 9.13.1 o app não desenha o brilho atrás dos cartões nem o granulado das telas: a separação vem do degradê e da borda.
- Luz que segue o cursor: pincel radial na posição do mouse, no preenchimento e na borda (`Painter::CursorLight`).
- `transition`: `ui::Anim`, com valores que deslizam até o alvo (ease-out cubic-bezier(0.23, 1, 0.32, 1) nas entradas, ease-in-out no que já está na tela). Só roda enquanto algo anima e nunca com a janela oculta ou minimizada. Com "Mostrar animações no Windows" desligado, sobram só fades de até 150 ms.
