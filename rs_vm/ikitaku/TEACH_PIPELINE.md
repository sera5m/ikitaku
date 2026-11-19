# How to teach iki (without translating English)

English → concept *is* a data transform. It is laborious **once per branch**,
not once per word. That labor is inherent: a word is an R4/R3 **alias**; a
Wikipedia page is a pile of `attr=val` on one species. Iki should not ingest
prose. A human (or later a separate translator) emits compact rows.

The pyramid is why this gets cheaper: every new fact is `c` on an existing F.

```
cost(first R5)     = huge   (lock a platonic)
cost(first R4)     = large  (name a family)
cost(first R3)     = medium (first species + sheet)
cost(Nth sibling)  ≈ 1 delta   (orange-cat = cat + color)
cost(Nth alias)    ≈ 1 bind    ("feline" → cat)
```

Storage is shared at the top. 10¹ species do not store 10¹ copies of `info`.

---

## Do not

- Parse English inside iki
- Add a verb per noun
- Copy an encyclopedia into a chain
- Average blueprint vs atoms because both are “the ship”

## Do

1. **Alias bind** — word → existing subject. `cat` is not a concept.
2. **Need context** — if the parent node is missing, stop and ask. That *is*
   the encyclopedia-shaped hole.
3. **Teach delta** — child inherits parent r5/r4/facts; only write attrs that
   differ. Pyramid compression.
4. **Conf interval** — every row is a posteriori with lo/hi. Synthetic a
   priori stays at the verb layer and stays challengeable.
5. **One page = many rows** on one subject, not a paragraph.

Worked cost:

| teach | stored |
|-------|--------|
| atom sheet (15 attrs) | 15 on `info\|atom` |
| gas / liquid / solid | 4 deltas each, inherit the 15 |
| water | 1 delta `phase=liquid` |
| orange cat | cat + `color=orange` |
| “kitty” | alias → cat |

The 4000-word “cat” article is mostly unused c (world_is_superset). Pull the
rows you need. Leave the rest outside the session.

---

## Who does the English

Iki does not. The transform English→row is **outside**: this chat, a later
plugin, a human. Iki’s job is map, inherit, challenge, ask.

If a sentence arrives raw: `need_context` / incomplete priori. Do not guess a
new R5 from a noun.

---

## Exponential claim (precise)

Not magic compression of English. Shared **structure**:

- R5 locked once
- R4 reused by every species under it
- R3 is a pointer + delta
- aliases are a dictionary, not copies of the sheet

So bytes per *new* real-world token fall as the tree fills. First 15 atom
attrs were expensive. The 16th is one line. The 100th animal is mostly
`living+path+energy-loss` already sitting on creature.
