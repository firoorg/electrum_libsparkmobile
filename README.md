# electrum_libsparkmobile

Native C/C++ Spark library for Electrum-Firo. It wraps
[firoorg/sparkmobile](https://github.com/firoorg/sparkmobile) behind a C ABI
that Electrum-Firo loads with `ctypes`.

## Building

```bash
# from the electrum-firo checkout
./contrib/make_libsparkmobile.sh
```

The script configures `src/` with CMake, fetches the pinned dependencies,
builds a shared library and installs it into `electrum_dash/`, where the Python
side loads it with `ctypes`. Desktop targets only: macOS, Linux and Windows
hosts are detected automatically. There is no mobile build — Spark is not part
of Electrum-Firo's Android package.

Dependencies are fetched at exact, verified commits — see
[DEPENDENCIES.md](DEPENDENCIES.md). Nothing here resolves a moving branch or a
mutable tag; a checkout that does not match its pin, or that has local
modifications, is discarded and re-fetched rather than built.

## Layout

```
src/electrum_libsparkmobile.{h,cpp}  the C ABI: 15 exports, the only door out
src/transaction.{h,cpp}              mint and spend construction, coin selection
src/utils.{h,cpp}                    address/coin decoding, spend key derivation
src/structs.h                        the flat C structs carried across the ABI
src/CMakeLists.txt                   build and dependency pinning
patches/                             CMake build files for sparkmobile and OpenSSL
src/deps/boost-cmake/                vendored CMake wrapper for Boost
src/deps/missing_headers/            POSIX headers missing on Windows and macOS
src/deps/sparkmobile/                fetched at build time, not in this repository
src/deps/openssl-cmake/              fetched at build time, not in this repository
```

## ABI contract

Everything crossing this boundary is either wallet key material or data chosen
by a remote Electrum server, so the library treats every argument as hostile:

- Every pointer argument carries an explicit byte length. Fixed-width inputs
  (32-byte hashes and keys, 34-byte linking tags) are checked for the exact
  size; nothing is inferred from the protocol.
- Counts must be non-negative and are capped, and no pointer arithmetic happens
  before the count and the pointer have been validated. Allocation sizes are
  computed with checked multiplication.
- No exception may escape an export. Every entry point ends in `catch (...)`
  and reports failure by returning `NULL` or a result struct with an error
  field, never by terminating the host process.
- Addresses are rejected unless `Q1` and `Q2` are canonical group elements that
  are not the point at infinity, and unless the encoded network matches the one
  the caller expects. A checksum-valid infinity address would otherwise produce
  outputs that confirm on chain but can never be identified or recovered.
- Every buffer and struct returned must be released with `native_free()`. The
  only exception is the opaque full view key handle, which is released with
  `deleteFullViewKey()`. Results are zero-initialised, so a partially built
  result is always safe to free and never exposes heap residue.

Changing an export's signature means changing `electrum_dash/libsparkmobile.py`
in the same commit; the two sides are one contract.

## Tests

The ABI regression tests live with the caller, in
`electrum_dash/tests/test_libsparkmobile.py` of the Electrum-Firo repository,
because they exercise the Python binding and the native library together:

```bash
python3 -m unittest electrum_dash.tests.test_libsparkmobile
```

They cover address validation (including a constructed infinity-point address),
the length and exception guards on the ABI, the allocator ownership contract,
and the mint serial-context invariant. There is no in-repo C++ test target or
fuzz harness yet.

## Known open items

See the end of [DEPENDENCIES.md](DEPENDENCIES.md): OpenSSL 1.1.1w is past its
public end of life, and upstream sparkmobile's deterministic key derivation
produces a constant `s2`. Neither can be fixed in this repository alone.
