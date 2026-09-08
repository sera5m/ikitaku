import type { Meters as M } from "@/lib/iki/think";

const ROWS: { key: keyof M; label: string }[] = [
  { key: "hunger", label: "hunger" },
  { key: "confusion", label: "confusion" },
  { key: "novelty", label: "novelty" },
  { key: "desperation", label: "desperation" },
  { key: "boredom", label: "boredom" },
  { key: "patience", label: "patience" },
];

const CELLS = 20;

export function Meters({ m }: { m: M }) {
  return (
    <ul className="grid gap-3">
      {ROWS.map((r) => {
        const filled = Math.round((Math.min(100, m[r.key]) / 100) * CELLS);
        return (
          <li key={r.key} className="grid grid-cols-[7rem_1fr_2.2rem] items-center gap-2 text-xs">
            <span className="lcd-glow font-mono text-muted">{r.label}</span>
            <span className="flex gap-px" aria-hidden>
              {Array.from({ length: CELLS }, (_, i) => (
                <span
                  key={i}
                  className={i < filled ? "lcd-bar h-2 w-1.5" : "h-2 w-1.5"}
                  style={{
                    background: i < filled ? "var(--color-accent)" : "var(--color-dim)",
                    boxShadow: i < filled ? "0 0 6px #ffb000aa" : "none",
                  }}
                />
              ))}
            </span>
            <span className="lcd-glow text-right font-mono text-fg tabular-nums">{m[r.key]}</span>
          </li>
        );
      })}
    </ul>
  );
}
