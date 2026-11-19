# Desire notes — 2026-11-19

What was asked, kept as stated.

Ikitaku is solid for something we can invoke under the correct context.
It would require the model to only invoke it when comparing several
options or hypotheses.

Conversion of English: the thought shapes ikitaku thinks in are
structures composed of the base set of ideas. English simply represents
those ideas, so it is not impossible to convert. The difficulty is
decomposing English ideas into the more complex ones. A way of doing
this more efficiently is to fill out several basic concepts as English
mappings into the equations, but this is dubious at best. That is a
subtask to test.

"Itaku speaks" is dubious. There are several language models on the
machine. A local test suite comes later; the machine is under full
control.

Installed via ollama:

- qwen2.5vl:latest
- deepseek-r1:8b
- qwen2.5-coder:7b
- digitsflow/bonsai-8b:latest
- qwen2.5:7b

A newer DeepSeek or Qwen is acceptable. A larger model from Hugging
Face is fine if it overflows into system RAM. 28 GB is available.

Expand the ikitaku memory sector to map normal words to concepts, as a
pre-existing formatting. Use a dictionary dataset of verbs and nouns so
the layer 4 mapping can be done on the machine, which has free compute.
Layer 4 is a simple composition working down from concepts and their
relations.

A weight viewer is wanted, so the hierarchies and links can be seen
with keyword search. The look is the obsidian-galaxy force graph:
https://github.com/agentage/obsidian-galaxy

Split the AI tool ikitaku into its own repo. Then create a new one
based on that galaxy view, to revamp the ikitaku viewer into something
an AI or a human can use to build the brain: Toplethos neuron reference
tracking in both 3D and 2D, and tracking the programming and how the
nodes link, instead of the usual mess.

This way layer 4 is automated by using layer 5 compositions in various
configurations.

Even a crude test works. A language model with no special instructions,
asked what "cycle+add" means with different signs, produced:

- +cycle +add — compounding spiral
- +cycle -add — depleting / pruning loop
- -cycle +add — backpropagation / rewind and patch
- -cycle -add — rollback / unwinding
- cycle xor add — modular addition, ring buffer

That shows a model can make layer 4 concepts easily.

Ikitaku sees all concepts as an algebraic differential polynomial.
Instead of plugging numbers into x^2 + 3xy - y^2, you plug in a
function y and its derivatives. Abstracting a = y, b = y', c = y''
leaves an ordinary multivariable expression. The space is
non-commutative, which is why concepts are like that.

Cycle-add is different from add-cycle. Cycle-add means loop n { var++ }.
Add-cycle means the add is outside, or the loop is not that add.

## Follow-on (same session)

The index page should show the English word above the chunk of data
arrangement, and a list in text assembled at startup by index that is
the file. Ikitaku galaxy must require ikitaku engine, so the engine
holds the actual memory file that galaxy is viewing.
