# GLib Rust migration plan

This document describes a phased strategy for rewriting GLib from C to Rust while
keeping the existing Meson build and C API usable during the transition.

## Principles

1. **Incremental replacement** — Rust modules land alongside C sources; each phase
   replaces a leaf or well-bounded subsystem.
2. **Behavior parity** — Rust implementations are validated against existing GLib
   tests where possible, plus new Rust unit tests.
3. **FFI last** — Early phases are pure Rust crates under `rust/`. C interoperability
   (static libs, `cbindgen` headers, GObject type registration) comes when a
   subsystem is ready to be called from remaining C code.
4. **Bottom-up dependency order** — Convert modules with few or no internal GLib
   dependencies first.

## Repository layout

```
rust/
  Cargo.toml              # workspace root
  glib-native/              # Phase 1+ Rust implementations
    src/
      lib.rs
      endian.rs
      checked.rs
      refcount.rs
      bytes.rs
      ...
docs/rust-migration.md    # this file
```

The crate is named `glib-native` to avoid confusion with the existing
[gtk-rs glib](https://crates.io/crates/glib) bindings crate.

## Phase overview

| Phase | Scope | Key C sources | Status |
|-------|-------|---------------|--------|
| **0** | Tooling: Cargo workspace, `cargo test` in CI, migration docs | — | **Done** |
| **1** | Foundation types: endian swaps, checked arithmetic, refcounts, `GBytes` | `gtypes.h`, `grefcount.*`, `gbytes.*` | **Done** |
| **2** | Atomics, memory helpers, strings | `gatomic.*`, `gmem.*`, `gstrfuncs.*`, `gstring.*` | Planned |
| **3** | Sequential containers | `garray.*`, `glist.*`, `gslist.*`, `gqueue.*`, `gptrarray.*` | Planned |
| **4** | Associative containers & datasets | `ghash.*`, `gtree.*`, `gdataset.*`, `gquark.*` | Planned |
| **5** | Errors, logging, options | `gerror.*`, `gmessages.*`, `goption.*` | Planned |
| **6** | I/O primitives | `gfileutils.*`, `gconvert.*`, `gcharset.*`, `gchecksum.*`, `gbase64.*` | Planned |
| **7** | Date/time & variants | `gdate.*`, `gdatetime.*`, `gvariant.*` | Planned |
| **8** | Main loop & threading | `gmain.*`, `gsource.*`, `gthread.*`, `gasyncqueue.*` | Planned |
| **9** | GObject core | `gobject/*` (types, signals, properties, values) | Planned |
| **10** | GModule & GThread | `gmodule/*`, `gthread/*` | Planned |
| **11** | GIO (split into sub-phases: streams, sockets, D-Bus, settings, …) | `gio/*` | Planned |
| **12** | GObject Introspection & tools | `girepository/*`, `tools/*` | Planned |
| **13** | Remove C implementations; expose stable C ABI from Rust via `extern "C"` | all | Planned |

## Phase 1 detail (current)

### Modules

- **`endian`** — `GUINT16/32/64_SWAP_LE_BE`, host/network byte order helpers
  from `gtypes.h`.
- **`checked`** — `g_*_checked_add` / `g_*_checked_mul` overflow-safe arithmetic.
- **`refcount`** — `grefcount` (single-threaded) and `gatomicrefcount` semantics.
- **`bytes`** — Immutable reference-counted byte buffer matching `GBytes` behavior
  (inline storage for small buffers, slicing, hash, compare).

### Exit criteria

- `cargo test` passes in `rust/`.
- Rust tests cover refcount and bytes cases from `glib/tests/refcount.c` and
  `glib/tests/bytes.c`.
- CI job `rust-check` runs on merge requests.

### Next (Phase 2)

Port `gatomic.c`, `gmem.c`, and the non-printf subset of `gstrfuncs.c` / `gstring.c`.
Introduce a `glib-native-sys` crate with `#[no_mangle]` shims only when C callers
need the new implementations.

## Running Rust tests

```bash
cd rust && cargo test
```

## Contributing

When adding a new converted module:

1. Add the module under `rust/glib-native/src/`.
2. Port or mirror relevant tests from `glib/tests/`.
3. Update the phase table in this document.
4. Do not remove C sources until the phase’s FFI integration is complete and
   upstream tests pass with the Rust backend.
