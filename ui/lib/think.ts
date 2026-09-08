import { FAMILIES, VERBS, type Family, type Verb } from "./catalog";

export type IkiNode = {
  id: string;
  rank: number;
  label: string;
  sub: string;
  conf: number;
  hot: boolean;
  attrs: string[];
};

export type IkiEdge = {
  from: string;
  to: string;
  kind: "parent" | "assoc" | "cause" | "trace";
  w: number;
};

export type TraceStep = {
  t: number;
  lane: string;
  english: string;
  bag: string;
  conf: number;
};

export type Meters = {
  hunger: number;
  boredom: number;
  confusion: number;
  desperation: number;
  novelty: number;
  patience: number;
};

export type StimulusId = "vanish" | "move" | "even" | "qsort" | "sure";

export type ThinkResult = {
  nodes: IkiNode[];
  edges: IkiEdge[];
  trace: TraceStep[];
  meters: Meters;
  accepted: string;
  gap: boolean;
  who: string;
  hotVerbs: string[];
  hotFams: string[];
};

export const STIMULI: { id: StimulusId; title: string; blurb: string; pattern: string }[] = [
  {
    id: "vanish",
    title: "Tree gone, no who",
    blurb: "Count dropped. Persist prior vs empty agent. Wonder should open.",
    pattern: "count n-1\nid=tree1 absent\nwho=-\npersist=1",
  },
  {
    id: "move",
    title: "Us moved a tree",
    blurb: "Same id, new xy, who=us. Notice without vanish-gap. Mimic not wired.",
    pattern: "id=tree1\npos 1.00,5.00→2.60,5.00\nwho=us\nmoved",
  },
  {
    id: "even",
    title: "Mystery predicate",
    blurb: "I/O table 0→true 1→false …. Extract map-table / cycle.",
    pattern: "0 true\n1 false\n2 true\n5 false\n18 true\n36 true",
  },
  {
    id: "qsort",
    title: "Partition recurse",
    blurb: "Abstract to ORDER. Family partition-then-order. Repeat(decision).",
    pattern: "partition-then-order\nrepeat(decision)\nqsort",
  },
  {
    id: "sure",
    title: "Other mind: sure?",
    blurb: "Social activation. Furniture persist becomes persist=?. Same gap machine.",
    pattern: "us>iki | sure?\npersist=1",
  },
];

function hunger(conf: number) {
  if (conf < 15) return 10;
  if (conf > 92) return 8;
  return Math.max(8, 90 - Math.abs(conf - 62));
}

function jaccard(a: string[], b: string[]) {
  const A = new Set(a);
  const B = new Set(b);
  let inter = 0;
  A.forEach((x) => {
    if (B.has(x)) inter++;
  });
  const u = A.size + B.size - inter;
  return u ? inter / u : 0;
}

function verbNode(v: Verb, conf: number, hot: boolean): IkiNode {
  return {
    id: `v:${v.id}`,
    rank: v.r6 ? 6 : 5,
    label: v.id,
    sub: v.form,
    conf,
    hot,
    attrs: [v.id, v.r6 ? "strategy" : "verb", v.locked ? "locked" : "novel"],
  };
}

function famNode(f: Family, conf: number, hot: boolean): IkiNode {
  return {
    id: `f:${f.id}`,
    rank: 4,
    label: f.id,
    sub: f.form,
    conf,
    hot,
    attrs: [f.parent, f.id, "family"],
  };
}

export function classifySource(text: string): StimulusId | "free" {
  const t = text.toLowerCase();
  if (!t.trim()) return "free";
  if (t.includes("who=-") || t.includes("no who") || t.includes("absent") || t.includes("vanish"))
    return "vanish";
  if (t.includes("moved") || t.includes("who=us")) return "move";
  if ((t.includes("true") && t.includes("false")) || t.includes("even")) return "even";
  if (t.includes("qsort") || t.includes("partition") || t.includes("quicksort")) return "qsort";
  if (t.includes("sure?") || t.includes("persist=?")) return "sure";
  return "free";
}

export function think(stim: StimulusId, tick: number): ThinkResult {
  const nodes: IkiNode[] = [];
  const edges: IkiEdge[] = [];
  const trace: TraceStep[] = [];
  const push = (lane: string, english: string, bag: string, conf: number) => {
    trace.push({ t: trace.length, lane, english, bag, conf });
  };

  const hotVerbs = new Set<string>();
  const hotFams = new Set<string>();
  let who = "-";
  let gap = false;
  let accepted = "unknown";
  const meters: Meters = {
    hunger: 12,
    boredom: Math.min(100, tick * 4),
    confusion: 8,
    desperation: tick > 8 ? 55 : 18,
    novelty: 20,
    patience: 80,
  };

  push("R0", `intake stimulus ${stim}`, "surface", 90);

  if (stim === "vanish") {
    who = "-";
    gap = true;
    hotVerbs.add("seek");
    hotVerbs.add("theorize");
    hotVerbs.add("invert");
    hotFams.add("peel");
    meters.confusion = 72;
    meters.novelty = 64;
    meters.hunger = hunger(55);
    push("notice", "memory had tree1, now absent. count n-1. no agent tagged.", "count", 88);
    push("wonder", "persist ∧ who-empty → gap. not finished fact.", "persist", 80);
    push("R5", "abstract: DESTROY? or SEEK missing cause?", "destroy", 40);
    push("origin", "C = missing object. look for B (agent act). B empty.", "cause", 45);
    push("invert", "treat persist=1 as persist=?. same machine as vanish.", "persist=?", 70);
    push("experiment", "observe–wait–compare persist. orchard fruit can break count.", "experiment", 62);
    accepted = "persist-broke | unknown-agent";
  } else if (stim === "move") {
    who = "us";
    hotVerbs.add("map");
    hotVerbs.add("copy");
    hotVerbs.add("compare");
    hotFams.add("mov");
    meters.confusion = 22;
    meters.novelty = 48;
    meters.hunger = hunger(78);
    push("notice", "tree1 moved 1.00,5.00→2.60,5.00 who=us", "tree1.pos", 92);
    push("R5", "abstract: MAP of pose along space axis. same id → permanence holds.", "map", 84);
    push("deabstract", "+c: this tree, this agent, this translate.", "tree1", 80);
    push("wonder", "who tagged. persist not violated. gap=0.", "persist", 88);
    push("R6", "no mimic lane. self.can.translate is not a prior. fidget instead.", "can-act", 30);
    accepted = "agent-translate (other). self-act unknown";
  } else if (stim === "even") {
    hotVerbs.add("map");
    hotVerbs.add("compare");
    hotFams.add("map-table");
    meters.hunger = hunger(58);
    meters.novelty = 70;
    meters.confusion = 35;
    push("R0", "table: 0 T, 1 F, 2 T, 5 F, 18 T, 36 T …", "series", 90);
    push("schema", "map-table not map-math. bool emission on int.", "map-table", 74);
    push("assoc", "cycle + remainder ≈ modulo. evenness as 2-cycle.", "mod-2", 68);
    push("occam", "is_even / n%2==0 beats deeper wraps.", "occam", 82);
    accepted = "map-table: parity";
  } else if (stim === "qsort") {
    hotVerbs.add("order");
    hotVerbs.add("split");
    hotVerbs.add("join");
    hotVerbs.add("repeat");
    hotVerbs.add("decision");
    hotFams.add("partition-then-order");
    hotFams.add("divide-conquer");
    meters.hunger = hunger(60);
    meters.confusion = 28;
    push("R1", "watch heap bag monotone after run.", "region", 70);
    push("R5", "purpose ORDER (not English 'qsort').", "order", 86);
    push("R4", "split → recurse → join. alias qsort is costume.", "partition-then-order", 78);
    push("origin", "repeat(decision) for purpose order.", "cause", 72);
    accepted = "partition-then-order";
  } else {
    who = "us";
    gap = true;
    hotVerbs.add("theorize");
    hotVerbs.add("invert");
    hotVerbs.add("occam");
    hotFams.add("peel");
    meters.hunger = hunger(50);
    meters.confusion = 40;
    meters.novelty = 55;
    push("hear", "us>iki | sure? persist=1", "thought", 88);
    push("social", "furniture activated. persist=?", "persist=?", 80);
    push("wonder", "same gap machine as vanish (weld invert==vanish-gap).", "gap", 76);
    push("R6", "hegel: thesis persist, antithesis not-persist, hunt poles.", "hegelian", 64);
    accepted = "persist=? (social)";
  }

  const usedV = VERBS.filter((v) => hotVerbs.has(v.id) || v.id === "info" || v.id === "occam");
  usedV.forEach((v) => nodes.push(verbNode(v, hotVerbs.has(v.id) ? 82 : 40, hotVerbs.has(v.id))));

  FAMILIES.filter((f) => hotFams.has(f.id) || hotVerbs.has(f.parent)).forEach((f) => {
    nodes.push(famNode(f, hotFams.has(f.id) ? 78 : 36, hotFams.has(f.id)));
    edges.push({ from: `v:${f.parent}`, to: `f:${f.id}`, kind: "parent", w: 0.9 });
  });

  nodes.push({
    id: "s:surface",
    rank: 0,
    label: stim,
    sub: "intake",
    conf: 90,
    hot: true,
    attrs: ["surface", stim],
  });
  nodes.push({
    id: "s:trace-end",
    rank: 6,
    label: accepted.split(" ")[0],
    sub: "accepted form",
    conf: 70,
    hot: true,
    attrs: ["accepted"],
  });

  edges.push({
    from: "s:surface",
    to: nodes.find((n) => n.rank >= 5 && n.hot)?.id ?? "s:trace-end",
    kind: "trace",
    w: 1,
  });

  for (let i = 0; i < nodes.length; i++) {
    for (let j = i + 1; j < nodes.length; j++) {
      const s = jaccard(nodes[i].attrs, nodes[j].attrs);
      if (s > 0.25 && nodes[i].id !== nodes[j].id) {
        edges.push({ from: nodes[i].id, to: nodes[j].id, kind: "assoc", w: s });
      }
    }
  }

  if (gap) {
    edges.push({ from: "v:invert", to: "v:theorize", kind: "cause", w: 0.7 });
  }

  return { nodes, edges, trace, meters, accepted, gap, who, hotVerbs: [...hotVerbs], hotFams: [...hotFams] };
}

export function thinkSource(text: string, tick: number): ThinkResult {
  const id = classifySource(text);
  if (id !== "free") return think(id, tick);
  const nodes: IkiNode[] = [];
  const edges: IkiEdge[] = [];
  const snippet = text.trim().slice(0, 80) || "(empty bag)";
  const meters: Meters = {
    hunger: hunger(48),
    boredom: Math.min(100, tick * 4),
    confusion: text.trim() ? 32 : 12,
    desperation: tick > 8 ? 55 : 18,
    novelty: text.trim() ? 50 : 10,
    patience: 80,
  };
  const trace: TraceStep[] = [
    { t: 0, lane: "R0", english: "intake stimulus free", bag: "surface", conf: 70 },
    { t: 1, lane: "notice", english: snippet, bag: "editor", conf: 60 },
    { t: 2, lane: "R5", english: "no catalog hit. remainder. seek + occam.", bag: "unknown", conf: 40 },
  ];
  VERBS.filter((v) => v.id === "seek" || v.id === "occam" || v.id === "info").forEach((v) =>
    nodes.push(verbNode(v, v.id === "seek" ? 70 : 40, v.id === "seek")),
  );
  nodes.push({
    id: "s:surface",
    rank: 0,
    label: "free",
    sub: "intake",
    conf: 70,
    hot: true,
    attrs: ["surface", "free"],
  });
  nodes.push({
    id: "s:trace-end",
    rank: 9,
    label: "remainder",
    sub: "accepted form",
    conf: 35,
    hot: true,
    attrs: ["accepted"],
  });
  edges.push({ from: "s:surface", to: "v:seek", kind: "trace", w: 1 });
  return {
    nodes,
    edges,
    trace,
    meters,
    accepted: "R9 remainder",
    gap: true,
    who: "-",
    hotVerbs: ["seek", "occam", "info"],
    hotFams: [],
  };
}
