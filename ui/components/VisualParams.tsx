import type { CSSProperties } from "react";

export type Vis = {
  text: number;
  bloom: number;
  scan: number;
  vig: number;
};

export const VIS_DEFAULT: Vis = {
  text: 38,
  bloom: 42,
  scan: 52,
  vig: 78,
};

const KEY = "iki-vis";

export function loadVis(): Vis {
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return VIS_DEFAULT;
    const p = JSON.parse(raw) as Partial<Vis>;
    return {
      text: clamp(p.text ?? VIS_DEFAULT.text),
      bloom: clamp(p.bloom ?? VIS_DEFAULT.bloom),
      scan: clamp(p.scan ?? VIS_DEFAULT.scan),
      vig: clamp(p.vig ?? VIS_DEFAULT.vig),
    };
  } catch {
    return VIS_DEFAULT;
  }
}

export function saveVis(v: Vis) {
  try {
    localStorage.setItem(KEY, JSON.stringify(v));
  } catch {
    /* ignore */
  }
}

export function visStyle(v: Vis): CSSProperties {
  return {
    ["--glow-text" as string]: String(v.text / 100),
    ["--glow-screen" as string]: String(v.bloom / 100),
    ["--scan-op" as string]: String(v.scan / 100),
    ["--vig-op" as string]: String(v.vig / 100),
  };
}

function clamp(n: number) {
  if (Number.isNaN(n)) return 0;
  return Math.max(0, Math.min(100, Math.round(n)));
}

const ROWS: { key: keyof Vis; label: string }[] = [
  { key: "text", label: "text" },
  { key: "bloom", label: "crt" },
  { key: "scan", label: "scan" },
  { key: "vig", label: "vig" },
];

export function VisualParams({
  vis,
  onChange,
}: {
  vis: Vis;
  onChange: (v: Vis) => void;
}) {
  return (
    <div className="lcd-panel grid grid-cols-2 gap-x-4 gap-y-1 border-t border-border px-3 py-2 pr-24 md:grid-cols-4 md:px-6">
      {ROWS.map((r) => (
        <label key={r.key} className="grid grid-cols-[2.6rem_1fr_1.8rem] items-center gap-2">
          <span className="font-mono text-[10px] uppercase tracking-widest text-muted">
            {r.label}
          </span>
          <input
            type="range"
            min={0}
            max={100}
            value={vis[r.key]}
            aria-label={`${r.label} glow`}
            className="h-11 w-full accent-accent"
            onChange={(e) => onChange({ ...vis, [r.key]: Number(e.target.value) })}
          />
          <span className="text-right font-mono text-[10px] tabular-nums text-fg">
            {vis[r.key]}
          </span>
        </label>
      ))}
    </div>
  );
}
