# Expression bags & F(x)+c — theory check (2026-08-26)

## Your logic (compressed)

1. Work = transform of information under a rule set (lambda-like).
2. Platonic **F** = form tip. **x** = data. **c** = context (specificity).
3. Derivative = strip toward F / open interior (macro→micro focus).
4. Anti-derivative = wrap +c / close as unit (micro→macro focus).
5. Nesting stacks: F_a(F_b(x)+c1)+c2 — multi-slot expression, not one layer name.
6. Subject is always a **bag**; may hold verb + descriptor.

## Consistency

| Claim | Verdict |
|-------|---------|
| Transforms-all-the-way-down | **Consistent** with ladder + priors |
| F platonic, +c specificity | **Consistent** |
| Open/close = micro/macro focus | **Consistent** and useful |
| Nested F as multi-var “equation” | **Metaphor OK**; not literal calculus — treat as expression tree |
| Tree “down” = more specific | **Consistent** if root=platonic, leaves=+c |
| Our code FLOW_DOWN = abstract | **Matches derivative toward F** (not tree-leaf direction). Vocabulary must stay explicit: *derive/abstract* vs *tree-depth* |
| Subject = bag with verb | **Consistent**; was under-specified in code |

## Reasoning gaps (yours + ours)

1. **Direction words** — “down the tree” (more specific) ≠ our `FLOW_DOWN` (more abstract). Same physics, opposite labels. Fix: prefer *derive / anti-derive* in metacog, not only up/down.
2. **Per-layer thinking** — real bug: iki walked one form name; nested interiors were invisible. Open/close bags fix that.
3. **“Multi-variable equations”** — overclaim if taken literally. Nested bags + unity/disunity slots are the right structure; Lie/Hegel still apply between slots.
4. **Subject-verb-descriptor** — theory sound; needs subject-as-bag API (done this pass).

## Plan applied

1. `ikitaku_expr_bag` / tree: form F, data x, context c, child, open flag.
2. `expr_open` = derive into interior; `expr_close` / `expr_wrap` = anti-derive.
3. `expr_reason` = layer_flow on F while reporting nest state.
4. `subject_as_bag` = subject holds verb + descriptor.
5. Keep fuzzy Hegel / Kant / Lie on bag slots later (unity field reserved).
