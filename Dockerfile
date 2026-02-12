# Sea-Claw Docker Image
# Multi-stage: build + test in builder, slim runtime image
#
# Build:  docker build -t seaclaw .
# Run:    docker run -it --rm seaclaw
# Config: docker run -it --rm -v ~/.config/seaclaw:/root/.config/seaclaw seaclaw --config /root/.config/seaclaw/config.json

# ── Stage 1: Build & Test ────────────────────────────────────
FROM debian:bookworm-slim AS builder

RUN apt-get update -qq && \
    apt-get install -y -qq --no-install-recommends \
        build-essential \
        libcurl4-openssl-dev \
        libsqlite3-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

# Build release binary (tests run on host before commit; ASan is incompatible with Docker)
RUN make clean && make release 2>&1

# ── Stage 2: Runtime ─────────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update -qq && \
    apt-get install -y -qq --no-install-recommends \
        libcurl4 \
        libsqlite3-0 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/dist/sea_claw /usr/local/bin/sea_claw

# Default config directory
RUN mkdir -p /root/.config/seaclaw

# Default data directory
VOLUME ["/root/.config/seaclaw"]

ENTRYPOINT ["sea_claw"]
CMD ["--help"]
