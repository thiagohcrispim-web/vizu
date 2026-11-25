# Receptor de relatórios do Vizu

O Vizu manda relatórios de erro anônimos (só com o aceite do usuário) para este Worker. O Worker
abre uma issue no GitHub com a etiqueta `relatorio-automatico`. O mesmo erro vira comentário na
issue que já existe, sem abrir outra.

O token do GitHub fica guardado no Cloudflare, nunca dentro do programa.

## Colocar no ar (uma vez, ~10 minutos)

1. **Token do GitHub**: GitHub → Settings → Developer settings → Fine-grained tokens → Generate new token.
   - Repository access: *Only select repositories* → `vizu`
   - Permissions → Repository → **Issues: Read and write** (e nada mais)
   - Copie o token.
2. **Etiqueta**: no repositório, Issues → Labels → New label → `relatorio-automatico`.
3. **Cloudflare** (conta grátis em cloudflare.com). Depois, num terminal com Node instalado:
   ```
   cd server/report-worker
   npx wrangler login
   npx wrangler secret put GITHUB_TOKEN     (cole o token)
   npx wrangler deploy
   ```
   O deploy mostra o endereço, por exemplo `vizu-reports.SUA-CONTA.workers.dev`.
4. Em `Diag.h`, ponha esse endereço em `kReportHost` (sem `https://`) e publique uma versão nova do Vizu.

Enquanto `kReportHost` estiver vazio, o Vizu só grava o `erros.log` local e não envia nada.

## Testar

```
curl -X POST https://vizu-reports.SUA-CONTA.workers.dev/report -H "Content-Type: application/json" \
  -d "{\"app\":\"vizu\",\"version\":\"19.5.4\",\"kind\":\"teste\",\"where\":\"manual\",\"signature\":\"teste|manual\",\"id\":\"0123456789abcdef\"}"
```

Deve aparecer uma issue `[auto] teste · manual`.

## O que chega num relatório

| Campo | Exemplo |
|---|---|
| versão | 19.5.4 |
| Windows | 10.0.26100 |
| contexto | tema=Crispy escala=150% edicao=Ultra idioma=pt recursos=relogio,espaco,alt,PerfWidget |
| tipo e onde | `desenho falhou` em `ui::Surface::End hr=0x88990012` |
| detalhe | texto do crash.log: código do erro e endereços `+0x...` |
| autoteste | `desenho1=ok desenho2=vazio registro=ok versao=ok fonte=ok` |
| instalação | número aleatório, só para agrupar erros do mesmo PC |

Nunca vão nomes de arquivo, caminhos, nome de usuário ou nome do PC. O Worker ainda apaga
qualquer coisa com cara de caminho ou e-mail antes de criar a issue.
