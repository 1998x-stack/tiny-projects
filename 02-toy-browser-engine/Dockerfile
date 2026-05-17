FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Rust toolchain
    curl build-essential pkg-config \
    libfreetype6-dev libfontconfig1-dev \
    # Image output
    libpng-dev \
    # Qt6 for GUI (optional, for mini_browser-like projects)
    qt6-base-dev \
    # CMake for C++ projects
    cmake \
    # Utilities
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

WORKDIR /workspace

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
