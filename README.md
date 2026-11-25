<p align="center"><img src="docs/hero.png" alt="Vizu: miniaturas e preview de arquivos de design no Explorer do Windows" width="100%"></p>

<h3 align="center">Veja todos os seus arquivos de design no Windows,<br>mesmo sem os programas instalados.</h3>

<p align="center">
  <a href="https://github.com/thiagohcrispim-web/vizu/releases/latest/download/Vizu_Setup_19.5.5_x64.exe"><b>⬇ Baixar o Vizu para Windows</b></a> &nbsp;·&nbsp; grátis e de código aberto (MIT)
</p>

---

O Vizu ensina o Explorer do Windows 10 e 11 a mostrar **miniaturas e preview** de arquivos de Photoshop, Illustrator,
InDesign, CorelDRAW, Affinity, Sketch, fotos RAW e muito mais. Sem abrir nenhum programa, sem ter os programas
instalados. E, de quebra, junta no mesmo lugar as ferramentas que você usa o dia todo: converter, recortar, ler texto
de imagens, traduzir, cronometrar o foco e organizar o seu dia.

**Vizu 9.8:** escolha a edição no instalador (cerca de 2 MB):

| Edição | O que tem |
|---|---|
| **Vizu Lite** | Prévia com Espaço e miniaturas no Explorer |
| **Vizu** | Tudo do Lite + menu circular (converter, ferramentas, PDF, extras), OCR, relógio e agenda |
| **Vizu Ultra** | Tudo do Vizu + CDR/SVG para **EPS vetorial** (baixa o motor Inkscape, ~100 MB, uma única vez) |

## Veja tudo, sem abrir nada

<img src="docs/formatos.png" alt="Formatos suportados" width="100%">

Miniaturas direto nas pastas e no painel de visualização do Explorer (`Alt+P`). Nada de ícone genérico: você vê a
arte, a capa da revista, a prancheta do Illustrator, a foto da câmera.

## Espaço pra ver

<img src="docs/preview.png" alt="Preview com a barra de espaço" width="100%">

Selecione um arquivo e aperte **Espaço**: um preview grande abre na hora, como o Quick Look do Mac. Setas trocam de
arquivo, a roda do mouse dá zoom, `Z` mostra em 100%. Embaixo aparecem tamanho, DPI, modo de cor, camadas e até as
fontes usadas no arquivo (as que faltam no seu PC ficam em vermelho). Passe o mouse para ver o **HEX** de qualquer cor,
e clique para copiar.

## Alt pra resolver

<img src="docs/ring.png" alt="Menu circular de conversão" width="100%">

Selecione arquivos e segure **Alt**: um menu circular aparece em volta do mouse, só com o que faz sentido para eles.

- **Converter** para PNG, JPG, PDF, WEBP, TIFF, SVG ou EPS, ou **compactar em ZIP**.
- **CorelDRAW** (CDR e CMX) para **PDF vetorial**, **SVG** e PNG/JPG/WEBP/TIFF em alta resolução, sem instalar nada: o desenho é lido de verdade (libcdr), não a miniatura guardada no arquivo. Os textos viram curvas com as fontes do Windows, para a aparência ficar igual em qualquer computador. Preenchimentos com textura e efeitos (sombras, lentes) ainda não são reproduzidos.
- **PDF** para documentos do Word, Excel e PowerPoint (DOC, DOCX, XLS, XLSX, PPT, PPTX, RTF e OpenDocument): usa o Microsoft Office, se estiver instalado, ou o LibreOffice (gratuito). O documento original não é alterado; arquivos com senha não são convertidos.
- **PPT** (aparece só quando a seleção é de PDFs): converte o PDF em apresentação do PowerPoint com o **texto editável** — fonte, tamanho, cor, negrito e itálico; formas, tabelas e fotos ficam no fundo, sem texto duplicado. Texto girado ou contornado continua como imagem. Em PDF escaneado, cada linha só vira texto quando o fundo em volta é liso (camada de texto do PDF ou OCR do Windows).
- **Ferramentas**: redimensionar, recortar com proporções prontas, girar, espelhar, tons de cinza, tirar metadados.
- **PDF**: juntar na ordem em que você clicou, dividir, extrair páginas, comprimir.
- **Extras**: exportar camadas do PSD, folha de cores, HEX/CSS, converter paletas ASE ⇄ ACO, amostra de fonte.

Os arquivos novos ficam ao lado dos originais. Nada é sobrescrito.

**Conversão vetorial SVG/EPS:** disponível para arquivos CDR e SVG, quando o Conversor de Arquivos Vetoriais é selecionado no instalador. O motor portátil está incluído no pacote e é configurado automaticamente. Os objetos são lidos do documento original; bitmaps não são incorporados como substitutos dos vetores. Efeitos incompatíveis bloqueiam EPS com diagnóstico. Veja [configuração do motor e limitações](docs/vector-conversion.md).

## Ler texto de qualquer lugar

<img src="docs/ocr.png" alt="OCR e tradução" width="100%">

Aperte **Ctrl+Alt+T** (dá para trocar em Configurações › Outros), arraste sobre qualquer coisa na tela (imagem, PDF, vídeo, site) e o texto já vai para a área de
transferência. Um clique traduz para **português, espanhol ou inglês**. A leitura é feita no seu próprio PC; o texto
só sai dele quando você pede a tradução.

## Relógio, foco e o seu dia

<img src="docs/clock.png" alt="Relógio com Pomodoro e roubo de tempo" width="100%">

Um relógio discreto na área de trabalho, ou dentro da barra de tarefas, com:

- **Foco (Pomodoro)** por cores: 25 minutos de foco, 5 de pausa, e o relógio muda de cor.
- **Roubo de tempo**: alguém parou na sua mesa? Clique na caveira para contar, e de novo para registrar. No fim do dia
  você sabe exatamente quanto tempo perdeu.
- **Dia**: sua agenda, com avisos 10 e 5 minutos antes de cada compromisso.

<img src="docs/agenda.png" alt="Agenda do dia e avisos" width="100%">

## E mais

<img src="docs/extras.png" alt="Limpeza de temporários, Pastas Padrão Vizu e mais" width="100%">

- **Pastas Padrão Vizu**: botão direito › Novo cria `Edit`, `Links` e `Preview` (cada uma com `_Morto`) de uma vez.
- **Limpeza** (botão direito no fundo da pasta, ou pela bandeja): acha temporários, caches de navegadores, do Windows e da
  Adobe, mostra tudo em lista ou mapa e limpa com um clique. Arquivos de trabalho (PSD, AI, INDD, PDF…) e o que estiver
  em uso nunca saem; os itens para revisar vão para a Lixeira. Widgets opcionais mostram o espaço em disco e se o PC
  está lento, e o que está pesando.
- Ícone na área de notificação para abrir as Configurações, reiniciar ou encerrar.

## Do seu jeito

<img src="docs/temas.png" alt="Temas: Claro, Escuro, Windows, Vizu Classic e Vizu Crispy" width="100%">

Cinco temas: **Claro**, **Escuro**, **Windows** (segue o seu Windows e a cor de destaque), **Vizu Classic** e
**Vizu Crispy** (neon na cor que você escolher). Em português, inglês e espanhol. Cada recurso pode ser ligado ou
desligado nas Configurações.

<p align="center"><img src="docs/settings.png" alt="Configurações do Vizu" width="60%"></p>

## Instalar

1. Baixe o **[instalador](https://github.com/thiagohcrispim-web/vizu/releases/latest/download/Vizu_Setup_19.5.5_x64.exe)** e abra. Não precisa de administrador.
2. Pronto: as miniaturas aparecem conforme você navega pelas pastas.

Para atualizar, o Vizu avisa sozinho quando sai versão nova. Para desinstalar, rode o instalador de novo ou use
**Configurações do Windows › Aplicativos**.

> **O Windows ou o navegador avisou que o arquivo é suspeito?** É um alarme falso: o Vizu é novo e ainda não tem
> assinatura digital, então o SmartScreen desconfia. Clique em **Mais informações › Executar assim mesmo** (no Chrome,
> **Manter**). O código está todo aqui, e o instalador é compilado pelo GitHub Actions a partir deste repositório.

## Privacidade

O Vizu não envia nada sobre você. Ele só acessa a internet para: procurar versão nova uma vez por dia (consulta o
`latest.json` deste repositório; dá para desligar) e traduzir um texto quando você clica em PT, ES ou EN. Mais nada.
Se você ligar o **widget de uso de IA** (desligado por padrão), ele lê o login que o Claude Code e o Codex já guardam no
seu PC (`.claude\.credentials.json` e `.codex\auth.json`) e consulta o uso direto na Anthropic e na OpenAI, a cada 5 minutos.
O token vai só para o serviço dono dele e o Vizu nunca o grava. Perto de vencer, o Vizu roda `claude -p` escondido para o próprio
Claude Code renovar o login. O botão "Instalar e entrar" usa o instalador oficial da Anthropic (`claude.ai/install.ps1`). A ideia veio do
[Codenotch](https://github.com/vinzdg/codenotch) (MIT, © 2026 Vinz). O **widget de bateria** só lê o Windows, sem internet.
A política de privacidade do GitHub se aplica à consulta de atualização:
https://docs.github.com/site-policy/privacy-policies/github-general-privacy-statement

<details>
<summary><b>Todos os formatos</b></summary>

| Tipo | Extensões | Origem da imagem |
|---|---|---|
| Photoshop | psd, psb | imagem composta (8/16/32 bits, RGB/Cinza/CMYK) |
| Illustrator | ai | miniatura XMP; no preview e na conversão, cada prancheta renderizada pelo leitor de PDF do Windows |
| PDF | pdf | painel Alt+P, preview e conversão, página a página (leitor de PDF do Windows) |
| InDesign | indd, indt | miniaturas XMP, uma por página |
| EPS | eps, epsf, epsi | preview TIFF/WMF, XMP ou EPSI |
| SVG | svg, svgz | Direct2D |
| Pacotes | cdr, afdesign, afphoto, afpub, xd, sketch, kra, ora, procreate | preview guardado no pacote (na conversão, CDR e CMX são desenhados a partir dos vetores) |
| Paletas | ase, aco | miniatura com as amostras; no preview, nome e HEX de cada cor |
| Fontes | ttf, otf, ttc | painel Alt+P e preview com folha de amostra (miniaturas ficam com o Windows) |
| RAW de câmera | cr2, cr3, crw, nef, nrw, arw, srf, sr2, dng, raf, orf, rw2, rwl, pef, srw, 3fr, erf, kdc, mrw, mef, iiq | JPEG de preview embutido, com rotação |
</details>

<details>
<summary><b>Atalhos do preview</b></summary>

| Tecla | Ação |
|---|---|
| Espaço / Esc / clique direito | fecha |
| Setas ou clique em outro arquivo | troca o preview |
| Roda do mouse, + / - | zoom no cursor |
| Arrastar | mover |
| Z / 0 / duplo clique | 100% / ajustar / alternar |
| PgUp / PgDn ou setas ‹ › na imagem | páginas (InDesign) |
| Enter | abre no programa padrão |
| Ctrl+C | copia a imagem (PNG com transparência) |
| Passar o mouse / clicar | conta-gotas: mostra o HEX da cor / copia |
| ⚙ (canto superior esquerdo) | Configurações |
</details>

<details>
<summary><b>Para desenvolvedores</b></summary>

- Compilar no Linux: `./build.sh` (MinGW `g++-mingw-w64-x86-64-posix`; os headers mais novos do mingw-w64 estão em `third_party/`).
- O GitHub Actions compila a cada envio e publica o instalador como artefato (`.github/workflows/build.yml`).
- Instalação silenciosa: `Vizu_Setup_19.5.5_x64.exe /install /S` · desinstalar: `Vizu.exe /uninstall [/S]`.
- Diagnóstico de um arquivo: `Vizu.exe /diag "arquivo"` ou arraste o arquivo sobre `diagnostico.bat`. Sem arquivo (`Vizu.exe /diag` ou dois cliques no `.bat`): diagnóstico do Espaço e do Alt.
- A biblioteca `DesignThumbs.dll` (miniaturas do Explorer) e a chave de registro `HKCU\Software\DesignThumbs` mantêm o
  nome antigo, para não quebrar as miniaturas nem as configurações de quem já usa.
</details>

<details>
<summary><b>Histórico de versões</b></summary>

- **v19.5.5** — Liga o envio de relatórios de erro anônimos (para quem aceitar em Configurações › Sistema): os erros viram issues no GitHub com a etiqueta `relatorio-automatico`.
- **v19.5.4** — Atualização não fica mais oferecendo versão nova sem parar (a versão interna dizia 9.14.5). Novo registro de erros em `%LOCALAPPDATA%\DesignThumbs\erros.log`, autoteste rápido ao abrir, vigia de travamento e opção "Enviar relatórios de erro anônimos" (desligada por padrão) em Configurações › Sistema.
- **v19.5.3** — Corrige a janela de Configurações preta: os cantos arredondados eram reaproveitados entre janelas com fábricas de desenho diferentes e o Windows descartava o quadro. Também deixa de vazar memória de desenho.
- **v19.5.2** — Correção da v19.5.1, que não compilava: relógio, widgets, agenda, chuva digital e animações com mola voltam a funcionar; conversão CorelDRAW pelo leitor libcdr testado; publicação automática restaurada.
- **v19.5.1** — **Grande salto**: base dos motores próprios (VizuVector, VizuPeek, VizuIDML) em desenvolvimento — nesta versão CDR/CMX continuam saindo pelo leitor libcdr já testado. Chuva digital no tema Classic (WorkerW layer, loop perfeito, hash determinístico). Sistema de design completo: squircle com cache, concentricidade, tokens de espaçamento 4px, tipografia Inter, animações com mola (ui::Spring). Limpeza profunda: timers só quando janela visível, TrimMemory, DestroyWindow, remoção de código legado (DesignPeek, migrações antigas), buffers estáticos → locais, leitura por mapping. Flags modernas: -flto, -march=x86-64-v2, -Wl,--gc-sections em todas as etapas. Repositório limpo: dist/ em .gitignore, protótipos movidos para branch/wiki.
- **v9.14.4** — Tudo da 9.14.3 (Alt na Área de Trabalho, arquivos do Affinity e bateria anexada), que não chegou a ser publicada porque o teste automático não conseguia baixar o motor vetorial do inkscape.org. O teste agora usa uma cópia guardada no GitHub, conferida pelo mesmo SHA-256; para os usuários, o download continua direto do inkscape.org.
- **v9.14.3** — Alt na Área de Trabalho volta a abrir o menu de converter. Arquivos do Affinity (`.af` e os antigos `.afdesign`, `.afphoto`, `.afpub`) ganham miniatura, prévia e conversão para PNG, JPG, PDF, WEBP e TIFF pela miniatura que o Affinity guarda no arquivo. A bateria anexada à barra de tarefas não abre mais o cartão: realça ao passar o mouse e atualiza com um clique.
- **v9.14.2** — **Uso de IA:** as logos do Claude e do ChatGPT no lugar das letras, no trilho e no cartão. Novas opções em Configuração › Monitor: mostrar o **restante** em vez do usado; **limites de cor** ajustáveis (atenção e crítico); **anel semanal** por dentro ou por fora; **exibição** sempre, ao passar o cursor (o trilho vira uma faixa fina e abre ao chegar o mouse) ou oculta; **recolher em tela cheia** (vídeos, jogos, apresentações); **aviso quando um limite renovar**, com som opcional e prévia; **tamanho** pequeno, médio ou grande; e **Recentralizar** (também no menu do botão direito). **Espaço e Alt agora funcionam nos arquivos da área de trabalho**, não só nas janelas do Explorer.
- **v9.14.1** — **Nova seção Monitor** na Configuração, com os widgets de disco, desempenho, uso de IA e bateria. **Uso de IA:** um trilho preso à borda da tela, com um anel por IA mostrando o limite de 5 horas do Claude e do ChatGPT; passe o mouse para ver a sessão, o limite semanal e quando cada um volta. Arraste para cima e para baixo ou leve até outra borda (esquerda, direita, topo ou base). Um botão instala o Claude Code e abre o login, e o Vizu pede ao próprio Claude Code para renovar o login antes de vencer. **Bateria:** fones, mouses, teclados e controles Bluetooth, do mais fraco para o mais forte, com destaque abaixo de 15%, e pode ser anexada à barra de tarefas como o relógio.
- **v9.14.0** — **Dois widgets novos** (em Configuração › Limpeza › Avançado, desligados por padrão): **uso de IA**, com os limites de 5 horas e semanal do Claude e do ChatGPT e quando cada um volta, lido pelo login do Claude Code e do Codex; e **bateria**, com o nível dos fones, mouses, teclados e controles Bluetooth, do mais fraco para o mais forte, com destaque abaixo de 15%.
- **v9.13.3** — **Espaço e Alt no Explorer novo do Windows** (builds Insider com partes em WinUI): o Vizu não conseguia ler os arquivos selecionados, então a prévia não abria e o Alt mostrava o menu de atalhos em vez do de converter. Agora, se a lista não for encontrada pelo identificador, ele usa a aba ativa da mesma janela. A inspeção também testa a leitura da seleção, solta o Alt sem ativar o menu do Explorer e não conta PDFs (só prévia no Vizu) nem o gerador do CorelDRAW como erro de miniatura.
- **v9.13.2** — **Inspeção do Vizu** (Configurações › Sistema › Avançado): um botão testa de verdade o Espaço (prévia), o Alt com arquivo selecionado (menu de converter), o Alt sem seleção (menu de atalhos) e as miniaturas de uma pasta que você escolhe, comparando a imagem do Vizu com a que o Explorer mostra. O resultado vira um relatório em `%LOCALAPPDATA%\Vizu\Inspecao` (Markdown com os dados em JSON, fácil de ler por pessoas e por assistentes como Claude ou Codex) e o Vizu abre um e-mail pronto para o suporte com o relatório anexado.
- **v9.13.1** — **Crispy mais limpo**: sem granulado na janela e sem brilho laranja atrás dos cartões; o neon fica só nos controles (chaves, seletores, botão principal, links). **InDesign**: o Vizu mostra o total real de páginas; as que o InDesign não salvou no arquivo (por padrão ele guarda só as 2 primeiras) aparecem com o caminho para liberar todas nas preferências do InDesign. **PSD**: canais extras (seleções salvas, cores especiais) não são mais lidos como transparência, o que deixava miniaturas em branco. `diagnostico.bat` com um arquivo agora mostra qual gerador de miniaturas o Windows usa para a extensão (se não for o do Vizu, outro programa tomou o lugar) e quanto da imagem saiu branca.
- **v9.13.0** — **Tema Crispy com visual novo** (o nome continua Vizu Crispy): janela de vidro grafite com o acrílico do Windows (sem ele, preto #08080A com manchas laranja e azulada), cartões em degradê com borda que vai do branco ao laranja, brilho laranja atrás de cada cartão e uma luz suave que segue o mouse, neon nas chaves ligadas, no preset ativo, nos botões principais e nos links, barra de rolagem laranja e granulado fino. **Animações em todos os temas**: chaves e seletores deslizam, *Avançado* abre e fecha suave, Configurações, Limpeza e Agenda abrem e fecham com fade e escala, os avisos entram deslizando e saem com fade, o menu circular abre e fecha com escala e a fatia ativa acende ao trocar, e números e barras da Limpeza contam até o valor final. Com "Mostrar animações no Windows" desligado, sobra só um fade curto. **Atalhos de programas no Alt**: segurando Alt sem nada selecionado, o menu radial mostra 8 espaços. Clique num deles, escolha o `.exe` e ele vira um atalho com o ícone do programa. O botão direito troca o programa, e *Configurações › Sistema › Avançado › Resetar atalhos do menu radial* apaga todos. **Correções**: PSD salvo com "Maximizar compatibilidade" desligado deixava a miniatura e a prévia em branco; agora o Vizu monta a imagem pelas camadas (ou usa a miniatura embutida). Para refazer miniaturas que já ficaram brancas, rode a Limpeza com "Miniaturas do Windows". Espaço e Alt ficaram mais firmes: funcionam com Lupa, Narrador e teclados que mostram cursor na lista, e não dependem só dos eventos de foco do Windows. Se outro programa ocupar o Espaço, aparece um aviso. Dois cliques em `diagnostico.bat` mostram por que o Espaço ou o Alt não respondem.
- **v9.12.1** — Em Configurações › Outros, o **botão do ChatGPT** no relógio ganhou liga/desliga e **Editar botão**: uma caixinha onde você digita outro site (ou o caminho de um programa) para o botão abrir. Em branco volta para o ChatGPT. O preset Minimal desliga o botão; o Design liga.
- **v9.12.0** — **Configurações novas, com presets**: no topo, *Aplicar presets* troca entre **Minimal** (só o Espaço e o OCR, o resto desligado), **Design** (quase tudo ligado; o padrão de instalações novas) e **Custom**, que entra sozinho quando você muda alguma opção. A janela ficou mais larga, com o nome da seção numa coluna fixa, chaves com **OFF/ON** e cada seção mostrando só o essencial; o resto fica em **Avançado**, recolhível. Novidades: atalho de teclado configurável para o **OCR** (Ctrl+Alt+T continua o padrão) e para abrir a **Limpeza**, botão do OCR no relógio que pode ser escondido, OCR também no **Vizu Lite**, e *Sobre o programa* e *Agradecimentos* abertos por links. O rodapé ganhou Atualizações, Desinstalar, Pasta do programa e Encerrar processo.
- **v9.11.1** — Quem vinha de uma versão antiga e ficou com a fonte da interface antiga (Bahnschrift ou Segoe UI) volta para a Inter automaticamente; fontes escolhidas por você são mantidas. Ao desinstalar, o Vizu agora apaga o motor de PDF extraído em `%LOCALAPPDATA%\Vizu` e pergunta se também deve apagar as configurações (tema, fontes, relógio, agenda e widgets).
- **v9.11.0** — **CorelDRAW sem Inkscape**: CDR e CMX viram **PDF vetorial**, **SVG** e PNG/JPG/WEBP/TIFF em alta resolução direto no Converter, sem baixar nada. O desenho é lido de verdade (libcdr embutida), não a miniatura do arquivo; textos viram curvas com as fontes do Windows; vários CDRs selecionados viram um PDF só. EPS continua no Vizu Ultra.
- **v9.10.1** — **Word, Excel e PowerPoint → PDF**: com DOC, DOCX, XLS, XLSX, PPT, PPTX, RTF ou OpenDocument selecionados, o lado Converter do menu circular ganha **PDF**. Usa o Microsoft Office instalado (sem abrir janela) ou o LibreOffice (gratuito). O original não é alterado, arquivos com senha não são convertidos e uma instância do Office que já estava aberta nunca é fechada.
- **v9.10.0** — **PDF → PPT**: com PDFs selecionados, o lado Converter do menu circular ganha **PPT**, que transforma o PDF numa apresentação do PowerPoint com o texto editável (fonte, tamanho, cor, negrito e itálico). Formas, tabelas e fotos ficam no fundo sem texto duplicado; texto girado ou contornado continua como imagem; em PDF escaneado, as linhas sobre fundo liso viram texto (camada de texto do PDF ou OCR do Windows). Usa o PDFium (BSD/Apache), embutido e extraído só no primeiro uso.
- **v9.9.2** — O widget de desempenho pode ser anexado à barra de tarefas, como o relógio (botão direito › Anexar à barra de tarefas); o painel completo abre acima da barra ao passar o mouse. Botão direito nos widgets também oculta. Agradecimento especial mais discreto.
- **v9.9.1** — Widget de disco mostra todos os discos (inclusive externos) quando "Todos" está marcado. Botão da Limpeza no relógio, antes do OCR. Ícones do relógio redesenhados, cheios e mais fáceis de reconhecer. Botão **Otimizar** no widget de desempenho: fecha os programas que mais pesam (com a sua confirmação, nunca à força), libera espaço quando o disco está quase cheio e sugere reiniciar quando o PC está ligado há muitos dias.
- **v9.9.0** — **Limpeza**: o Crispy Clean agora faz parte do Vizu. Botão direito no fundo da pasta (ou bandeja) abre a análise de temporários e caches (Windows, navegadores, Adobe, mensageiros, jogos, desenvolvimento e apps detectados sozinhos), em lista ou mapa, com limpeza em um clique. Arquivos de trabalho (PSD, AI, INDD, PDF…) e o que estiver em uso nunca saem; o que é para revisar vai para a Lixeira; programas abertos são fechados só com a sua permissão, nunca à força. Widgets opcionais de espaço em disco e de desempenho (mostra se o PC está lento e o que está pesando). Nova seção Limpeza nas Configurações.
- **v9.8.12** — Interface conforme o canvas do Vizu. Inter e Space Grotesk vêm dentro do Vizu (licença OFL, em `fonts/`), então o visual é o mesmo mesmo sem essas fontes no Windows. Instalador com a tela de escolha, o progresso e a conclusão do protótipo (título, teclas Espaço e Alt, brilho do botão). Configurações com títulos de seção em texto normal, sem cartões, e interruptores laranja nos temas Claro e Classic. Textos das Configurações revisados.
- **v9.8.11** — O instalador passa a levar a versão no nome (`Vizu_Setup_9.8.11_x64.exe`), e a atualização baixa o arquivo com esse nome. Escolha da versão e download do Ultra com o botão Cancelar com contorno, como no protótipo.
- **v9.8.10** — Correções de segurança e robustez: PDFs e PSDs malformados não travam nem derrubam o Vizu; agenda não perde eventos se a gravação falhar; atualização troca a DLL com segurança; OCR junta palavras com hífen só quando faz sentido; erros do conversor vetorial em três idiomas. README explica o uso gratuito (licença MIT).
- **v9.8.9** — Corrige o fechamento do Vizu ao abrir a prévia com Espaço em pastas com SVG (também afetava as miniaturas de SVG). Se o Vizu fechar sozinho, o motivo fica em `%LOCALAPPDATA%\DesignThumbs\crash.log`.
- **v9.8.8** — Telas no visual do Vizu Classic: menu do Alt, Espiar (com botões copiar, abrir e fechar), avisos, relógio, agenda, OCR e conta-gotas. Histórico do roubo de tempo com Exportar CSV e Ver semana; "Atualizações" no menu da bandeja. Mais segurança: arquivos corrompidos não fecham o Vizu, a limpeza de temporários poupa pastas em uso e a atualização só instala com SHA-256 conferido.
- **v9.8.7** — Instalador com novo visual: escolha da versão em lista, progresso com etapas (e MB, velocidade e tempo no download do Ultra) e tela final com as teclas Espaço e Alt.
- **v9.8.6** — Configurações reorganizadas: Tempo (Relógio e Agenda), Outros (OCR, Pastas e Limpeza) e Sistema recolhível, com "Procurar nova versão" na linha de atualização. Cores do Pomodoro atualizadas e OCR com "Juntar" por padrão.
- **v9.8.5** — Nova versão de distribuição, sem mudanças de funcionamento em relação à 9.8.4.
- **v9.8.4** — Download do motor vetorial (Ultra) com progresso real: MB, velocidade e tempo restante. Retoma de onde parou se a conexão cair e permite cancelar.
- **v9.8.3** — Renomeado para Vizu: executável `Vizu.exe`, instalador `Vizu-Setup.exe` e repositório `vizu`.
- **v9.8.2** — Correções do conta-gotas: textos "Sugestão:" traduzidos; busca PANTONE mantém o resultado exato.
- **v9.8.1** — Conta-gotas mostra, embaixo do HEX, o **CMYK** (perfil de impressão do Windows) e o **PANTONE** Coated mais
  próximo (marcado como "Sugestão:", consultivo; confirme no guia físico). O clique continua copiando só o HEX.
- **v9.8** — Instalador com três edições: **Lite**, **Vizu** e **Ultra**. O instalador volta a ter ~2 MB: o Ultra baixa o
  motor vetorial com verificação SHA-256 e não o reinstala nas atualizações. SVG aceita bitmaps embutidos no CDR e o EPS
  aceita gradientes; erros de conversão viram um aviso curto com o detalhe num arquivo. Atualizações conferem o SHA-256.
- **v9.6.2** — SVG convertido agora abre no Illustrator (antes aparecia em branco).
- **v9.6.1** — Converter agora também gera **SVG** e **EPS** (a imagem vai embutida no arquivo; não é vetorização).
- **v9.6** — Identidade **Vizu**: logotipo nas Configurações, tema novo **Vizu Classic** (Design System Vizu:
  grafite e Ardósia, laranja só como preenchimento) e **Vizu Crispy** (o neon, agora translúcido a 85%). Tema Escuro
  com controles no grafite da Vizu; Padrão Windows com o menu circular claro/escuro e a cor de destaque do sistema.
  Configurações reorganizadas: Ver · Converter · Outros · Sistema · Temas; Relógio e OCR abrem seus subitens ao
  ativar; Roubo de tempo, Foco (Pomodoro, com tempos e cores) e Agenda podem ser desligados; Ctrl+Alt+T funciona
  com o relógio desligado; Pastas Padrão Vizu; qualidade JPG/PDF padrão 75%; interface em Inter (se instalada) e
  relógio em Bahnschrift; a tela esmaece para o preto ao abrir as Configurações (a barra de tarefas continua
  visível). Nome e ícone Vizu em todo o programa; código sem uso removido e programa mais leve.
- **v9.5.2** — **Agenda** no DesktopClock (clique no triângulo): dia e semana, linha do tempo com o horário atual,
  novo evento com título, dia, hora, duração e avisos; clique num evento para editar ou excluir. **Avisos** no canto
  inferior direito 10 min antes (some em 20 s) e 5 min antes (com Ok / Adiar 5 min). Ao anexar o relógio à barra
  de tarefas, o relógio do Windows na bandeja é ocultado (e volta ao desanexar).
- **v9.5.1** — DesktopClock: tamanho 100% / 90% / 70% e opção **Anexar à barra de tarefas** (o relógio fica dentro
  da barra, na cor dela, e desliza para os lados). Agradecimento especial nas Configurações.
- **v9.5** — **Ler texto da tela (OCR)** no DesktopClock: botão ao lado do ChatGPT ou **Ctrl+Alt+T**; a tela congela,
  você arrasta sobre o texto, o OCR do próprio Windows lê (sem internet) e o texto vai para a área de transferência.
  Um cartão oferece tradução para **PT / ES / EN** (Google Tradutor, com o MyMemory de reserva), juntar linhas e
  abrir no Google Tradutor. Configurações › DesktopClock: liga/desliga, quebras de linha e idiomas de leitura.
- **v9.4.3** — Crispy com menos luzes no menu circular (sem a régua na borda); o lado Converter fica com PNG, JPG,
  PDF, WEBP, TIFF e **ZIP** (compacta qualquer seleção); saem BMP, GIF, ICO e as predefinições.
- **v9.4.2** — menu circular redesenhado: Crispy com régua na borda e trecho aceso da fatia ativa; Minimalista
  Escuro no mesmo desenho, sem luzes; Minimalista Claro em porcelana com divisórias que somem e sombra difusa.
  Nomes com letras espaçadas e os lados vizinhos visíveis no centro.
- **v9.4** — **DesktopClock** integrado (ative em Configurações › DesktopClock): relógio na área de trabalho com
  qualquer fonte instalada, agenda com lembretes (Dispensar / +5 min), botão de atalho (ChatGPT ou qualquer app),
  **Pomodoro** por cores (foco/pausa: verde/vermelho, azul/cinza, lima/roxo, âmbar/céu) e **Roubo de tempo**
  (caveira: clique para contar quem te interrompeu, clique de novo para registrar; histórico do dia e da semana,
  planilha CSV). Ícones redesenhados no estilo Feather, ícone novo do programa, fonte Bahnschrift em todo o app
  (ou qualquer fonte do Windows em Configurações › Sistema › Fonte da interface).
- **v9.3** — Crispy cyberpunk: menu circular com disco preto fosco, vincos entre as fatias, centro rebaixado e
  fatia de vidro neon; **cor do neon escolhível** (6 cores prontas ou qualquer RGB) em Configurações › Sistema.
- **v9.2** — temas Minimalista Claro, Minimalista Escuro (preto × chumbo), Padrão Windows (cor de destaque do
  sistema) e Crispy (neon); destaque do menu circular em disco de vidro macio; preview com fundo de vidro;
  controles minimalistas; Pasta Padrão Maker cria "_Morto" dentro de cada pasta; botão direito ›
  **Limpar arquivos temporários** (TEMP com mais de 1 dia → Lixeira, depois oferece esvaziar a Lixeira;
  opção de excluir direto em Configurações › Limpeza).
- **v9.1** — WEBP com compressão máxima (método 6, YUV nítido; gráficos tentam sem perdas e fica o menor arquivo);
  menu circular em vidro líquido com brilho laranja que segue o mouse e granulado; ícones e botões minimalistas;
  temas Windows / Claro / Escuro / **Crispy** (preto chumbo, #FF6633, toque neon, fonte Bahnschrift);
  ícone na área de notificação com Configurações, Reiniciar e Encerrar.
- **v9** — menu circular com lados (converter, ferramentas, PDF, extras, predefinições); WEBP, TIFF, ICO, GIF, BMP;
  redimensionar, recortar, girar, espelhar, cinza, sem metadados; juntar/dividir/extrair PDF mantendo o original; comprimir PDF;
  camadas do PSD, paletas (folha, HEX/CSS, ASE⇄ACO), amostra de fonte; instalador oferece desinstalar ao ser reaberto.
- **v8.1** — instalador próprio (o Vizu se instala; sem NSIS), que não reinicia o Explorer; evita o alarme falso de "PUP" de antivírus.
- **v8** — instalador (DesignThumbs-Setup.exe) com desinstalação pelo Windows; atualização automática; paletas .ase/.aco;
  fontes .ttf/.otf; conta-gotas no preview; Pasta Padrão Maker configurável; português, inglês e espanhol.
- **v7.3** — ícone do programa.
- **v7.2** — ferramenta de diagnóstico (`diagnostico.bat`).
- **v7.1** — corrige o PDF travando até o tempo-limite (o arquivo é copiado para a memória antes de ir ao leitor de PDF);
  registro de diagnóstico em `%LOCALAPPDATA%\DesignThumbs\pdf.log`.
- **v7** — PDF e pranchetas do Illustrator página a página (leitor de PDF nativo do Windows); setas discretas no canto
  da imagem no painel Alt+P, só para PDF e InDesign; Pasta Padrão Maker logo abaixo de Pasta/Atalho no menu Novo.
- **v6** — setas de página no painel Alt+P e no preview; Novo › Pasta Padrão Maker (Edit, Links, Preview).
- **v5** — nova interface de vidro (preview, botões do Alt, aviso e configurações), tema claro/escuro do Windows,
  animação de abertura, `Vizu.exe /peek arquivo` para abrir um arquivo direto.
- **v4.2** — botões de conversão no Alt; 72/300 dpi; PDF comprimido ou sem compressão; ordem do PDF = ordem dos cliques.
- **v4.1** — botões de conversão passam do Shift para o Ctrl; Ctrl+clique reinicia a contagem.
- **v4** — painel de visualização (Alt+P), preview em segundo plano com cache e pré-carregamento dos vizinhos,
  conversão para PNG/JPG/PDF segurando Shift (v4.1: Ctrl, v4.2: Alt) (PDF multipáginas na ordem da seleção), configurações (engrenagem),
  fundo claro/escuro.
- **v3** — preview com zoom/arrastar, fundo xadrez, páginas do InDesign, barra de informações com fontes faltando,
  Enter/Ctrl+C, AI nítido via PDF, novos formatos (EPS completo, SVGZ, CDR, Affinity, XD, Sketch, Krita, Procreate, RAW de câmera),
  instalação sem administrador.
- **v2.1** — Vizu sem hook de teclado (WinEvents + atalho só na lista de arquivos), inicialização pela pasta Inicializar.
- **v2** — Vizu (preview com Espaço).
- **v1.1** — PSD lido sob demanda (arquivos grandes e na nuvem); miniaturas XMP truncadas ignoradas.
- **v1** — thumbnail provider inicial.
</details>

---

<p align="center">Vizu · by CrispyPixel · <a href="LICENSE">licença MIT</a></p>
<p align="center"><sub>O Vizu é gratuito. Você pode usar, copiar, modificar e redistribuir o programa, inclusive em trabalhos comerciais, desde que mantenha o aviso de copyright e a licença MIT (<a href="LICENSE">LICENSE</a>). Não há cobrança nem cadastro.</sub></p>
