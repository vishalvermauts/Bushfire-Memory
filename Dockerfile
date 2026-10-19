# ==============================================================================
# Bushfire Memory (v3.0 C++20 Edition) — Multi-Stage Linux Dockerfile
# ==============================================================================

# --- Stage 1: Build & Verification ---
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy full source tree
COPY . .

# Configure and compile with Ninja (default 2 parallel jobs for safety)
RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel 2

# Execute verification test suite during build
RUN ./build/bushfire_memory_tests
RUN ./build/bushfire_memory_cli --self-check

# --- Stage 2: Production Runtime ---
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    libsqlite3-0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled binaries from builder stage
COPY --from=builder /src/build/bushfire_memory_server /usr/local/bin/
COPY --from=builder /src/build/bushfire_memory_cli /usr/local/bin/
COPY --from=builder /src/build/bushfire_memory_tests /usr/local/bin/

# Copy embedded web assets
COPY web /app/web

EXPOSE 8080

ENTRYPOINT ["bushfire_memory_server", "--host", "0.0.0.0", "--port", "8080", "--web", "/app/web"]
