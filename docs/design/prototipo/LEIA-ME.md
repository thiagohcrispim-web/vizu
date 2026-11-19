# Protótipo da interface do Vizu (9.12)

Arquivos de referência das telas aprovadas e dos temas. Não são carregados pelo programa.

- `canvas.json`: índice do canvas (posição, tamanho e título de cada tela).
- `*.dc.html`: uma tela por arquivo.
  - `Config*.dc.html`: Configurações da 9.12 (veja a seção abaixo).
  - `Settings-copia.dc.html`, `Settings-Lite-copia.dc.html`: Configurações até a 9.11, guardadas como registro.
  - `V-*`: tema Vizu Classic. `L-*`: tema Claro. `D-*`: tema Escuro. `C-*`: tema Crispy. `W-*`: Padrão Windows.
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

Chaves de registro novas em `HKCU\Software\DesignThumbs`: `SettingsPreset` (0 Minimal, 1 Design, 2 Custom; 100 = aplicar Design na primeira abertura depois de uma instalação nova), `ClockOcr` (botão do OCR no relógio), `OcrHotkey` e `CleanHotkey` (modificadores << 16 | tecla virtual; o OCR usa Ctrl+Alt+T por padrão e a Limpeza começa sem atalho).
