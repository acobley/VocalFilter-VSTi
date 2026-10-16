// Shared drawing kit for the ForTran design mockups - R2Synth's palette and parts.
const C = {
  ground:'#141518', header:'#1a1c20', border:'#2a2d33', section:'#1c1e22', rule:'#24262b',
  track:'#2e3137', cap:'#2a2c31', capEdge:'#3c4047', text:'#eceae4', title:'#c9ccd2',
  muted:'#9ca1aa', subtle:'#7e838c', dim:'#4a4e56', screen:'#0f1012', screenEdge:'#26292e',
  grid:'#1d2024', seg:'#121316',
  amber:'#f2a33a', blue:'#4da3f0', mint:'#4fd1a1', violet:'#a77bf3', lampOn:'#ff8a3d', lampOff:'#3a2a20'
};
const SANS = 'Inter, Helvetica Neue, sans-serif';
const MONO = 'DejaVu Sans Mono, monospace';
const out = [];
const add = s => out.push(s);
const f1 = n => Math.round(n * 100) / 100;

function rect(x, y, w, h, fill, stroke, r = 0, sw = 1, extra = '') {
  add(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${r}" fill="${fill}" ${stroke ? `stroke="${stroke}" stroke-width="${sw}"` : ''} ${extra}/>`);
}
function line(x1, y1, x2, y2, stroke, sw = 1, extra = '') {
  add(`<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${stroke}" stroke-width="${sw}" ${extra}/>`);
}
function circle(cx, cy, r, fill, stroke, sw = 1, extra = '') {
  add(`<circle cx="${cx}" cy="${cy}" r="${r}" fill="${fill}" ${stroke ? `stroke="${stroke}" stroke-width="${sw}"` : ''} ${extra}/>`);
}
function text(x, y, s, o = {}) {
  const { size = 11, fill = C.text, weight = 400, anchor = 'start', font = SANS, ls = 0, opacity = 1 } = o;
  add(`<text x="${x}" y="${y}" font-family="${font}" font-size="${size}" font-weight="${weight}" fill="${fill}" text-anchor="${anchor}" letter-spacing="${ls}" opacity="${opacity}">${s}</text>`);
}
function path(d, stroke, sw = 1.5, fill = 'none', extra = '') {
  add(`<path d="${d}" stroke="${stroke}" stroke-width="${sw}" fill="${fill}" stroke-linejoin="round" stroke-linecap="round" ${extra}/>`);
}
function pt(cx, cy, r, a) { const t = a * Math.PI / 180; return [cx + r * Math.sin(t), cy - r * Math.cos(t)]; }
function arc(cx, cy, r, a0, a1) {
  if (a1 < a0) [a0, a1] = [a1, a0];
  const [x0, y0] = pt(cx, cy, r, a0), [x1, y1] = pt(cx, cy, r, a1);
  return `M${f1(x0)},${f1(y0)} A${r},${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${f1(x1)},${f1(y1)}`;
}
function hexA(hex, a) {
  const n = parseInt(hex.slice(1), 16);
  return `rgba(${n >> 16},${(n >> 8) & 255},${n & 255},${a})`;
}

// A knob: value arc in the section colour, cap, pointer, reading and label under it.
function knob(cx, cy, r, frac, col, val, label, o = {}) {
  const w = Math.max(3, r * 0.17);
  const ang = -135 + 270 * frac;
  path(arc(cx, cy, r, -135, 135), C.track, w);
  if (o.bipolar) { if (Math.abs(frac - 0.5) > 0.005) path(arc(cx, cy, r, 0, ang), col, w); }
  else if (frac > 0.001) path(arc(cx, cy, r, -135, ang), col, w);
  if (o.bipolar) { const [dx, dy] = pt(cx, cy, r + w + 1, 0); circle(dx, dy, 1.6, col); }
  circle(cx, cy, r * 0.68, C.cap, C.capEdge, 1);
  const [ax, ay] = pt(cx, cy, r * 0.18, ang), [bx, by] = pt(cx, cy, r * 0.58, ang);
  line(f1(ax), f1(ay), f1(bx), f1(by), C.text, r > 14 ? 2 : 1.5, 'stroke-linecap="round"');
  if (val !== null && val !== undefined)
    text(cx, cy + r + (o.vgap || 13), val, { size: o.vs || 10, font: MONO, anchor: 'middle', fill: C.text });
  if (label)
    text(cx, cy + r + (o.vgap || 13) + (o.lgap || 13), label, { size: o.ls || 9.5, weight: 700, anchor: 'middle', fill: C.title, ls: 0.3 });
}

// A segment switch: the active position filled in the section colour.
function seg(x, y, w, h, items, active, col, o = {}) {
  rect(x, y, w, h, C.seg, C.border, h / 2.6);
  const iw = w / items.length;
  items.forEach((it, i) => {
    if (i === active) rect(x + i * iw + 2, y + 2, iw - 4, h - 4, col, null, (h - 4) / 2.6);
    text(x + i * iw + iw / 2, y + h / 2 + (o.size || 10.5) * 0.36, it,
      { size: o.size || 10.5, weight: 600, anchor: 'middle', fill: i === active ? '#16171a' : C.muted });
  });
}

function lamp(cx, cy, on, col = C.lampOn, r = 5) {
  if (on) circle(cx, cy, r + 3, hexA(col, 0.18));
  circle(cx, cy, r, on ? col : hexA(col, 0.16), on ? hexA(col, 0.9) : hexA(col, 0.45), 1);
}

function section(x, y, w, h, title, sub, col, o = {}) {
  rect(x, y, w, h, C.section, C.border, 10);
  circle(x + 20, y + 22, 4.5, col);
  text(x + 31, y + 27, title, { size: o.ts || 13, weight: 700, fill: C.text, ls: 0.4 });
  if (sub) text(x + 31 + (o.subx || title.length * 9.6 + 10), y + 27, sub, { size: 11.5, fill: C.subtle });
}
function subhead(x, y, s, col) { text(x, y, s, { size: 9.5, weight: 700, fill: col, ls: 0.6 }); }

function screen(x, y, w, h, o = {}) {
  rect(x, y, w, h, C.screen, C.screenEdge, 6);
  const nx = o.nx || 8, ny = o.ny || 4;
  for (let i = 1; i < nx; i++) line(f1(x + i * w / nx), y + 1, f1(x + i * w / nx), y + h - 1, C.grid, 1);
  for (let j = 1; j < ny; j++) line(x + 1, f1(y + j * h / ny), x + w - 1, f1(y + j * h / ny), C.grid, 1);
}
function chevron(x, y) { path(`M${x - 4},${y - 9} L${x + 4},${y} L${x - 4},${y + 9}`, C.dim, 2); }

// ADSR curve inside a screen, as R2Synth draws it. Times in seconds.
function adsrPath(x, y, w, h, A, D, S, R, total, keyUp, lvl = 1) {
  const sx = t => x + 6 + (t / total) * (w - 12), sy = v => y + h - 6 - v * lvl * (h - 14);
  const pts = [];
  const N = 90;
  for (let i = 0; i <= N; i++) {
    const t = total * i / N; let v;
    if (t < A) v = t / A;
    else if (t < keyUp) v = S + (1 - S) * Math.exp(-6.9 * (t - A) / D);
    else { const vk = keyUp < A ? keyUp / A : S + (1 - S) * Math.exp(-6.9 * (keyUp - A) / D); v = vk * Math.exp(-6.9 * (t - keyUp) / R); }
    pts.push([sx(t), sy(v)]);
  }
  return { d: 'M' + pts.map(p => `${f1(p[0])},${f1(p[1])}`).join(' L'), base: sy(0), sx, sy, x0: sx(0), x1: sx(total) };
}
function envScreen(x, y, w, h, col, A, D, S, R, o = {}) {
  screen(x, y, w, h, o);
  const total = o.total || 1.4, keyUp = o.keyUp || 0.85;
  const p = adsrPath(x, y, w, h, A, D, S, R, total, keyUp, o.lvl || 1);
  path(`${p.d} L${f1(p.x1)},${f1(p.base)} L${f1(p.x0)},${f1(p.base)} Z`, 'none', 0, hexA(col, 0.16));
  path(p.d, col, 1.8);
  line(f1(p.sx(keyUp)), y + 4, f1(p.sx(keyUp)), y + h - 4, C.dim, 1, 'stroke-dasharray="3 3"');
  if (o.label) text(x + 8, y + 15, o.label, { size: 9.5, font: MONO, fill: C.muted });
  if (o.right) text(x + w - 8, y + 15, o.right, { size: 9.5, font: MONO, fill: C.muted, anchor: 'end' });
  return p;
}

// The spectral filter's response: LP on, comb on Add, Grass off.
function filterMag(f, fc = 2400) {
  const lp = 1 / Math.sqrt(1 + Math.pow(f / fc, 8));
  const comb = 1 + 0.18 * Math.cos(2 * Math.PI * Math.log2(f / 110) * 0.8);
  return Math.min(1.25, lp * comb);
}
function filterScreen(x, y, w, h, o = {}) {
  screen(x, y, w, h, { nx: 10, ny: 4 });
  const fx = f => x + 4 + (Math.log(f / 20) / Math.log(1000)) * (w - 8);
  const fy = m => y + h - 10 - (m / 1.3) * (h - 34);
  const pts = [];
  for (let i = 0; i <= 160; i++) { const f = 20 * Math.pow(1000, i / 160); pts.push([fx(f), fy(filterMag(f))]); }
  const d = 'M' + pts.map(p => `${f1(p[0])},${f1(p[1])}`).join(' L');
  path(`${d} L${x + w - 4},${y + h - 2} L${x + 4},${y + h - 2} Z`, 'none', 0, hexA(C.blue, 0.14));
  path(d, C.blue, 1.8);
  if (o.dots) {
    const f0 = o.f0 || 220;
    for (let k = 1; k <= 10; k++) {
      const f = f0 * k, X = fx(f), Y = fy(filterMag(f));
      line(f1(X), f1(Y), f1(X), y + h - 3, hexA(C.amber, 0.35), 1);
      circle(f1(X), f1(Y), k === o.sel ? 4.5 : 3.2, k === o.sel ? C.amber : C.screen, C.amber, 1.4);
    }
  }
  if (o.label) text(x + 8, y + 15, o.label, { size: 9.5, font: MONO, fill: C.muted });
  if (o.right) text(x + w - 8, y + 15, o.right, { size: 9.5, font: MONO, fill: C.muted, anchor: 'end' });
  if (o.axis) {
    text(x + 8, y + h - 7, '20 Hz', { size: 9, font: MONO, fill: C.blue });
    text(x + w - 8, y + h - 7, '20 kHz', { size: 9, font: MONO, fill: C.muted, anchor: 'end' });
  }
}

// The four shape switches - LP and HP are on/off, Comb and Grass Off/Add/Mult/Max.
function shapeSwitches(x, y, w, gap = 28, h = 20) {
  const half = (w - 10) / 2;
  text(x, y + 14, 'LP', { size: 9.5, weight: 700, fill: C.title });
  seg(x + 24, y, half - 24, h, ['Off', 'On'], 1, C.blue, { size: 9.5 });
  text(x + half + 10, y + 14, 'HP', { size: 9.5, weight: 700, fill: C.title });
  seg(x + half + 34, y, half - 24, h, ['Off', 'On'], 0, C.blue, { size: 9.5 });
  text(x, y + gap + 14, 'COMB', { size: 9.5, weight: 700, fill: C.title });
  seg(x + 38, y + gap, half - 38, h, ['Off', 'Add', 'Mult', 'Max'], 1, C.blue, { size: 8.5 });
  text(x + half + 10, y + gap + 14, 'GRASS', { size: 9.5, weight: 700, fill: C.title });
  seg(x + half + 52, y + gap, half - 42, h, ['Off', 'Add', 'Mult', 'Max'], 0, C.blue, { size: 8.5 });
}

function header(engine) {
  rect(0, 0, 1200, 56, C.header);
  line(0, 56, 1200, 56, C.border, 1);
  text(24, 37, 'FOR', { size: 25, weight: 800, fill: C.text, ls: 1 });
  text(80, 37, 'TRAN', { size: 25, weight: 800, fill: C.amber, ls: 1 });
  line(170, 16, 170, 40, C.border, 1);
  text(188, 33, engine, { size: 12, fill: C.subtle });
  // Voice meter: sixteen lamps, the sounding voices lit.
  text(912, 33, 'VOICES', { size: 9.5, weight: 700, fill: C.subtle, ls: 0.6 });
  for (let i = 0; i < 16; i++) {
    const on = [0, 1, 2, 3].includes(i);
    rect(966 + i * 13, 22, 9, 14, on ? C.mint : C.track, null, 2, 1, on ? '' : '');
  }
}

// Partial levels, phases etc. for the demo patch - one patch across all three concepts.
const P = {
  level: [0.92, 0.70, 0.64, 0.58, 0.52, 0.47, 0.42, 0.38, 0.34, 0.30],
  phase: [0.5, 0.5, 0.62, 0.5, 0.35, 0.5, 0.5, 0.7, 0.5, 0.5],
  inv:   [0, 0, 0, 0, 1, 0, 0, 0, 1, 0],
  env:   [1, 1, 1, 1, 1, 1, 1, 0, 0, 0],
  envInv:[0, 0, 0, 0, 0, 0, 1, 0, 0, 0],
  A: [0.01, 0.012, 0.012, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08],
  D: [0.9, 0.6, 0.24, 0.30, 0.25, 0.20, 0.18, 0.16, 0.14, 0.12],
  S: [0.8, 0.6, 0.45, 0.40, 0.32, 0.25, 0.20, 0.16, 0.12, 0.10],
  R: [0.5, 0.45, 0.38, 0.32, 0.28, 0.24, 0.20, 0.18, 0.16, 0.14],
  envLvl: [1, 0.9, 0.8, 0.8, 0.7, 0.7, 0.6, 0.6, 0.5, 0.5],
  lfoRate: [0, 0, 0.35, 0, 0.5, 0, 0.62, 0, 0, 0],
  lfoDepth:[0, 0, 0.3, 0, 0.22, 0, 0.4, 0, 0, 0]
};
const ms = s => s >= 1 ? `${s.toFixed(2)} s` : `${Math.round(s * 1000)} ms`;
const pc = v => `${Math.round(v * 100)} %`;

function render() {
  document.body.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="640" viewBox="0 0 1200 640">${out.join('')}</svg>`;
}
