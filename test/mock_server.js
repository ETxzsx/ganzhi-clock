const http = require('http');
const fs = require('fs');
const path = require('path');
const WWW = path.join(__dirname, '..', 'www');
const MIME = { '.html': 'text/html; charset=utf-8', '.js': 'application/javascript; charset=utf-8' };
let dev = { mode: 'run', view: 0, ssid: 'TP-LINK_5G', ip: '192.168.1.88', rssi: -52, prov: 'idle', reason: '', ap: false, chart: { seq: 0, valid: false } };
let provAt = 0;
const server = http.createServer((req, res) => {
  const u = req.url.split('?')[0];
  const json = (o) => { res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' }); res.end(JSON.stringify(o)); };
  const body = (cb) => { let b = ''; req.on('data', (c) => b += c); req.on('end', () => cb(b)); };
  if (u === '/api/now') return json({ epoch: Date.now(), synced: true });
  if (u === '/api/state') return json({ mode: dev.mode, view: dev.view, ssid: dev.ssid, ip: dev.ip, rssi: dev.rssi, synced: true, uptime: 3725, ver: '1.1.0', mac: 'A4:CB:8F:D6:08:A0', ap: dev.ap });
  if (u === '/api/chart') return json(dev.chart);
  if (u === '/qa-boardcast' && req.method === 'POST') {
    const vals = [7, 8, 9, 6, 7, 8];
    dev.chart = {
      seq: dev.chart.seq + 1, valid: true, epoch: Date.now(), dayIdx: 31,
      lines: vals.map(v => ({ yang: v === 7 || v === 9, moving: v === 6 || v === 9, value: v }))
    };
    dev.view = 1;
    return json({ ok: true, seq: dev.chart.seq });
  }
  if (u === '/api/ap' && req.method === 'POST') return body((b) => { try { const on = JSON.parse(b).on; if (dev.mode === 'run') dev.ap = !!on; } catch (e) {} json({ ok: true }); });
  if (u === '/api/view' && req.method === 'POST') return body((b) => { try { const v = JSON.parse(b).v; if (v >= 0 && v <= 2) dev.view = v; } catch (e) {} json({ ok: true }); });
  if (u === '/api/reconfig' && req.method === 'POST') { dev.mode = 'setup'; dev.view = 2; dev.prov = 'idle'; dev.ap = false; return json({ ok: true }); }
  if (u === '/scan') return json([
    { ssid: 'TP-LINK_5G', rssi: -45, open: false },
    { ssid: 'Xiaomi_Home', rssi: -62, open: false },
    { ssid: 'CMCC-2.4G', rssi: -71, open: false },
    { ssid: 'CoffeeShop-Free', rssi: -80, open: true },
    { ssid: 'HUAWEI-B315', rssi: -88, open: false }
  ]);
  if (u === '/connect' && req.method === 'POST') return body((b) => {
    let d = {}; try { d = JSON.parse(b); } catch (e) {}
    if (!d.ssid) return json({ ok: false, reason: '请输入网络名称' });
    dev.ssid = d.ssid; dev.prov = 'connecting'; provAt = Date.now();
    json({ ok: true });
  });
  if (u === '/status') {
    if (dev.prov === 'connecting' && Date.now() - provAt > 3000) {
      if (dev.ssid === 'bad') { dev.prov = 'fail'; dev.reason = '密码错误或认证失败'; }
      else { dev.prov = 'ok'; dev.ip = '192.168.1.99'; }
    }
    if (dev.prov === 'ok' && Date.now() - provAt > 6500) { dev.mode = 'run'; dev.view = 0; dev.prov = 'idle'; }
    return json({ state: dev.prov, ssid: dev.ssid, ip: dev.ip, reason: dev.reason, mac: 'A4:CB:8F:D6:08:A0' });
  }
  if (u === '/qa-reset' && req.method === 'POST') { dev.chart = { seq: 0, valid: false }; dev.view = 0; return json({ ok: true }); }
  if (u === '/qa-mobile') {
    const q = new URL(req.url, 'http://x').searchParams;
    const w = +q.get('w') || 409, h = +q.get('h') || 860;
    const theme = q.get('theme') || '', src = q.get('src') || '/', cast = q.get('cast') === '1', reset = q.get('reset') === '1';
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
    return res.end('<!doctype html><meta charset="utf-8"><body style="margin:0;background:#000">' +
      '<script>try{if(' + JSON.stringify(theme) + ')localStorage.setItem("gz_theme",' + JSON.stringify(theme) + ');}catch(e){}' +
      (reset ? 'fetch("/qa-reset",{method:"POST"}).then(function(){' + (cast ? 'fetch("/qa-boardcast",{method:"POST"}).then(go)' : 'go()') + '});' : (cast ? 'fetch("/qa-boardcast",{method:"POST"}).then(go);' : 'go();')) +
      'function go(){var f=document.createElement("iframe");f.src=' + JSON.stringify(src) + ';f.style.cssText="width:' + w + 'px;height:' + h + 'px;border:0;display:block";document.body.appendChild(f);}</script>');
  }
  let file = u === '/' ? (dev.mode === 'setup' ? '/setup.html' : '/index.html') : u;
  if (u === '/index.html' || u === '/setup.html') file = u;
  const p = path.join(WWW, file);
  if (!p.startsWith(WWW) || !fs.existsSync(p) || !fs.statSync(p).isFile()) {
    res.writeHead(302, { Location: 'http://localhost:8090/' }); return res.end();
  }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(p)] || 'application/octet-stream', 'Cache-Control': 'no-cache' });
  fs.createReadStream(p).pipe(res);
});
server.listen(8090, () => console.log('mock on http://localhost:8090'));