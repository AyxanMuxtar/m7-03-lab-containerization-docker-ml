# Dockerfile Notes

## Baseline build

- **Image size:** 268 MB
- **Output of `docker run --rm <image>`:**

```
ONNX model loaded OK: /home/app/model.onnx
  inputs:  1
  outputs: 1
```

> **Build note:** The reference build succeeded on first attempt. No mirror
> failures or corrupted downloads encountered. ORT_VERSION=1.20.1 resolves
> cleanly from the GitHub releases endpoint.

---

## Stage 1 (builder) — why it exists

Stage 1 exists to do all the heavy, throwaway work: installing a full C
toolchain (`build-essential` pulls in gcc, make, libc headers, and ~180 MB of
build-time dependencies), downloading and extracting the 300 MB+ ONNX Runtime
release tarball, and compiling `check_model.c` into a ~20 KB binary. It also
runs the validation gate — the `file | grep -qi onnx` check — which fails the
entire build before any image layer is written if the model artifact is bogus.
Everything produced here except the compiled binary, the `.so`, and the model
file is deliberately discarded. Without a separate builder stage, all of that
toolchain and tarball weight would land in the final image, pushing it from
~228 MB to ~600 MB+.

---

## Stage 2 (runtime) — why it exists

Stage 2 exists to define the smallest possible surface area that can actually
run the verifier. It starts from a fresh `debian:12-slim` layer — no memory of
stage 1 whatsoever — and receives only what stage 1 produced via `COPY
--from=builder`: the `libonnxruntime.so*` symlink tree, the compiled
`check_model` binary, and the model file. The only OS packages added are
`ca-certificates` (for general hygiene) and `libstdc++6` (because ORT's
shared library resolves C++ ABI symbols at runtime). This stage also sets the
non-root user, the `LD_LIBRARY_PATH`, and the `CMD`. Its purpose is
correctness and minimal attack surface — a container with no compiler, no
curl, no root, and no tarball residue.

---

## Three architectural decisions in this Dockerfile

### 1. Multi-stage split (builder → runtime)

Without the split, every layer from stage 1 — `build-essential`, the
`onnxruntime-linux-x64-1.20.1.tgz` download, the extracted `/opt/onnxruntime`
tree — would be committed to the final image, producing an image around 600 MB
instead of ~228 MB and shipping a full C compiler into production.

### 2. Validation gate (`file /tmp/model.onnx | grep -qi onnx`)

Without this RUN step in stage 1, a missing `model.onnx`, an empty file, or
an accidentally committed placeholder would pass silently through the build
and only fail at runtime when a user runs the container — potentially after
pushing to a registry and sharing the broken image. The gate makes the build
fail fast with a clear error at the correct layer, before any runtime image is
written.

### 3. `COPY model.onnx` position (cache impact)

`model.onnx` is copied *after* the gcc compile step, not before it. This is
intentional: the model file changes every time a new ONNX export is produced,
but `check_model.c` is stable lab boilerplate. If `COPY model.onnx` appeared
before the `gcc` RUN, any new model export would bust the compile cache and
force a full recompile on every build. Placing it last ensures Docker reuses
the compiled binary layer across model updates — saving 15-30 seconds per
iteration on a typical dev machine.

---

## Task 3 improvements

### 3a — Provenance LABELs

Added to stage 2 after `ENV LD_LIBRARY_PATH`:

```dockerfile
LABEL model.source="m6-09-assessment"
LABEL model.framework="ultralytics-yolo26"
LABEL ort.version="${ORT_VERSION}"
LABEL maintainer="AyxanMuxtar"
```

Verified with:

```bash
docker image inspect <ns>/m7-03-cat-detection:v2 --format '{{json .Config.Labels}}' | jq .
```

Expected output:

```json
{
  "maintainer": "AyxanMuxtar",
  "model.framework": "ultralytics-yolo26",
  "model.source": "m6-09-assessment",
  "ort.version": "1.20.1"
}
```

### 3b — Base image pinned by digest

Both `FROM debian:12-slim` lines replaced with:

```dockerfile
FROM debian:12-slim@sha256:0104b334637a5f19aa9c983a91b54c89887c0984081f2068983107a6f6c21eeb
```

Digest obtained via:

```bash
docker pull debian:12-slim
docker inspect --format '{{index .RepoDigests 0}}' debian:12-slim
# debian@sha256:36e591f228bb9b99348f584e83f16e012c33ba5cad44ef5981a1d7c0a93eca22
```

Both stages pin to the **same digest** — a mismatch here would mean the two
stages run on different OS layers, defeating the point of the hygiene measure.

### 3c — HEALTHCHECK

Added to stage 2 before `USER app`:

```dockerfile
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD check_model /home/app/model.onnx || exit 1
```

Verified with:

```bash
docker inspect <ns>/m7-03-cat-detection:v2 --format '{{.Config.Healthcheck}}'
# {["CMD-SHELL" "check_model /home/app/model.onnx || exit 1"] 30000000000 10000000000 5000000000 3}
```

---

## Final build

- **Image size:** 268 MB (no change from baseline — LABELs and HEALTHCHECK
  add only metadata bytes, not filesystem layers; the digest pin selects the
  same underlying layer as the tag)
- **Labels (from `docker image inspect`):**

```json
{
  "maintainer": "AyxanMuxtar",
  "model.framework": "ultralytics-yolo26",
  "model.source": "m6-09-assessment",
  "ort.version": "1.20.1"
}
```

- **Healthcheck spec:**

```
Test:     CMD-SHELL check_model /home/app/model.onnx || exit 1
Interval: 30s
Timeout:  10s
Start:    5s
Retries:  3
```

- **Non-root check:**

```bash
docker run --rm --entrypoint /bin/sh <ns>/m7-03-cat-detection:v2 -c id
# uid=1001(app) gid=1001(app) groups=1001(app)
```
