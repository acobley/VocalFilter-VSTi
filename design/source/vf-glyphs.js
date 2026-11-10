// VocalFilter's additions to the 70s sci-fi lettering (rbfont.js).
//
// The face is capitals only, but the vowel chart needs the IPA symbols, and
// /i/ and /ɪ/ differ only in case. So five lower-case glyphs are drawn in
// the same idiom (x-height 2..6 on the 4 x 6 grid, rounded-square bowls),
// used only by SVG <text data-keep="1">. Plus four punctuation marks the
// panel's captions use.
Object.assign(RB, {
  '←': [4.4, 'M4.4,3 L0,3 M1.6,1.4 L0,3 L1.6,4.6'],
  '…': [4, 'M0,6 L0,6.01 M2,6 L2,6.01 M4,6 L4,6.01'],
  '○': [2.6, 'M1.3,1.7 Q2.6,1.7 2.6,3 Q2.6,4.3 1.3,4.3 Q0,4.3 0,3 Q0,1.7 1.3,1.7 Z'],
  ';': [0.4, 'M0.4,2 L0.4,2.01 M0.4,5.5 L0,6.8'],
});
const RBX = {
  'i': [0, 'M0,0.4 L0,0.41 M0,2.4 L0,6'],
  'ɪ': [2.4, 'M0,2 L2.4,2 M1.2,2 L1.2,6 M0,6 L2.4,6'],
  'o': [3.6, 'M1.2,2 L2.4,2 Q3.6,2 3.6,3.2 L3.6,4.8 Q3.6,6 2.4,6 L1.2,6 Q0,6 0,4.8 L0,3.2 Q0,2 1.2,2 Z'],
  'u': [3.6, 'M0,2 L0,4.6 Q0,6 1.5,6 L2.2,6 Q3.6,6 3.6,4.6 M3.6,2 L3.6,6'],
  'ɑ': [3.6, 'M3.6,2 L3.6,6 M3.6,4.6 Q3.6,6 2.2,6 L1.4,6 Q0,6 0,4.6 L0,3.4 Q0,2 1.4,2 L2.2,2 Q3.6,2 3.6,3.4'],
};

// Same as rbPaths, but case is kept and RBX is consulted first.
function rbPathsKeep(str, cap, w, track) {
  const s = cap / 6; let x = 0, d = [];
  for (const ch of str) {
    if (ch === ' ') { x += RB_SPACE; continue; }
    const g = RBX[ch] || RB[ch.toUpperCase()] || RB['·'];
    d.push(`<path transform="translate(${(x + w / 2) * s},${w / 2 * s}) scale(${s})" d="${g[1]}"/>`);
    x += g[0] + w + track;
  }
  return { paths: d.join(''), width: Math.max(0, (x - track)) * s, height: cap + w * s };
}

function rbConvertKeep() {
  document.querySelectorAll('svg text[data-keep]').forEach(t => {
    const fs = +t.getAttribute('font-size'), cap = fs * 0.64, w = 1.25;
    const p = rbPathsKeep(t.textContent, cap, w, 1.05);
    const x = +t.getAttribute('x'), y = +t.getAttribute('y');
    const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    g.innerHTML = `<g transform="translate(${x - p.width / 2},${y - cap})" fill="none" stroke="${t.getAttribute('fill')}" stroke-width="${w}" stroke-linecap="round" stroke-linejoin="round">${p.paths}</g>`;
    t.replaceWith(g);
  });
}
