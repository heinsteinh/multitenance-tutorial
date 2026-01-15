# Multi-stage build for step10 complete system
# Stage 1: Builder
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    ninja-build \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
WORKDIR /opt
RUN git clone https://github.com/Microsoft/vcpkg.git && \
    ./vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Copy source code
WORKDIR /app
COPY . .

# Build the application
RUN cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja && \
    cmake --build build --parallel

# Stage 2: Runtime
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -r -s /bin/false appuser

# Create directories
RUN mkdir -p /app/data /app/config /app/logs && \
    chown -R appuser:appuser /app

WORKDIR /app

# Copy binary from builder
COPY --from=builder /app/build/step10_server /app/
COPY --from=builder /app/config /app/config

# Set ownership
RUN chown -R appuser:appuser /app

# Switch to non-root user
USER appuser

# Environment variables
ENV CONFIG_FILE=/app/config/config.json
ENV DATABASE_PATH=/app/data/multitenant.db
ENV JWT_SECRET=change-me-in-production
ENV LOG_LEVEL=info

# Expose gRPC port
EXPOSE 50053

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD grpcurl -plaintext localhost:50053 grpc.health.v1.Health/Check || exit 1

# Run the server
ENTRYPOINT ["/app/step10_server"]
CMD ["--config=/app/config/config.json"]
