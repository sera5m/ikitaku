/** Locked R5/R4 from ikitaku catalogs. Read-only. */

export type Verb = {
  id: string;
  form: string;
  locked: boolean;
  r6: boolean;
  note: string;
};

export type Family = {
  parent: string;
  id: string;
  form: string;
  note: string;
};

export const VERBS: Verb[] = [
  { id: "info", form: "a value/structure exists", locked: true, r6: false, note: "struct var const" },
  { id: "name", form: "bind identity", locked: true, r6: false, note: "let x" },
  { id: "copy", form: "duplicate without change", locked: true, r6: false, note: "a = b" },
  { id: "map", form: "one to one, shape kept", locked: true, r6: false, note: "map-math or map-table" },
  { id: "reduce", form: "many to one", locked: true, r6: false, note: "sum fold" },
  { id: "generate", form: "one to many", locked: true, r6: false, note: "create is alias" },
  { id: "filter", form: "many to fewer, same type", locked: true, r6: false, note: "not a decision" },
  { id: "split", form: "one bag to two", locked: true, r6: false, note: "partition" },
  { id: "join", form: "two bags to one", locked: true, r6: false, note: "zip concat" },
  { id: "order", form: "rearrange by key", locked: true, r6: false, note: "sort is R3" },
  { id: "seek", form: "find witness", locked: true, r6: false, note: "scan-until" },
  { id: "repeat", form: "apply along an axis", locked: true, r6: false, note: "iterator or time" },
  { id: "sequence", form: "do T then U", locked: true, r6: false, note: "compose" },
  { id: "decision", form: "compare then take a path", locked: true, r6: false, note: "not a table" },
  { id: "wait", form: "defer on time axis", locked: true, r6: false, note: "T is a var" },
  { id: "invert", form: "undo a known transform", locked: true, r6: false, note: "peel" },
  { id: "bound", form: "obey a constraint", locked: true, r6: false, note: "clamp" },
  { id: "emit", form: "cross boundary out", locked: true, r6: false, note: "print" },
  { id: "absorb", form: "cross boundary in", locked: true, r6: false, note: "sensor" },
  { id: "destroy", form: "delete a structure", locked: true, r6: false, note: "invert of generate" },
  { id: "compare", form: "relation across values", locked: true, r6: false, note: "not a path choice" },
  { id: "theorize", form: "claim then score", locked: true, r6: true, note: "R6" },
  { id: "occam", form: "prefer shallower wrap", locked: true, r6: true, note: "R6" },
  { id: "hegelian", form: "determinate negation", locked: true, r6: true, note: "R6" },
];

export const FAMILIES: Family[] = [
  { parent: "map", id: "map-math", form: "y=f(x)", note: "closed form" },
  { parent: "map", id: "map-table", form: "y=table[x]", note: "not a branch" },
  { parent: "order", id: "compare-swap", form: "adjacent swap", note: "bubble" },
  { parent: "order", id: "divide-conquer", form: "split merge", note: "merge" },
  { parent: "order", id: "key-sort", form: "rearrange by key", note: "watch family" },
  { parent: "order", id: "partition-then-order", form: "split recurse join", note: "qsort concept" },
  { parent: "repeat", id: "iterator-axis", form: "advance i", note: "" },
  { parent: "repeat", id: "time-axis", form: "advance t", note: "wait loop" },
  { parent: "repeat", id: "counted", form: "fixed n", note: "do n" },
  { parent: "decision", id: "binary-cmp", form: "two-way path", note: "if" },
  { parent: "wait", id: "delay", form: "block then resume", note: "" },
  { parent: "invert", id: "peel", form: "T^-1", note: "" },
  { parent: "info", id: "array", form: "indexed bag", note: "" },
  { parent: "filter", id: "denoise", form: "drop residual", note: "" },
  { parent: "seek", id: "template", form: "short bag in long", note: "partial want" },
];

export const RANKS = [
  "R0 surface",
  "R1 region",
  "R2 micro",
  "R3 genus",
  "R4 family",
  "R5 verb",
  "R6 strategy",
  "R7 task",
  "R8 useful",
  "R9 remainder",
] as const;
