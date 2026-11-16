# Componentes de terceiros

| Pasta | Projeto | Licença |
|---|---|---|
| `libwebp/` | [libwebp](https://chromium.googlesource.com/webm/libwebp) 1.4.0 (só o codificador) | BSD 3-Clause (`libwebp/COPYING`, `libwebp/PATENTS`) |
| `mingw-w64-headers/` | Headers do [mingw-w64](https://www.mingw-w64.org/) | Domínio público |
| Inkscape portátil (embutido no build) | [Inkscape 1.4.2](https://inkscape.org/) | Licenças originais preservadas; [procedência e fontes](inkscape/NOTICE.md) |
| PDFium (embutido no build, extraído no primeiro uso do PDF → PPT) | [pdfium.dll](https://pdfium.googlesource.com/pdfium/) do wheel oficial [pypdfium2 5.15.2](https://pypi.org/project/pypdfium2/5.15.2/) para Windows x64 (compilação de [pdfium-binaries](https://github.com/bblanchon/pdfium-binaries)), baixado pelo `build.sh` com SHA-256 fixo | BSD 3-Clause / Apache-2.0; componentes (FreeType, libjpeg-turbo, OpenJPEG, Little CMS, libpng, zlib, ICU, HarfBuzz e outros) com licenças permissivas, listadas em `pypdfium2-5.15.2.dist-info/licenses/` dentro do wheel |
| libcdr 0.1.9, librevenge 0.0.6 (embutidos no build, sem modificação) | leitor de CorelDRAW CDR/CMX; fontes oficiais [libcdr-0.1.9.tar.xz](https://dev-www.libreoffice.org/src/libcdr/libcdr-0.1.9.tar.xz) e [librevenge-0.0.6.tar.bz2](https://dev-www.libreoffice.org/src/librevenge-0.0.6.tar.bz2), baixadas pelo `build.sh` com SHA-256 fixo | MPL 2.0 (o código-fonte é o publicado nesses endereços; librevenge usada sob a opção MPL) |
| Little CMS 2.16, zlib 1.3.1, Boost 1.86 (só cabeçalhos) | dependências da libcdr, também baixadas pelo `build.sh` com SHA-256 fixo | MIT; licença zlib; Boost Software License 1.0 |
| `../cdr/icu-shim/` | ponte própria para o ICU do Windows (`icu.dll`), usado pela libcdr para converter codificações de texto | MIT (código do Vizu) |
| `../tests/fixtures/` (arquivos CDR/CMX de teste) | suíte de testes do LibreOffice; veja `tests/fixtures/README.md` | MPL 2.0 |
| `../pptx/` (tema, mestre e layout em branco do PPTX) | partes do modelo padrão do [python-pptx](https://github.com/scanny/python-pptx) 1.0.2 | MIT (`../pptx/LICENSE-python-pptx`) |
