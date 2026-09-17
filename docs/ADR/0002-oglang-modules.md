# ADR 0002: OGLang's module system (v1)

**Status:** Accepted (describes the implemented state as of this
writing). This is explicitly a first version — see "What this is not"
below for what a later version would need to add before this could be
called a complete module system.

## Context

OGLang previously compiled exactly one source file at a time; there
was no way to split a program across files, and no namespacing of any
kind. This ADR records the module system built to close that gap: what
syntax it introduces, what "importing a module" actually does and does
not do, and — since this project's own rule is that a feature is only
as complete as its tests and documentation say it is — exactly which
parts of a "real" module system (as the term is normally used) are and
are not implemented.

## Decision

**A module is one source file.** Its name is its filename without the
`.og` extension, assigned by the compiler driver — nothing declared
inside the file names the module. There is no directory hierarchy, no
package concept, and no way for two different files to be the same
module or for one file to be split across modules.

**`import other;`** at the top level of a file makes every one of
`other`'s top-level functions, structs, and enums reachable from this
file — but *only* through qualification: `other.function(...)`,
`let p: other.StructName;`, `other.EnumName.Variant`. There is no
unqualified access to anything from an imported module. This is the
central design choice, and it's what makes the rest of the system
simple: **because an imported symbol is never visible unqualified,
two modules exposing a function/struct/enum under the same name are
never ambiguous with each other** — there is nothing to disambiguate,
since you always write `a.thing` or `b.thing`, never bare `thing`, to
reach either one. A module's own declarations remain reachable
unqualified only within that same module.

**Qualified names reuse existing syntax rather than inventing new
syntax**, per this project's stated preference: `other.function(...)`
is parsed as an ordinary `CallExpr` whose `callee` string happens to
contain a dot; `other.StructName` is parsed by `parseType()`'s existing
identifier-as-type-name path, folded into one compound string; and
`other.EnumName.Variant` reuses the identical `FieldAccessExpr` node
struct field access already uses, with `structVarName` set to the
compound string `"other.EnumName"`. No new AST node was added for any
of this — see `03-compiler/oglang/parser/parser.cpp`'s `parsePrimary()`
and `parseType()`.

### Compiler driver

`ogc file.og` (a single file) behaves exactly as before — this code
path (`compileSingleFile` in `main.cpp`) is untouched by this feature
and has no module context at all; an `import` statement in a
single-file program is a compile error (`TypeChecker::check` rejects
any non-empty `program.imports` outright, since there's no second file
to resolve it against).

`ogc file1.og file2.og ...` (two or more files) enters
`compileModules()`, which:

1. Parses every file into its own `Program`, deriving each one's
   module name from its filename. Two files that would produce the
   same module name are rejected (`Duplicate module name`).
2. Resolves every `import` against the set of parsed modules; an
   import naming a file that wasn't passed on the command line is
   rejected (`Unknown module in import`).
3. Detects import cycles via DFS and rejects them outright — see
   "Cycles" below.
4. Computes a deterministic topological compilation order (dependencies
   before dependents, ties broken by command-line order) via Kahn's
   algorithm.
5. Collects each module's own declarations
   (`TypeChecker::collectModuleSymbols`, the exact same function
   `TypeChecker::check()` uses internally for a single file — there is
   one implementation of "what counts as a valid declaration," not a
   parallel one that could silently drift).
6. Type-checks every module (`TypeChecker::checkModule`) against its
   own declarations plus the *qualified* declarations of whatever it
   imports. Only the first file on the command line (the *entry
   module*) is required to declare `main`; every other module is a
   library and may or may not have one.
7. Once every module has checked successfully, rewrites (in place,
   post-type-check) every non-entry module's own function names and
   every qualified call site into safe assembly symbols — see
   "Symbol mangling" below — then lowers, allocates registers for, and
   generates code for every function across every module into **one**
   assembly file, exactly as the single-file path already did, and
   assembles + links that one file into one native binary.

### Symbol mangling

Type-checking works entirely with OGLang-level qualified names like
`"colors.brightness"` — a plain string used as a map key, nothing
more. Assembly labels are a different concern: a dot is not a
universally safe or portable choice of linker symbol character, so
`main.cpp`'s `mangleCallsInExpr`/`mangleCallsInStatement` walk each
module's AST after type-checking succeeds and rewrite:

- any qualified call (`"colors.brightness"`) to `"colors__brightness"`,
  regardless of which module the call site is in;
- for a **non-entry** module only, its own function declarations to
  `"itsModuleName__functionName"`, and every *unqualified* call site
  within that same module (which, by construction, can only refer to
  one of that module's own functions) to match.

The **entry module** is left completely unmangled: its own function
names and its own unqualified call sites are untouched, so a
single-module "entry point" always has a plain `main` label — exactly
the label `X86Codegen::generateEntryPoint("main")` already calls,
unchanged from the single-file path.

### Cycles

**v1 rejects import cycles outright, full stop** — `A` importing `B`
importing `A` (directly or through a longer chain) is a compile error
(`Circular module import detected: A -> B -> A`), never a supported
pattern. This isn't a gap so much as a consequence of the current
architecture: because the whole program is lowered and assembled as
one unit (see "What this is not" below), there is no structural need
for cycles — nothing here is analogous to a forward-declared header in
a system with genuinely separate compilation, where a cycle sometimes
has a legitimate resolution. Rejecting cycles outright avoids having to
define what "compilation order" would even mean inside one, and keeps
the mental model simple: import edges form a DAG, always.

### Visibility

There is no `pub`/private distinction. Every top-level function,
struct, and enum in a module is visible to every module that imports
it. A future version might add explicit visibility; v1 does not
attempt it — see "What this is not."

## What this is not

This module system provides real namespacing and real multi-file
compilation into one linked, running binary — the tests in
`tests/e2e_test.sh` and `tests/unit_tests.cpp` prove actual cross-module
function calls, struct types, and enum access, not merely that the
`import` syntax parses. But it is honestly a **first version**, and
specifically:

- **Not separately-compiled objects.** Every module is parsed and
  type-checked as its own unit, but lowering/codegen still happens for
  the *entire* program at once into one assembly file (see
  `compileModules()`'s final loop), the same one-shot pipeline the
  single-file path always used, just iterated over more than one
  module's functions. There is no per-module `.o` file, no incremental
  recompilation of only the module that changed, and no real linker
  step resolving symbols *across* separately assembled objects — `as`
  and `ld` are invoked exactly once each, on one generated `main.s`,
  same as the single-file path.
- **No transitive re-export.** If `A` imports `B` and `B` imports `C`,
  `A` cannot reach `C`'s symbols at all, qualified or not — only
  directly-imported modules are visible. `A` would need its own
  `import C;` to reach it.
- **No selective/partial imports.** `import other;` always brings in
  *all* of `other`'s top-level declarations; there is no
  `import other::{ f, g };`-style syntax to import a subset.
- **No visibility control.** Every top-level declaration is implicitly
  exported; there is no way to keep something module-private.
- **No module aliasing.** You cannot `import other as o;` to shorten a
  qualified name.
- **Struct/enum access through 2–3-token dot chains only.** A qualified
  enum access (`module.Enum.Variant`) is parsed by folding the first
  two segments into one compound key; deeper nesting (a struct field
  that is itself qualified, e.g. accessing a field *through* a
  qualified enum in some hypothetical extension) was never a goal and
  isn't supported.

None of this is claimed as done. A later ADR would need to supersede
this one before any of the above could be described as implemented.

## Tested invariants

- A single-file program continues to compile and behave identically
  (the single-file driver path and `TypeChecker::check()` are
  unmodified in behavior — verified by the full pre-existing 23-program
  end-to-end suite and 121 pre-existing unit assertions, all still
  green after this change).
- A two-file program with a real cross-module function call, a real
  cross-module struct type used as a local variable, and a real
  cross-module enum variant access, all compile, link, and run as one
  native binary producing the correct exit code
  (`tests/programs/modules/module_main.og` +
  `tests/programs/modules/colors.og`, `tests/e2e_test.sh`).
- Importing a module that wasn't passed on the command line is a
  compile error naming the missing module
  (`tests/programs/modules/bad_import.og`).
- Calling an unresolved symbol on a real, successfully-imported module
  is a compile error naming the qualified symbol
  (`tests/programs/modules/bad_symbol.og`) — this needed no new
  diagnostic code at all: the existing "Call to undefined function"
  check already fires correctly against a qualified name, since
  qualified and unqualified names share the same signature-table
  lookup mechanism.
- A circular import is rejected with a diagnostic naming the cycle
  (`tests/programs/modules/cycle_a.og` /
  `cycle_b.og`).
- Two independently-collected modules declaring a function under the
  *same unqualified name* type-check together without any conflict
  when both are imported and called through qualification
  (`testCheckModuleAllowsSameUnqualifiedNameInTwoModulesWithoutAmbiguity`,
  `tests/unit_tests.cpp`) — the concrete proof that same-name
  collisions across modules are a non-issue by construction, not
  resolved by some tie-breaking rule.
- An imported symbol is *not* reachable unqualified
  (`testCheckModuleRejectsUnqualifiedAccessToImportedSymbol`).
- Only the entry module is required to declare `main`; a library
  module lacking one still checks successfully
  (`testCheckModuleRejectsWhenNonEntryModuleLacksMain`, which also
  confirms the same program *is* rejected when checked as the entry
  module — proving the `requireMain` flag actually gates the check,
  not merely that omitting `main` from a lone-module test happens to
  not throw for unrelated reasons).
- Duplicate-declaration checks (two functions/structs/enums sharing a
  name within one module) are completely unaffected by module support,
  since `collectModuleSymbols` is the identical function used for both
  single-file and multi-file compilation
  (`testCollectModuleSymbolsRejectsDuplicateFunctionInOneModule`).
- An `import` in a single-file program is rejected with a clear
  diagnostic rather than silently ignored
  (`testTypeCheckerRejectsUnresolvedImportInSingleFileProgram`).
- The existing struct-variable-vs-enum-type dot-access priority rule
  (added when enums were introduced — see ADR-adjacent comments in
  `types/type_checker.cpp`) is unaffected by qualified names, since a
  local variable name can never contain a dot and so can never collide
  with a compound `"module.Type"` key.

## Consequences

Every claim about OGLang elsewhere in this repository must describe
modules using the scope recorded here: real multi-file compilation and
namespacing into one linked binary, not separate compilation, not
selective imports, not visibility control. This ADR is the single
source of truth for that distinction until a future ADR supersedes it.
