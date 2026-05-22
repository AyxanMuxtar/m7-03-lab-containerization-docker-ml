![logo_ironhack_blue 7](https://user-images.githubusercontent.com/23629340/40541063-a07a0a8a-601a-11e8-91b5-2f13e4e6b441.png)

# Lab | Containerization with Docker for ML

## Overview

You will containerize a pre-built inference binary plus a tiny ONNX model, push the image to a public registry, and submit the pull command. By the end you'll have an image any reviewer can run with one `docker run`.

This is a 90-minute hands-on lab. You will not write Python or train anything. You'll write a Dockerfile, run Docker commands, and read logs.

## Learning Goals

By the end of this lab you should be able to:

- Write a working multi-stage Dockerfile for an ML inference service
- Build, tag, and push an image to a public container registry
- Verify your image runs and serves predictions

## Setup

You need:

- Docker Desktop or Docker Engine installed locally (`docker --version` should work)
- A free account on **Docker Hub** ([hub.docker.com](https://hub.docker.com)) **or** **GitHub Container Registry** (your existing GitHub account works)

Fork and clone the lab repo. It provides:

- `runtime/serve` — a precompiled Linux/amd64 binary that loads `model.onnx` and exposes `POST /predict` on port 8080
- `model.onnx` — a small tabular classifier (~2 MB)
- `examples/request.json` — an example payload to test the running container

You don't need any language toolchain to do the lab. The binary is already built.

## Tasks

### Task 1 — Write the Dockerfile

Create `Dockerfile` at the repo root. Requirements:

- **Multi-stage build** with at least two stages. The first installs OS-level dependencies; the final stage contains only the runtime, the model, and the OS minimum.
- **Non-root user** (uid 1001 or similar) runs the binary in the final stage.
- **Bake the model** into the image — `COPY ./model.onnx ...`.
- **Expose port 8080** and set a sensible default command.
- **`.dockerignore`** at the repo root that excludes everything not needed at runtime (test data, docs, README, `.git`).

A reference Dockerfile structure is provided in the lesson notes — adapt it; don't copy-paste it word for word.

### Task 2 — Build, tag, push

From the repo root:

```bash
# 1. Build
docker build -t <your-namespace>/m7-03-eta:v1 .

# 2. Inspect the image size (write it down — you'll report it)
docker images <your-namespace>/m7-03-eta:v1

# 3. Run locally and smoke-test
docker run --rm -p 8080:8080 <your-namespace>/m7-03-eta:v1 &
sleep 2
curl -s -X POST http://localhost:8080/predict \
     -H "Content-Type: application/json" \
     --data @examples/request.json
docker stop $(docker ps -q --filter ancestor=<your-namespace>/m7-03-eta:v1)

# 4. Log in to your registry, then push
docker login                                              # or: docker login ghcr.io
docker push <your-namespace>/m7-03-eta:v1
```

Your image **must be public** so reviewers can pull it without credentials. Make the repository public in the registry UI.

### Task 3 — Document the image

Update the repo `README.md` with a section titled `## Image` containing **exactly** these four items:

1. **Pull command** — a single fenced shell block with the working `docker pull <fully-qualified-image>:v1` command
2. **Run command** — a single fenced shell block with a working `docker run --rm -p 8080:8080 ...` command
3. **Image size** — final image size in MB (from `docker images`)
4. **Sample request** — `curl` command + expected response shape

These four items must let any reviewer with Docker installed verify your image works in under 90 seconds. If they can't, you fail the task.

## Submission

Open a Pull Request to the lab repository with:

```
Dockerfile
.dockerignore
README.md           # with the ## Image section completed
```

Paste the PR link as your deliverable. The PR description must include the public `docker pull` command on its own line for the reviewer.

## Quality bar

You will be reviewed on:

- **Does the image actually run** when pulled fresh and started with the documented `docker run`?
- **Is it multi-stage and slim?** A 4 GB image with a 2 MB model fails this bar; aim for under 200 MB.
- **Is the user non-root?** Check with `docker run --rm <image> id` — uid must not be 0.
- **Is the image public?** A registry that asks for login fails the bar.
- **Is the model version logged on boot?** Run the container and check that the first log line names the model file/version.
