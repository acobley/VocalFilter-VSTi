// VocalFilter additions to the shared R2Synth drawing kit (common.js).
// The response curves are computed, not drawn: RBJ constant-0 dB-peak
// bandpasses at 48 kHz, summed as complex values + - + exactly as the DSP does.

const FS = 48000;
const FC = [C.amber, C.blue, C.violet];           // F1, F2, F3
const VOWELS = ['Aaaa', 'Eeee', 'Iiii', 'Oooo', 'Uuuu'];
const IPA = ['ɑ', 'i', 'ɪ', 'o', 'u'];
const WORD = ['father', 'beet', 'bit', 'boat', 'boot'];
// [F1,F2,F3], [B1,B2,B3], [A1,A2,A3] dB
const MALE = [
  [[730, 1090, 2440], [80, 90, 120], [0, -3.3, -26.8]],
  [[270, 2290, 3010], [50, 100, 140], [0, -8.0, -2.3]],
  [[390, 1990, 2550], [60, 100, 130], [0, -8.2, -7.6]],
  [[450, 900, 2400], [60, 90, 120], [0, -7.4, -30.0]],
  [[300, 870, 2240], [50, 90, 120], [0, -10.2, -30.0]]
];
const FEMALE = [
  [[850, 1220, 2810], [95, 100, 140], [0, -3.3, -26.8]],
  [[310, 2790, 3310], [55, 120, 155], [0, -8.0, -2.3]],
  [[430, 2480, 3070], [65, 125, 155], [0, -8.2, -7.6]],
  [[500, 995, 2810], [65, 100, 140], [0, -7.4, -30.0]],
  [[370, 950, 2670], [60, 100, 145], [0, -10.2, -30.0]]
];
const FMIN = [200, 500, 1500], FMAX = [1200, 3000, 4000];
const fracF = (k, f) => (f - FMIN[k]) / (FMAX[k] - FMIN[k]);
const fracB = b => (b - 20) / 380;
const fracL = l => (l + 40) / 52;
const hz = f => f >= 1000 ? `${(f / 1000).toFixed(2)}k` : `${Math.round(f)} Hz`;
const db = v => `${v > 0 ? '+' : v < 0 ? '−' : ''}${Math.abs(v).toFixed(1)} dB`;

function bq(f0, bw, f) {
  const w0 = 2 * Math.PI * f0 / FS, Q = f0 / bw, al = Math.sin(w0) / (2 * Q);
  const a0 = 1 + al, a1 = -2 * Math.cos(w0), a2 = 1 - al;
  const w = 2 * Math.PI * f / FS, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
  // N = al (1 - z^-2), D = a0 + a1 z^-1 + a2 z^-2
  const nr = al * (1 - c2), ni = al * s2;
  const dr = a0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
  const m = dr * dr + di * di;
  return [(nr * dr + ni * di) / m, (ni * dr - nr * di) / m];
}
// Returns dB of each branch and of the + - + sum.
function resp(v, f) {
  const [F, B, A] = v, sg = [1, -1, 1];
  let sr = 0, si = 0; const br = [];
  for (let k = 0; k < 3; k++) {
    const g = Math.pow(10, A[k] / 20), [r, i] = bq(F[k], B[k], f);
    sr += sg[k] * g * r; si += sg[k] * g * i;
    br.push(20 * Math.log10(Math.max(1e-6, g * Math.hypot(r, i))));
  }
  return { br, sum: 20 * Math.log10(Math.max(1e-6, Math.hypot(sr, si))) };
}

// The response display. o: fLo, fHi, dbLo, dbHi, branches, handles, sel, label, right
function respScreen(x, y, w, h, v, o = {}) {
  const fLo = o.fLo || 80, fHi = o.fHi || 8000, dLo = o.dbLo ?? -48, dHi = o.dbHi ?? 6;
  screen(x, y, w, h, { nx: o.nx || 10, ny: o.ny || 6 });
  const fx = f => x + 6 + Math.log(f / fLo) / Math.log(fHi / fLo) * (w - 12);
  const top = y + (o.topPad ?? 24), bot = y + h - (o.botPad ?? 18);
  const fy = d => top + (dHi - Math.max(dLo, Math.min(dHi, d))) / (dHi - dLo) * (bot - top);
  const N = 260, pts = [[], [], [], []];
  for (let i = 0; i <= N; i++) {
    const f = fLo * Math.pow(fHi / fLo, i / N), r = resp(v, f);
    for (let k = 0; k < 3; k++) pts[k].push([fx(f), fy(r.br[k])]);
    pts[3].push([fx(f), fy(r.sum)]);
  }
  const dd = p => 'M' + p.map(q => `${f1(q[0])},${f1(q[1])}`).join(' L');
  if (o.branches !== false)
    for (let k = 0; k < 3; k++) path(dd(pts[k]), hexA(FC[k], o.sel === k ? 0.9 : 0.55), o.sel === k ? 1.6 : 1.1);
  const s = dd(pts[3]);
  path(`${s} L${f1(fx(fHi))},${bot} L${f1(fx(fLo))},${bot} Z`, 'none', 0, hexA(C.text, 0.06));
  path(s, C.text, 2.2);
  if (o.handles) for (let k = 0; k < 3; k++) {
    const X = fx(v[0][k]), Y = fy(resp(v, v[0][k]).br[k]);
    // width bracket at the -3 dB points of the branch
    const lo = v[0][k] - v[1][k] / 2, hi = v[0][k] + v[1][k] / 2, yb = Y + 9;
    line(f1(fx(lo)), yb, f1(fx(hi)), yb, FC[k], 2);
    line(f1(fx(lo)), yb - 4, f1(fx(lo)), yb + 4, FC[k], 1.5); line(f1(fx(hi)), yb - 4, f1(fx(hi)), yb + 4, FC[k], 1.5);
    line(f1(X), f1(Y), f1(X), bot, hexA(FC[k], 0.3), 1, 'stroke-dasharray="2 3"');
    circle(f1(X), f1(Y), o.sel === k ? 8 : 6.5, o.sel === k ? FC[k] : C.screen, FC[k], 2);
    if (o.tags) {
      const tw = 92, tx = Math.min(x + w - tw - 6, Math.max(x + 6, X - tw / 2)), ty = Y - 34;
      rect(f1(tx), f1(ty), tw, 20, hexA(FC[k], 0.14), hexA(FC[k], 0.6), 5);
      text(f1(tx + tw / 2), f1(ty + 14), `F${k + 1}  ${hz(v[0][k])}`, { size: 10, font: MONO, anchor: 'middle', fill: FC[k] });
    }
  }
  if (o.label) text(x + 10, y + 16, o.label, { size: 10, font: MONO, fill: C.muted });
  if (o.right) text(x + w - 10, y + 16, o.right, { size: 10, font: MONO, fill: C.muted, anchor: 'end' });
  if (o.axis !== false) {
    [100, 300, 1000, 3000].forEach(f => text(f1(fx(f)), y + h - 5, f >= 1000 ? `${f / 1000}k` : f, { size: 9, font: MONO, fill: C.dim, anchor: 'middle' }));
  }
  return { fx, fy };
}

function vfHeader(engine, W = 1200) {
  rect(0, 0, W, 56, C.header);
  line(0, 56, W, 56, C.border, 1);
  text(24, 37, 'VOCAL', { size: 25, weight: 800, fill: C.text, ls: 1 });
  text(118, 37, 'FILTER', { size: 25, weight: 800, fill: C.amber, ls: 1 });
  line(228, 16, 228, 40, C.border, 1);
  text(246, 33, engine, { size: 12, fill: C.subtle });
  // bypass pill, R2Synth's evaluation-pill position
  rect(W - 144, 14, 124, 28, C.seg, C.border, 14);
  lamp(W - 124, 28, false, C.mint, 4.5);
  text(W - 110, 32.5, 'BYPASS  off', { size: 11, weight: 700, fill: C.muted, ls: 0.4 });
}

// A vowel button: IPA glyph big, button name and example word under it.
function vowelBtn(x, y, w, h, i, active, o = {}) {
  rect(x, y, w, h, active ? C.amber : C.seg, active ? C.amber : C.border, 8);
  const ink = active ? '#16171a' : C.text, sub = active ? '#3a2a10' : C.subtle;
  text(x + w / 2, y + h * 0.5, IPA[i], { size: o.big || 22, weight: 700, anchor: 'middle', fill: ink, font: 'DejaVu Sans, sans-serif' });
  text(x + w / 2, y + h - (o.compact ? 8 : 22), VOWELS[i], { size: 10.5, weight: 700, anchor: 'middle', fill: ink });
  if (!o.compact) text(x + w / 2, y + h - 8, WORD[i], { size: 9.5, anchor: 'middle', fill: sub });
}

// A horizontal value bar, R2Synth's oscillator-row proportions.
function hbar(x, y, w, frac, col, label, val) {
  text(x, y + 4, label, { size: 9.5, weight: 700, fill: C.title, ls: 0.3 });
  const bx = x + 56, bw = w - 56 - 70;
  rect(bx, y - 3, bw, 8, C.track, null, 4);
  rect(bx, y - 3, Math.max(8, bw * frac), 8, col, null, 4);
  rect(bx + bw * frac - 5, y - 7, 10, 16, C.cap, C.capEdge, 3);
  text(x + w, y + 4, val, { size: 10, font: MONO, anchor: 'end', fill: C.text });
}

function renderAt(W, H) {
  document.body.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${out.join('')}</svg>`;
}
