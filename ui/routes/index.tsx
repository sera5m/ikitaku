import { createFileRoute } from "@tanstack/react-router";
import { useEffect, useMemo, useRef, useState } from "react";
import { CrtOverlay } from "@/components/iki/CrtOverlay";
import { GraphCanvas } from "@/components/iki/GraphCanvas";
import { Meters } from "@/components/iki/Meters";
import { NeuronsMatrix } from "@/components/iki/NeuronsMatrix";
import { TopoGround } from "@/components/iki/TopoGround";
import { STIMULI, thinkSource } from "@/lib/iki/think";
import { RANKS } from "@/lib/iki/catalog";

export const Route = createFileRoute("/")({ component: Home });

function Home() {
  const [source, setSource] = useState("");
  const [tick, setTick] = useState(3);
  const [picked, setPicked] = useState<string | null>(null);
  const [fileName, setFileName] = useState<string | null>(null);
  const [chrome, setChrome] = useState(2);
  const [tab, setTab] = useState<"graph" | "neurons">("graph");
  const fileRef = useRef<HTMLInputElement>(null);
  const result = useMemo(() => thinkSource(source, tick), [source, tick]);
  const node = result.nodes.find((n) => n.id === picked);

  useEffect(() => {
    function onKey(e: KeyboardEvent) {
      const t = e.target as HTMLElement | null;
      if (t && (t.tagName === "TEXTAREA" || t.tagName === "INPUT")) return;
      if (e.key === "-" || e.key === "_") {
        e.preventDefault();
        setChrome((c) => Math.max(0, c - 1));
      }
      if (e.key === "+" || e.key === "=") {
        e.preventDefault();
        setChrome((c) => Math.min(2, c + 1));
      }
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  function onFile(list: FileList | null) {
    const f = list?.[0];
    if (!f) return;
    setFileName(f.name);
    f.text().then((t) => {
      setSource(t);
      setPicked(null);
    });
  }

  const showChrome = chrome >= 1;
  const showSides = chrome >= 2;

  return (
    <div className="relative min-h-dvh cursor-cross text-fg">
      <TopoGround />
      <CrtOverlay />
      <div className="relative z-10">
        {showChrome ? (
          <header className="lcd-panel border-b border-border px-4 py-2 md:px-6">
            <p className="lcd-glow font-mono text-xs tracking-widest text-accent uppercase">
              ikitaku · read-only
            </p>
            <h1 className="lcd-glow text-2xl font-semibold tracking-tight md:text-3xl">
              Thought graph
            </h1>
            <p className="lcd-glow mt-1 max-w-2xl text-sm leading-snug text-muted">
              Separate from the watch and from the VM. Bags, ranks, association. You
              watch the trace. You do not edit the cells.
            </p>
            <div className="mt-2 flex gap-2">
              <button
                type="button"
                className={`lcd-glow min-h-11 border px-3 py-2 font-mono text-xs ${
                  tab === "graph" ? "border-accent text-fg" : "border-border text-muted"
                }`}
                onClick={() => setTab("graph")}
              >
                thought graph
              </button>
              <button
                type="button"
                className={`lcd-glow min-h-11 border px-3 py-2 font-mono text-xs ${
                  tab === "neurons" ? "border-accent text-fg" : "border-border text-muted"
                }`}
                onClick={() => setTab("neurons")}
              >
                neurons
              </button>
            </div>
          </header>
        ) : null}

        <div
          className={`grid gap-0 ${showSides ? "lg:grid-cols-[16rem_1fr_18rem]" : ""}`}
        >
          {showSides ? (
            <aside className="lcd-panel border-b border-border px-3 py-2 lg:border-r lg:border-b-0">
              <h2 className="lcd-glow font-mono text-xs text-muted">stimulus</h2>
              <textarea
                value={source}
                onChange={(e) => {
                  setSource(e.target.value);
                  setFileName(null);
                }}
                spellCheck={false}
                placeholder="write a pattern bag…"
                className="mt-2 min-h-32 w-full resize-y border border-border bg-surface px-2 py-1.5 font-mono text-xs leading-snug text-fg outline-none focus:border-accent"
              />
              <div className="relative mt-2 flex flex-wrap gap-2">
                <button
                  type="button"
                  className="lcd-glow min-h-11 border border-border px-3 py-2 font-mono text-xs text-fg hover:border-accent"
                  onClick={() => fileRef.current?.click()}
                >
                  select input file
                </button>
                <input
                  ref={fileRef}
                  type="file"
                  className="absolute h-px w-px overflow-hidden opacity-0"
                  accept=".txt,.cat,.vul,.json,.md,.csv"
                  onChange={(e) => onFile(e.target.files)}
                />
              </div>
              {fileName ? (
                <p className="lcd-glow mt-2 font-mono text-xs text-muted">{fileName}</p>
              ) : null}
              <p className="lcd-glow mt-2 font-mono text-[10px] leading-snug text-muted">
                insert pattern
              </p>
              <ul className="mt-2 grid gap-1">
                {STIMULI.map((s) => (
                  <li key={s.id}>
                    <button
                      type="button"
                      title={s.blurb}
                      onClick={() => {
                        setSource(s.pattern);
                        setFileName(null);
                        setPicked(null);
                      }}
                      className="lcd-glow w-full min-h-11 border border-border px-2 py-2 text-left font-mono text-[11px] text-muted hover:border-accent hover:text-fg"
                    >
                      {s.title}
                    </button>
                  </li>
                ))}
              </ul>
              <label className="lcd-glow mt-3 block font-mono text-xs text-muted">
                idle ticks {tick}
                <input
                  type="range"
                  min={0}
                  max={20}
                  value={tick}
                  onChange={(e) => setTick(Number(e.target.value))}
                  className="mt-2 w-full accent-accent"
                />
              </label>
              <p className="lcd-glow mt-2 font-mono text-xs text-muted">
                gap {result.gap ? "yes" : "no"} · who {result.who}
              </p>
            </aside>
          ) : null}

          <section
            className={`min-h-[22rem] border-b border-border lg:border-b-0 ${
              tab === "neurons" ? "bg-black" : "lcd-panel"
            }`}
          >
            {showChrome ? (
              <div className="flex gap-1 overflow-x-auto border-b border-border px-3 py-1 font-mono text-[10px] text-muted">
                {RANKS.map((r, i) => (
                  <span
                    key={r}
                    className={i === 5 || i === 6 ? "lcd-glow text-accent" : "lcd-glow"}
                  >
                    {r}
                  </span>
                ))}
              </div>
            ) : null}
            <div className={showChrome ? "h-[32rem] bg-black md:h-[36rem]" : "h-dvh bg-black"}>
              {tab === "neurons" ? (
                <NeuronsMatrix result={result} picked={picked} onPick={setPicked} />
              ) : (
                <GraphCanvas
                  nodes={result.nodes}
                  edges={result.edges}
                  picked={picked}
                  onPick={setPicked}
                />
              )}
            </div>
          </section>

          {showSides ? (
            <aside className="lcd-panel space-y-3 px-3 py-2">
              <div>
                <h2 className="lcd-glow font-mono text-xs text-muted">meters</h2>
                <div className="mt-1">
                  <Meters m={result.meters} />
                </div>
              </div>
              <div>
                <h2 className="lcd-glow font-mono text-xs text-muted">accepted</h2>
                <p className="lcd-glow mt-1 text-sm">{result.accepted}</p>
              </div>
              <div>
                <h2 className="lcd-glow font-mono text-xs text-muted">inspect</h2>
                {node ? (
                  <dl className="lcd-glow mt-1 space-y-1 font-mono text-xs">
                    <div>id {node.id}</div>
                    <div>
                      R{node.rank} conf {node.conf}
                    </div>
                    <div className="text-muted">{node.sub}</div>
                    <div className="text-muted">{node.attrs.join(" · ")}</div>
                  </dl>
                ) : picked ? (
                  <p className="lcd-glow mt-2 font-mono text-xs">{picked}</p>
                ) : (
                  <p className="lcd-glow mt-2 text-sm text-muted">
                    Click a node. Weights stay locked.
                  </p>
                )}
              </div>
            </aside>
          ) : null}
        </div>

        {showChrome ? (
          <section className="lcd-panel border-t border-border px-4 py-2 md:px-6">
            <h2 className="lcd-glow font-mono text-xs text-muted">trace</h2>
            <ol className="mt-1 grid gap-0.5">
              {result.trace.map((s) => (
                <li
                  key={s.t}
                  className="grid grid-cols-[4.5rem_1fr] gap-3 font-mono text-xs md:grid-cols-[7rem_1fr]"
                >
                  <span className="lcd-glow text-accent">{s.lane}</span>
                  <span className="lcd-glow">
                    {s.english}{" "}
                    <span className="text-muted">
                      [{s.bag} {s.conf}]
                    </span>
                  </span>
                </li>
              ))}
            </ol>
          </section>
        ) : null}

        <div className="fixed bottom-3 right-3 z-40 flex gap-1">
          <button
            type="button"
            aria-label="hide ui"
            className="lcd-glow min-h-11 min-w-11 border border-border bg-surface font-mono text-lg text-fg"
            onClick={() => setChrome((c) => Math.max(0, c - 1))}
          >
            -
          </button>
          <button
            type="button"
            aria-label="show ui"
            className="lcd-glow min-h-11 min-w-11 border border-border bg-surface font-mono text-lg text-fg"
            onClick={() => setChrome((c) => Math.min(2, c + 1))}
          >
            +
          </button>
        </div>
      </div>
    </div>
  );
}
