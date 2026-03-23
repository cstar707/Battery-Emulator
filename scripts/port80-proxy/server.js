require("dotenv").config({ path: require("path").join(__dirname, "..", ".env") });
// ─────────────────────────────────────────────────────────
//  server.js  –  Express server entry point
//
//  Serves:
//    /solis/*       → Proxy to solis-s6-app (3007): controls, settings, data, etc.
//    /api/*         → REST API (routes/api.js)
//    /*             → Static frontend files (frontend/)
// ─────────────────────────────────────────────────────────

const path    = require('path');
const http    = require('http');
const express = require('express');
const cors    = require('cors');
const config  = require('./config');
const apiRoutes = require('./routes/api');

require('./services/influxwriter');

const app = express();

// ── Proxy helper: forward /prefix/* → targetPort (strip prefix)
function proxyTo(prefix, targetPort, req, res) {
  const chunks = [];
  req.on('data', c => chunks.push(c));
  req.on('end', () => {
    const body = Buffer.concat(chunks);
    const targetPath = (!req.path || req.path === '/') ? '/' : req.path;
    const query = req.url.includes('?') ? '?' + req.url.split('?')[1] : '';
    const opts = {
      hostname: '127.0.0.1',
      port: targetPort,
      path: targetPath + query,
      method: req.method,
      headers: { ...req.headers, host: `127.0.0.1:${targetPort}` },
    };
    delete opts.headers['content-length'];
    if (body.length) opts.headers['content-length'] = String(body.length);
    const proxyReq = http.request(opts, (proxyRes) => {
      res.status(proxyRes.statusCode);
      Object.keys(proxyRes.headers).forEach(k => res.setHeader(k, proxyRes.headers[k]));
      proxyRes.pipe(res);
    });
    proxyReq.on('error', (e) => res.status(502).send(`Proxy to :${targetPort} failed: ` + e.message));
    if (body.length) proxyReq.write(body);
    proxyReq.end();
  });
}

// Proxy /solis/* → 3007 (with X-Forwarded-Prefix for base path)
app.use('/solis', (req, res) => {
  const chunks = [];
  req.on('data', c => chunks.push(c));
  req.on('end', () => {
    const body = Buffer.concat(chunks);
    const targetPath = (!req.path || req.path === '/') ? '/' : req.path;
    const query = req.url.includes('?') ? '?' + req.url.split('?')[1] : '';
    const opts = {
      hostname: '127.0.0.1',
      port: 3007,
      path: targetPath + query,
      method: req.method,
      headers: { ...req.headers, host: '127.0.0.1:3007', 'x-forwarded-prefix': '/solis' },
    };
    delete opts.headers['content-length'];
    if (body.length) opts.headers['content-length'] = String(body.length);
    const proxyReq = http.request(opts, (proxyRes) => {
      res.status(proxyRes.statusCode);
      Object.keys(proxyRes.headers).forEach(k => res.setHeader(k, proxyRes.headers[k]));
      proxyRes.pipe(res);
    });
    proxyReq.on('error', (e) => res.status(502).send('Proxy to solis-s6-app failed: ' + e.message));
    if (body.length) proxyReq.write(body);
    proxyReq.end();
  });
});

// Grafana, Envoy, SolarK, Legacy — proxy so everything works on port 80
app.use('/grafana', (req, res) => proxyTo('/grafana', 3000, req, res));
app.use('/envoy-debug', (req, res) => proxyTo('/envoy-debug', 3004, req, res));
app.use('/solark-support', (req, res) => proxyTo('/solark-support', 3002, req, res));
app.use('/legacy', (req, res) => proxyTo('/legacy', 3001, req, res));
app.use('/battery-dashboard', (req, res) => proxyTo('/battery-dashboard', 3008, req, res));

app.use(cors());
app.use(express.json());

// Proxy /api/envoy/* → 3004 (Envoy debug dashboard's API; page at /envoy-debug/ uses fetch('/api/envoy/...'))
app.use('/api/envoy', (req, res) => {
  const chunks = [];
  req.on('data', c => chunks.push(c));
  req.on('end', () => {
    const body = Buffer.concat(chunks);
    const [pathPart] = (req.originalUrl || req.url).split('?');
    const targetPath = pathPart || '/api/envoy';
    const query = req.url.includes('?') ? '?' + req.url.split('?')[1] : '';
    const opts = {
      hostname: '127.0.0.1',
      port: 3004,
      path: targetPath + query,
      method: req.method,
      headers: { ...req.headers, host: '127.0.0.1:3004' },
    };
    delete opts.headers['content-length'];
    if (body.length) opts.headers['content-length'] = String(body.length);
    const proxyReq = http.request(opts, (proxyRes) => {
      res.status(proxyRes.statusCode);
      Object.keys(proxyRes.headers).forEach(k => res.setHeader(k, proxyRes.headers[k]));
      proxyRes.pipe(res);
    });
    proxyReq.on('error', (e) => res.status(502).send('Proxy to Envoy :3004 failed: ' + e.message));
    if (body.length) proxyReq.write(body);
    proxyReq.end();
  });
});

app.use('/api', apiRoutes);

const frontendPath = path.join(__dirname, '..', 'frontend');
app.use(express.static(frontendPath));

app.get('*', (req, res) => {
  res.sendFile(path.join(frontendPath, 'index.html'));
});

app.listen(config.server.port, '0.0.0.0', () => {
  console.log(`────────────────────────────────────────`);
  console.log(`  Battery Dashboard`);
  console.log(`  http://0.0.0.0:${config.server.port}`);
  console.log(`  /solis/* → 3007 | /grafana/* → 3000 | /envoy-debug/* → 3004`);
  console.log(`  /solark-support/* → 3002 | /legacy/* → 3001 | /battery-dashboard/* → 3008`);
  console.log(`────────────────────────────────────────`);
});
