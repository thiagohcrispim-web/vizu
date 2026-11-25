// Receptor dos relatórios de erro do Vizu (Cloudflare Worker).
// Recebe o JSON do Vizu (Diag.h), confere o formato, apaga qualquer coisa que pareça caminho ou nome,
// e abre uma issue no GitHub. O mesmo erro (assinatura) vira comentário na issue que já existe.
//
// Segredo (wrangler secret put GITHUB_TOKEN): token "fine-grained" com acesso SÓ ao repositório
// do Vizu e SÓ à permissão Issues: Read and write. O token fica no Cloudflare, nunca no programa.

const REPO = 'thiagohcrispim-web/vizu';
const LABEL = 'relatorio-automatico';
const MAX_BODY = 8 * 1024;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (request.method !== 'POST' || url.pathname !== '/report') return new Response('Vizu reports', { status: 404 });
    const raw = await request.text();
    if (raw.length > MAX_BODY) return new Response('grande demais', { status: 413 });
    let r;
    try { r = JSON.parse(raw); } catch { return new Response('json inválido', { status: 400 }); }
    if (r.app !== 'vizu' || !/^\d+\.\d+\.\d+$/.test(r.version || '') || !r.kind || !r.signature) return new Response('formato', { status: 400 });

    // Limite simples por instalação: no máximo 20 relatórios por hora (cache do Cloudflare).
    const id = String(r.id || '').replace(/[^0-9a-f]/g, '').slice(0, 16) || 'sem-id';
    const hour = Math.floor(Date.now() / 3600000);
    const cache = caches.default, key = new Request(`https://limite.local/${id}/${hour}`);
    const hit = await cache.match(key);
    const count = hit ? parseInt(await hit.text(), 10) : 0;
    if (count >= 20) return new Response('limite', { status: 429 });
    await cache.put(key, new Response(String(count + 1), { headers: { 'Cache-Control': 'max-age=3600' } }));

    const clean = (s, n = 1500) => String(s || '')
      .replace(/[A-Za-z]:\\[^\s"]*/g, '<caminho>')          // C:\Users\...
      .replace(/\\\\[^\s"]+/g, '<caminho>')                 // \\servidor\...
      .replace(/\/(Users|home)\/[^\s"]*/g, '<caminho>')
      .replace(/[\w.+-]+@[\w-]+\.[\w.]+/g, '<email>')
      .slice(0, n);
    const sig = clean(r.signature, 200);
    const title = `[auto] ${clean(r.kind, 40)} · ${clean(r.where, 80)}`;
    const linha = `| ${clean(r.version, 20)} | ${clean(r.windows, 30)} | ${clean(r.context, 300)} | \`${id}\` |`;
    const detalhe = [r.detail && '**Detalhe**\n```\n' + clean(r.detail) + '\n```', r.selftest && '**Autoteste**: `' + clean(r.selftest, 600) + '`']
      .filter(Boolean).join('\n\n');

    const gh = (path, init = {}) => fetch(`https://api.github.com${path}`, {
      ...init,
      headers: { Authorization: `Bearer ${env.GITHUB_TOKEN}`, Accept: 'application/vnd.github+json', 'User-Agent': 'vizu-report-worker', 'Content-Type': 'application/json' },
    });

    // Já existe issue aberta com essa assinatura? Vira comentário.
    const q = encodeURIComponent(`repo:${REPO} is:issue is:open label:${LABEL} in:body "${sig.replace(/"/g, '')}"`);
    const found = await (await gh(`/search/issues?q=${q}&per_page=1`)).json();
    if (found.items && found.items.length) {
      await gh(`/repos/${REPO}/issues/${found.items[0].number}/comments`, {
        method: 'POST', body: JSON.stringify({ body: `Aconteceu de novo:\n\n| versão | Windows | contexto | instalação |\n|---|---|---|---|\n${linha}\n\n${detalhe}` }),
      });
      return new Response('ok', { status: 200 });
    }
    const body = `Relatório automático enviado pelo Vizu (o usuário aceitou enviar relatórios anônimos).\n\n` +
      `**Assinatura**: \`${sig}\`\n\n| versão | Windows | contexto | instalação |\n|---|---|---|---|\n${linha}\n\n${detalhe}\n\n` +
      `Endereços \`+0x...\` traduzem para nome de função com o \`build/Vizu.map\` da mesma versão.`;
    const res = await gh(`/repos/${REPO}/issues`, { method: 'POST', body: JSON.stringify({ title, body, labels: [LABEL] }) });
    return new Response(res.ok ? 'ok' : 'erro github', { status: res.ok ? 201 : 502 });
  },
};
