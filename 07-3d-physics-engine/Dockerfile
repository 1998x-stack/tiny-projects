FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    # OpenGL + GLUT
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    freeglut3-dev \
    # GLFW
    libglfw3-dev \
    # GLEW
    libglew-dev \
    # Math library (Eigen alternative)
    libeigen3-dev \
    # Raylib for simple graphics
    libasound2-dev libx11-dev libxrandr-dev libxi-dev \
    libxcursor-dev libxinerama-dev \
    && rm -rf /var/lib/apt/lists/*

# Build raylib from source
RUN git clone --depth 1 https://github.com/raysan5/raylib.git /tmp/raylib && \
    cd /tmp/raylib/src && make PLATFORM=PLATFORM_DESKTOP && \
    cp libraylib.a /usr/local/lib/ && \
    cp raylib.h /usr/local/include/ && \
    rm -rf /tmp/raylib

WORKDIR /workspace

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
