import { useMemo } from "react";
import { FAMILIES, VERBS } from "@/lib/iki/catalog";
import type { ThinkResult } from "@/lib/iki/think";

type Cell = {
  id: string;
  label: string;
  rank: number;
  hot: boolean;
  kind: "verb" | "fam" | "meta" | "low";
  parent?: string;
};

const META: { rank: number; id: string; label: string }[] = [
  { rank: 9, id: "m:remainder", label: "remainder" },
  { rank: 9, id: "m:need-ctx", label: "need-context" },
  { rank: 8, id: "m:useful", label: "useful" },
  { rank: 8, id: "m:drop", label: "drop-goal" },
  { rank: 7, id: "m:gate", label: "gate" },
  { rank: 7, id: "m:right-task", label: "right-task" },
  { rank: 7, id: "m:stuck", label: "stuck" },
];

function lowerOf(famId: string): Cell[] {
  return [
    { id: `g:${famId}`, label: `${famId}·genus`, rank: 3, hot: true, kind: "low", parent: `f:${famId}` },
    { id: `μ:${famId}`, label: `${famId}·micro`, rank: 2, hot: true, kind: "low", parent: `g:${famId}` },
    { id: `r:${famId}`, label: `${famId}·region`, rank: 1, hot: true, kind: "low", parent: `μ:${famId}` },
    { id: `s:${famId}`, label: "surface", rank: 0, hot: true, kind: "low", parent: `r:${famId}` },
  ];
}

export function NeuronsMatrix({
  result,
  picked,
  onPick,
}: {
  result: ThinkResult;
  picked: string | null;
  onPick: (id: string | null) => void;
}) {
  const hotV = new Set(result.hotVerbs);
  const hotF = new Set(result.hotFams);
  const selectedFam = picked?.startsWith("f:") ? picked.slice(2) : null;

  const { rows, pos, links } = useMemo(() => {
    const cells: Cell[] = [];
    META.forEach((m) => {
      const hot =
        (m.id.includes("remainder") && result.gap) ||
        (m.id.includes("need") && result.gap) ||
        (m.id.includes("stuck") && result.meters.desperation > 40) ||
        (m.id.includes("gate") && hotV.has("theorize")) ||
        (m.id.includes("useful") && !result.gap) ||
        (m.id.includes("right") && hotV.has("occam"));
      cells.push({ id: m.id, label: m.label, rank: m.rank, hot, kind: "meta" });
    });
    VERBS.filter((v) => v.r6).forEach((v) =>
      cells.push({ id: `v:${v.id}`, label: v.id, rank: 6, hot: hotV.has(v.id), kind: "verb" }),
    );
    VERBS.filter((v) => !v.r6).forEach((v) =>
      cells.push({ id: `v:${v.id}`, label: v.id, rank: 5, hot: hotV.has(v.id), kind: "verb" }),
    );
    FAMILIES.filter((f) => hotF.has(f.id)).forEach((f) =>
      cells.push({
        id: `f:${f.id}`,
        label: f.id,
        rank: 4,
        hot: true,
        kind: "fam",
        parent: `v:${f.parent}`,
      }),
    );
    if (selectedFam) cells.push(...lowerOf(selectedFam));

    const by = new Map<number, Cell[]>();
    for (let r = 9; r >= 0; r--) by.set(r, []);
    cells.forEach((c) => by.get(c.rank)?.push(c));
    const rows = [...by.entries()].filter(([, list]) => list.length);

    const pos = new Map<string, { x: number; y: number }>();
    rows.forEach(([rank, list], ri) => {
      const y = 28 + ri * (470 / Math.max(rows.length, 1));
      list.forEach((c, i) => pos.set(c.id, { x: 64 + i * 28, y }));
    });

    const links: { a: string; b: string; hot: boolean }[] = [];
    const add = (a?: string, b?: string, hot = false) => {
      if (a && b && pos.has(a) && pos.has(b) && a !== b) links.push({ a, b, hot });
    };

    cells.forEach((c) => {
      if (c.parent) add(c.parent, c.id, c.hot);
    });
    FAMILIES.filter((f) => hotF.has(f.id)).forEach((f) => add(`v:${f.parent}`, `f:${f.id}`, true));
    const r6 = cells.filter((c) => c.rank === 6 && c.hot);
    const r5hot = cells.filter((c) => c.rank === 5 && c.hot);
    r6.forEach((s) => r5hot.forEach((v) => add(s.id, v.id, true)));
    const r7 = cells.filter((c) => c.rank === 7);
    const r8 = cells.filter((c) => c.rank === 8);
    const r9 = cells.filter((c) => c.rank === 9);
    r9.forEach((a) => r8.forEach((b) => add(a.id, b.id, a.hot || b.hot)));
    r8.forEach((a) => r7.forEach((b) => add(a.id, b.id, a.hot || b.hot)));
    r7.filter((c) => c.hot).forEach((a) => r6.forEach((b) => add(a.id, b.id, true)));
    result.edges.forEach((e) => add(e.from, e.to, true));

    return { rows, pos, links };
  }, [hotF, hotV, result.edges, result.gap, result.meters.desperation, selectedFam]);

  return (
    <svg
      viewBox="0 0 920 520"
      className="h-full w-full cursor-cross bg-black phos-flicker"
      style={{ background: "#000000" }}
      role="img"
      aria-label="Neuron matrix, read only"
      onClick={() => onPick(null)}
    >
      <defs>
        <filter id="phos-dot" x="-120%" y="-120%" width="340%" height="340%">
          <feGaussianBlur stdDeviation="2.8" result="b" />
          <feMerge>
            <feMergeNode in="b" />
            <feMergeNode in="b" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>
      <rect width="920" height="520" fill="#000000" />
      {links.map((l, i) => {
        const a = pos.get(l.a);
        const b = pos.get(l.b);
        if (!a || !b) return null;
        const hi = picked && (l.a === picked || l.b === picked);
        return (
          <line
            key={i}
            x1={a.x}
            y1={a.y}
            x2={b.x}
            y2={b.y}
            stroke={hi ? "#ffe066" : l.hot ? "#c47a00" : "#883400"}
            strokeWidth={hi ? 1.4 : 0.7}
            opacity={hi ? 0.95 : 0.55}
            filter="url(#phos-dot)"
          />
        );
      })}
      {rows.map(([rank, list]) => (
        <g key={rank}>
          <text
            x="10"
            y={(pos.get(list[0].id)?.y ?? 0) + 3}
            fill="#ffcc66"
            fontSize="10"
            fontFamily="var(--font-mono)"
            style={{ filter: "url(#phos-dot)" }}
          >
            R{rank}
          </text>
          {list.map((c) => {
            const p = pos.get(c.id);
            if (!p) return null;
            const on = picked === c.id;
            const r = on ? 6 : c.hot ? 4.2 : 2.6;
            return (
              <g
                key={c.id}
                transform={`translate(${p.x},${p.y})`}
                className="cursor-cross"
                onClick={(ev) => {
                  ev.stopPropagation();
                  onPick(c.id);
                }}
              >
                {on ? (
                  <rect
                    x={-r - 5}
                    y={-r - 5}
                    width={(r + 5) * 2}
                    height={(r + 5) * 2}
                    fill="none"
                    stroke="#ffe066"
                    strokeWidth={1.2}
                    filter="url(#phos-dot)"
                  />
                ) : null}
                <circle r={r + (c.hot ? 3 : 0)} fill={c.hot ? "#ffb00033" : "none"} />
                <circle r={r} className={c.hot || on ? "dot-hot" : "dot-idle"} />
                {on || c.hot ? (
                  <text
                    y={16}
                    textAnchor="middle"
                    fill="#ffcc66"
                    fontSize="7"
                    fontFamily="var(--font-mono)"
                  >
                    {c.label.slice(0, 10)}
                  </text>
                ) : null}
              </g>
            );
          })}
        </g>
      ))}
    </svg>
  );
}
