# Pinned native dependencies

Every dependency of `electrum_libsparkmobile` is fetched at an exact commit and
verified after checkout (`src/CMakeLists.txt`). A build must never resolve a
moving branch or a mutable tag: doing so would change the wallet's cryptography
without changing anything in this repository, so nothing that can be verified
about this tree would say anything about future builds.

| Dependency | Source | Pinned revision |
| --- | --- | --- |
| sparkmobile | https://github.com/firoorg/sparkmobile.git | `a9d078955312b3467f5bb14cbd64ab2ed8852256` |
| openssl-cmake | https://github.com/janbar/openssl-cmake.git | `b0ac69581958cd658364147da9057da89a01c394` (OpenSSL 1.1.1w) |
| Boost | https://archives.boost.io/release/1.71.0/source/boost_1_71_0.zip | SHA-256 `85a94ac71c28e59cf97a96714e4c58a18550c227ac8b0388c260d6c717e47a69` |

The build overwrites exactly these files inside the fetched trees, from
`patches/`:

- `sparkmobile/CMakeLists.txt`
- `sparkmobile/secp256k1/CMakeLists.txt`
- `openssl-cmake/CMakeLists.txt`

Any other local modification to a dependency causes the tree to be discarded and
re-fetched.

## Updating a pin

1. Read the upstream diff between the current pin and the new commit.
2. Update the commit here and in `src/CMakeLists.txt`.
3. Rebuild from a clean `src/deps` and re-run the wrapper tests.

To check what upstream has moved to, or to develop against it, configure with:

```bash
cmake ... -DSPARKMOBILE_GIT_REF=HEAD
```

The branch is resolved to a concrete commit, printed, and then fetched and
verified exactly like a pin, so the build still records what it built. It warns
that the result is not reproducible: pin the resolved commit here and in
`src/CMakeLists.txt` before releasing.

## GMP

GMP is deliberately not linked. `find_package(GMP)` used to pick it up whenever
the build machine happened to have it installed, which produced different
binaries on different machines and added an LGPL dependency; it also applied its
include directories to the wrong target. secp256k1 now always uses its builtin
field and scalar inversion, as upstream's own build does.

## Known open items

- **OpenSSL 1.1.1w is past its public end of life (11 September 2023).** It is
  pinned by commit rather than by tag, so the build is reproducible, but the
  branch no longer receives public security fixes. Spark uses it for
  `RAND_bytes`, SHA-512, ChaCha20-Poly1305 and memory cleansing. Migrating to a
  supported OpenSSL release has to happen together with upstream sparkmobile;
  tracked as a follow-up, not fixed here.
- **Deterministic key derivation in upstream `src/keys.cpp` produces a constant
  `s2`.** This is deployed protocol behaviour: changing it locally would change
  P2, every address, and coin recovery, and would diverge from Firo Core and the
  mobile wallets. It must be fixed as a versioned, ecosystem-wide migration
  upstream, never unilaterally here.
- **Spend transactions are built as V2 only.** After the H2 fork, V1 spends stay
  single-input; multi-input requires the V2 format, which the pinned revision
  introduced. The ABI exposes no version selector: every spend and fee estimate
  is V2, and the caller must emit `nVersion=3` with `nType=11`.
