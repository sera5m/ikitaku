import { useMemo, useState } from "react";
import type { IkiEdge, IkiNode } from "@/lib/iki/think";

const W = 920;
const H = 520;

function glyph(rank: number, r: number) {
  if (rank >= 6) {
    return `M ${-r} 0 L 0 ${-r} L ${r} 0 L 0 ${r} Z`;
  }
  if (rank >= 5) {
    const s = r * 0.85;
    return `M ${-s} ${-s} L ${s} ${-s} L ${s} ${s} L ${-s} ${s} Z`;
  }
  if (rank >= 4) {
    const a = r;
    return `M ${-a} 0 L ${-a / 2} ${-a} L ${a / 2} ${-a} L ${a} 0 L ${a / 2} ${a} L ${-a / 2} ${a} Z`;
  }
  return `M 0 ${-r} L ${r * 0.7} ${r * 0.6} L ${-r * 0.7} ${r * 0.6} Z`;
}

export function GraphCanvas({
  nodes,
  edges,
  onPick,
  picked,
}: {
  nodes: IkiNode[];
  edges: IkiEdge[];
  onPick: (id: string | null) => void;
  picked: string | null;
}) {
  const layout = useMemo(() => {
    const byRank = new Map<number, IkiNode[]>();
    nodes.forEach((n) => {
      const a = byRank.get(n.rank) ?? [];
      a.push(n);
      byRank.set(n.rank, a);
    });
    const pos = new Map<string, { x: number; y: number }>();
    byRank.forEach((list, rank) => {
      list.forEach((n, i) => {
        const y = 36 + ((9 - rank) / 9) * (H - 72);
        const x = 80 + ((i + 1) / (list.length + 1)) * (W - 140);
        pos.set(n.id, { x, y });
      });
    });
    return pos;
  }, [nodes]);

  const [hover, setHover] = useState<string | null>(null);

  return (
    <svg
      viewBox={`0 0 ${W} ${H}`}
      className="h-full w-full cursor-cross phos-flicker"
      role="img"
      aria-label="Ikitaku thought graph, read only"
      onClick={() => onPick(null)}
    >
      <defs>
        <pattern id="lcd-dots" width="8" height="8" patternUnits="userSpaceOnUse">
          <circle cx="1" cy="1" r="0.6" fill="#ffb000" opacity="0.22" />
        </pattern>
        <filter id="phos-graph" x="-40%" y="-40%" width="180%" height="180%">
          <feGaussianBlur stdDeviation="1.1" result="b" />
          <feMerge>
            <feMergeNode in="b" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>
      <rect width={W} height={H} fill="#000000" />
      <rect width={W} height={H} fill="url(#lcd-dots)" />
      {edges.map((e, i) => {
        const a = layout.get(e.from);
        const b = layout.get(e.to);
        if (!a || !b) return null;
        const hi = picked && (e.from === picked || e.to === picked);
        const dash = e.kind === "assoc" ? "1 4" : e.kind === "trace" ? "6 3" : "2 3";
        return (
          <line
            key={i}
            x1={a.x}
            y1={a.y}
            x2={b.x}
            y2={b.y}
            stroke={hi ? "#ffe066" : "#8a5500"}
            strokeWidth={hi ? 1.4 : 0.8}
            strokeDasharray={dash}
            filter="url(#phos-graph)"
            opacity={e.kind === "assoc" ? 0.45 : 0.9}
          />
        );
      })}
      {nodes.map((n) => {
        const p = layout.get(n.id);
        if (!p) return null;
        const on = picked === n.id || hover === n.id;
        const r = on ? 14 : n.hot ? 11 : 8;
        return (
          <g
            key={n.id}
            transform={`translate(${p.x},${p.y})`}
            onClick={(ev) => {
              ev.stopPropagation();
              onPick(n.id);
            }}
            onMouseEnter={() => setHover(n.id)}
            onMouseLeave={() => setHover(null)}
            className="cursor-cross"
          >
            {picked === n.id ? (
              <rect
                x={-r - 6}
                y={-r - 6}
                width={(r + 6) * 2}
                height={(r + 6) * 2}
                fill="none"
                stroke="#ffe066"
                strokeWidth={1.2}
                filter="url(#phos-graph)"
              />
            ) : null}
            <path
              d={glyph(n.rank, r)}
              fill="none"
              stroke={n.hot || on ? "#ffb000" : "#4a3200"}
              strokeWidth={on ? 1.8 : 1.1}
              filter="url(#phos-graph)"
            />
            {n.hot ? (
              <path d={glyph(n.rank, r * 0.35)} fill="none" stroke="#ffc933" strokeWidth={0.8} />
            ) : null}
            <text
              y={r + 14}
              textAnchor="middle"
              fill="#ffcc66"
              fontSize={10}
              fontFamily="var(--font-mono)"
              className="lcd-glow"
            >
              {n.label}
            </text>
          </g>
        );
      })}
    </svg>
  );
}
