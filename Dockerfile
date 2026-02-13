# Sea-Claw Docker Image (Production)
# Multi-stage: build in builder, slim runtime image
#
# First run auto-triggers onboarding wizard (asks for LLM + Telegram config).
# Config persists in the /root/.config/seaclaw volume.
#
# Build:      docker build -t seaclaw .
# Run:        docker run -it --rm -v seaclaw-data:/root/.config/seaclaw seaclaw
# Test:       docker run --rm seaclaw test
# Re-onboard: docker run -it --rm -v seaclaw-data:/root/.config/seaclaw seaclaw --onboard

# ── Stage 1: Build ───────────────────────────────────────────
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
COPY docker-entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Config + DB persist here (mount a named volume)
RUN mkdir -p /root/.config/seaclaw
VOLUME ["/root/.config/seaclaw"]

ENTRYPOINT ["/entrypoint.sh"]
CMD ["run"]
