![logo_ironhack_blue 7](https://user-images.githubusercontent.com/23629340/40541063-a07a0a8a-601a-11e8-91b5-2f13e4e6b441.png)

# Lab | Containerization with Docker for ML

## Overview

You will take your **cat-detection ONNX model** from the m6-09 Week-2 assessment and wrap it in a slim, multi-stage Docker image that proves the model loads on the target runtime. The container's job is to be a **model verifier**: `docker run --rm <image>` loads the model with ONNX Runtime and prints its metadata. Exit 0 on success, exit 1 on failure. That's the entire contract.

This is a 90-minute hands-on lab. You will write a Dockerfile and a `.dockerignore`, then run Docker / curl-free commands. **No Python**. No HTTP server — Day 4 (API contracts) and Day 5 (serving at scale) cover that. Day 3 is about packaging, full stop.

The serving stack is **ONNX Runtime's C API** plus a 27-line C program (provided — you don't need to read it, just compile it). The result is a real, slim runtime image, not a 1.5 GB beast.

## Learning Goals

By the end of this lab you should be able to:

- Write a multi-stage Dockerfile where stage 1 is a real builder and stage 2 is a slim runtime
- Decide what belongs in stage 1 versus stage 2 (build tools, shared libraries, the model artifact)
- Run the container as a non-root user
- Build, tag, push, and verify a working containerized model artifact

## Setup

You need:

- Docker Desktop or Docker Engine installed locally (`docker --version` should work)
- A free account on **Docker Hub** ([hub.docker.com](https://hub.docker.com)) **or** **GitHub Container Registry** (your existing GitHub account works)
- Your **`model.onnx`** from the m6-09 assessment (YOLO26 export). If you didn't keep a local copy, grab it from your m6-09 lab repo.

Fork and clone this lab repo. It contains:

- `src/check_model.c` — a 27-line C program that uses the ONNX Runtime C API to load a model and print its input/output counts. Compiled in stage 1, shipped in stage 2.

Drop your `model.onnx` into the repo root before building. **Do not commit `model.onnx`** — it's bytes that belong in the container, not in source control. Add it to `.dockerignore`'s opposite: keep it OUT of git, IN the build context.

## Tasks

### Task 1 — `.gitignore`, `.dockerignore`, and getting the model in place

1. Copy your `model.onnx` from m6-09 into this repo's root directory.
2. Add `model.onnx` to **`.gitignore`** so it doesn't get committed (it's likely tens of MB and binary).
3. Create **`.dockerignore`** that excludes `.git/`, docs, the README, anything not needed inside the image. **Do not** ignore `model.onnx` or `src/` — the build needs both.

### Task 2 — Multi-stage Dockerfile

Create `Dockerfile` at the repo root. Requirements:

**Stage 1 — `builder`** (the heavy stage; thrown away after build):

- Base: `debian:12-slim` (or equivalent slim Debian/Ubuntu).
- Install build tools: `build-essential`, `ca-certificates`, `curl`.
- Fetch the official ONNX Runtime release tarball from GitHub:
  ```
  https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-linux-x64-1.20.1.tgz
  ```
  Pin the version with a `ARG ORT_VERSION=1.20.1` so it's a single line to bump. Extract to `/opt/onnxruntime`.
- Copy `src/check_model.c` and compile it against the extracted library:
  ```
  gcc -O2 -o /out/check_model src/check_model.c \
      -I/opt/onnxruntime/include \
      -L/opt/onnxruntime/lib \
      -lonnxruntime
  ```
- Copy your `model.onnx` into the stage. **Validate it** in the build itself — fail the build if it's missing, empty, or not a real ONNX file. A simple gate works:
  ```
  RUN test -s /tmp/model.onnx && \
      file /tmp/model.onnx | grep -qi onnx && \
      echo "Model SHA-256: $(sha256sum /tmp/model.onnx | awk '{print $1}')"
  ```

**Stage 2 — `runtime`** (what you actually ship):

- Base: `debian:12-slim`.
- Install **only** the runtime dependencies — `ca-certificates`, `libstdc++6`. No compilers, no headers, no curl.
- Create a non-root user (uid 1001), `USER` it.
- Copy from stage 1:
  - the compiled `check_model` binary into `/usr/local/bin/`
  - `libonnxruntime.so*` from `/opt/onnxruntime/lib/` into `/usr/local/lib/`
  - the validated `model.onnx` into `/home/app/` with `--chown=app:app`
- Set `ENV LD_LIBRARY_PATH=/usr/local/lib` so the binary can find the shared library.
- `CMD ["check_model", "/home/app/model.onnx"]`.

Aim for a **final image under ~250 MB**. The .so file is ~60 MB; the Debian base is ~75 MB; the model varies. With aggressive cleanup, sub-200 MB is realistic.

### Task 3 — Build, push, and verify

From the repo root (with your `model.onnx` in place):

```bash
# 1. Build
docker build -t <your-namespace>/m7-03-cat-detection:v1 .

# 2. Inspect the image size (write it down — you'll report it)
docker images <your-namespace>/m7-03-cat-detection:v1

# 3. Verify it actually runs and loads your model
docker run --rm <your-namespace>/m7-03-cat-detection:v1
# Expected output (your numbers will differ for inputs/outputs):
#   ONNX model loaded OK: /home/app/model.onnx
#     inputs:  1
#     outputs: 1

# 4. Verify the user is non-root
docker run --rm --entrypoint /bin/sh <your-namespace>/m7-03-cat-detection:v1 -c id
# Expected: uid=1001(app) gid=1001(app) groups=1001(app)

# 5. Push to a public registry
docker login                                                # or: docker login ghcr.io
docker push <your-namespace>/m7-03-cat-detection:v1
```

Your image **must be public** so reviewers can pull it without credentials. Make the repository public in the registry UI.

Then update the repo `README.md` with a section titled `## Image` containing **exactly** these four items:

1. **Pull command** — a fenced shell block with the working `docker pull <fully-qualified-image>:v1`
2. **Run command** — a fenced shell block with the working `docker run --rm <image>`
3. **Image size** — final image size in MB (from `docker images`)
4. **Sample output** — the actual stdout you saw when you ran the image (model path + input/output counts)

These four items must let any reviewer with Docker installed verify your image works in under 30 seconds.

## Submission

Open a Pull Request to the lab repository with:

```
Dockerfile
.dockerignore
.gitignore                # adding model.onnx to it
README.md                 # with the ## Image section completed
src/check_model.c         # already in the starter; do not modify
```

**Do not commit `model.onnx`** to the lab repo. Paste the PR link as your deliverable. The PR description must include the public `docker pull` command on its own line.

## Quality bar

You will be reviewed on:

- **Does the image actually run** when pulled fresh and started with the documented `docker run`?
- **Is it truly multi-stage?** Stage 1 must contain `build-essential` and `curl`; stage 2 must not.
- **Is the final image under ~250 MB?** A 1.5 GB image with a 5 MB model fails this bar.
- **Is stage 1 a real validation gate?** It must fail the build if `model.onnx` is missing, empty, or not actually an ONNX file.
- **Is the user non-root?** `docker run --rm --entrypoint /bin/sh <image> -c id` must show uid 1001.
- **Is the image public?** A registry that asks for login fails the bar.
- **Is `.dockerignore` actually filtering things out?** No `.git/`, no docs, no junk.
- **Does the container exit cleanly?** Exit code 0 on success; exit 1 (with a useful message) if the model can't load.

This is a real Day-3 packaging exercise. Build it like a platform engineer would.
