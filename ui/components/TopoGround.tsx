/** Angular topography: straight segments, headings in 15° steps, no 90° corners. */

function field(x: number, y: number) {
  return (
    Math.sin(x * 0.008 + y * 0.003) * 0.55 +
    Math.cos(y * 0.011 - x * 0.005) * 0.4 +
    Math.sin((x + y) * 0.004) * 0.28
  );
}

const HEADINGS: [number, number][] = [];
for (let a = 15; a < 360; a += 15) {
  if (a % 90 === 0) continue;
  const r = (a * Math.PI) / 180;
  HEADINGS.push([Math.cos(r), Math.sin(r)]);
}

function walk(x0: number, y0: number, w: number, h: number, sign: number) {
  const pts: string[] = [];
  let x = x0;
  let y = y0;
  const step = 28;
  for (let i = 0; i < 70; i++) {
    if (x < -40 || y < -40 || x > w + 40 || y > h + 40) break;
    pts.push(`${x.toFixed(1)},${y.toFixed(1)}`);
    const gx = field(x + 4, y) - field(x - 4, y);
    const gy = field(x, y + 4) - field(x, y - 4);
    const tx = -gy * sign;
    const ty = gx * sign;
    let best = HEADINGS[0];
    let score = -Infinity;
    for (const d of HEADINGS) {
      const s = d[0] * tx + d[1] * ty;
      if (s > score) {
        score = s;
        best = d;
      }
    }
    x += best[0] * step;
    y += best[1] * step;
  }
  return pts.length > 2 ? `M ${pts[0]} L ${pts.slice(1).join(" ")}` : "";
}

const SEEDS = [
  [80, 80],
  [400, 120],
  [900, 90],
  [1300, 160],
  [200, 400],
  [700, 380],
  [1100, 420],
  [1500, 500],
  [120, 700],
  [550, 720],
  [980, 680],
  [1400, 740],
  [300, 250],
  [800, 560],
  [1250, 300],
];

const PATHS = SEEDS.flatMap(([x, y], i) => {
  const a = walk(x, y, 1600, 900, 1);
  const b = i % 2 === 0 ? walk(x + 18, y + 22, 1600, 900, -1) : "";
  return [a, b].filter(Boolean);
});

export function TopoGround() {
  return (
    <svg
      className="pointer-events-none fixed inset-0 h-full w-full"
      viewBox="0 0 1600 900"
      preserveAspectRatio="xMidYMid slice"
      aria-hidden
    >
      <defs>
        <filter id="phos-topo" x="-30%" y="-30%" width="160%" height="160%">
          <feGaussianBlur stdDeviation="2.2" result="b" />
          <feMerge>
            <feMergeNode in="b" />
            <feMergeNode in="b" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>
      <rect width="1600" height="900" fill="#000000" />
      {PATHS.map((d, i) => (
        <path
          key={i}
          d={d}
          fill="none"
          stroke="#ffb000"
          strokeWidth={i % 3 === 0 ? 1.3 : 0.65}
          strokeLinejoin="miter"
          strokeMiterlimit={8}
          opacity={0.22 + (i % 5) * 0.06}
          filter="url(#phos-topo)"
        />
      ))}
    </svg>
  );
}
