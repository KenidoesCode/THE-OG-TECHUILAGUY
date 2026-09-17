# ADR 0003: OGLang's inline-assembly boundary (v1)

**Status:** Accepted (describes the implemented state as of this
writing). This is explicitly a first version — see "What this is not"
below.

## Context

Reaching hardware (MMIO, port I/O, privileged instructions, CPU
feature detection) eventually requires OGLang to emit raw machine
instructions the language itself has no syntax for. This ADR records
the smallest version of that escape hatch that is honestly real and
testable today, and explains why it stops well short of a full
GCC-style inline-asm feature with operand constraints.

`volatile` and atomics are the other two items usually grouped with
inline assembly in a systems language's low-level toolkit. Neither is
implemented yet, and deliberately so: OGLang's optimizer performs no
instruction reordering, no dead-store elimination, and no common
subexpression elimination of any kind — every load and store the
front end emits is generated exactly as written, in program order. A
`volatile` qualifier changes nothing observable in a compiler that
never reorders or eliminates memory accesses in the first place, so
adding the keyword today would be exactly the kind of "syntax that
implies a guarantee the implementation doesn't back up" this project's
own rules exist to prevent. The same reasoning applies to atomics
absent any concurrency model to make atomicity meaningful. Both remain
explicitly PLANNED, not started, gated on the optimizer/concurrency
work that would make them non-vacuous.

## Decision

`asm("template")` is a new expression form. It takes exactly one
string literal — the first use of a string literal anywhere in
OGLang, added solely to carry this template text; there is no general
string type or any other use of string literals in the language. At
run time, the raw template text is emitted verbatim into the generated
assembly, and the expression's value is whatever ends up in `%eax`
immediately afterward — the same register convention `Call` already
uses for a function's return value, so nothing new was introduced at
the ABI level.

**v1 has no operand binding whatsoever.** There is no way to pass an
OGLang value *into* the template (no `"=r"(x)`/`"r"(y)`-style GCC
constraint syntax), and exactly one implicit output (`%eax`, read
unconditionally). This is a real, working boundary for emitting raw
instructions and observing a single resulting register's value — not
yet a general mechanism for two-way data flow between OGLang and hand-
written assembly.

### Why this is still useful and testable

Even output-only, single-register inline asm is enough to prove the
mechanism actually works end to end: a template can compute anything
expressible without depending on OGLang-managed operands (an immediate
constant, a `cpuid`/`rdtsc`-style instruction whose result lands in a
known register, etc.) and hand the result back into ordinary OGLang
control flow and arithmetic. `tests/programs/inline_asm.og` and
`inline_asm_register_pressure.og` both compile, link, and run as real
native binaries whose exit codes depend on the asm-computed value
composed with ordinary OGLang arithmetic — this is a genuine, checked
round trip, not a syntax-only exercise.

### Register safety

Arbitrary raw assembly could clobber any register the compiler doesn't
know to protect. Rather than trying to parse or restrict the template
text, `InlineAsmI32`'s codegen (`codegen/x86_64.cpp`) treats it exactly
like `Call`: every pool register holding a value that's still needed
after the asm block is pushed before it runs and popped afterward,
using the identical `liveAcross` mechanism `Call` and `idivl` already
relied on. This is the same fully-conservative approach `Call` takes
toward a callee it also can't inspect — no new safety mechanism was
invented, an existing one was reused.
`tests/programs/inline_asm_register_pressure.og` and
`testAsmCodegenEmitsTemplateVerbatimAndPreservesLiveValues` (in
`tests/unit_tests.cpp`) both specifically exercise this: several live
locals (enough to include both register-allocated and spilled values)
must survive an intervening asm block unchanged.

## What this is not

- **Not operand binding.** No inputs, no multiple outputs, no
  GCC-style constraint strings. A template can only communicate a
  result back to OGLang through the single fixed `%eax` convention.
- **Not `volatile` or atomics.** See "Context" above for why both
  remain intentionally unimplemented rather than added as
  currently-vacuous syntax.
- **Not MMIO or a documented freestanding-compilation target.** This
  ADR is a necessary building block toward those (you cannot write an
  `outb`/`inb` wrapper, or anything hardware-facing, without *some*
  form of raw instruction emission), not the arrival at either.
  OGLang still only targets a hosted Linux x86-64 ELF process; nothing
  about this feature changes that.
- **Not validated or sandboxed in any way.** The template text is
  opaque to the type checker (`checkExpr(AsmExpr)` always returns
  `"i32"` without inspecting the text at all) and to the parser beyond
  "it's a string literal." A malformed or malicious template is
  exactly as unsafe as it would be in any systems language's inline
  asm — this is an explicitly unsafe escape hatch, not a checked one.

## Tested invariants

- The lexer tokenizes a double-quoted string literal (with `\"`/`\\`
  escapes) and rejects an unterminated one as a lex error
  (`testLexerTokenizesStringLiteral`,
  `testLexerRejectsUnterminatedStringLiteral`).
- `asm("...")` parses as `AsmExpr`, recording the template text
  verbatim (`testParserParsesAsmExpr`).
- An `asm()` expression type-checks as `i32` and composes with
  ordinary arithmetic (`testTypeCheckerTreatsAsmExprAsI32`).
- The template text is emitted verbatim into the generated assembly,
  and a live-across-asm register value is genuinely saved and restored
  around it, not merely assumed safe
  (`testAsmCodegenEmitsTemplateVerbatimAndPreservesLiveValues`).
- Two full pipeline (source → running native binary) tests confirm the
  actual computed result is correct both under light and heavy
  register pressure (`tests/programs/inline_asm.og`,
  `inline_asm_register_pressure.og`).

## Consequences

Every claim about OGLang elsewhere in this repository must describe
inline assembly using the scope recorded here: a real, tested,
output-only, single-register boundary — not general operand binding,
not `volatile`, not atomics, not MMIO, not a freestanding target. This
ADR is the single source of truth for that distinction until a future
ADR supersedes it.
