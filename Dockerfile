FROM ubuntu:24.04

# Prevent interactive prompts during apt installations
ENV DEBIAN_FRONTEND=noninteractive

# 1. Update and install essential build tools and Python
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    unzip \
    pkg-config \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# 2. Install OpenGL and GLFW dependencies
RUN apt-get update && apt-get install -y \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libglfw3-dev \
    && rm -rf /var/lib/apt/lists/*

# 3. Generate and install GLAD
# We generate a core profile for OpenGL 3.3
RUN pip3 install glad && \
    glad --profile core --api gl=3.3 --generator c --out-path /tmp/glad && \
    cp -r /tmp/glad/include/* /usr/local/include/ && \
    mkdir -p /usr/local/src && \
    cp /tmp/glad/src/glad.c /usr/local/src/ && \
    rm -rf /tmp/glad
# 4. Install OpenCV system dependencies
RUN apt-get update && apt-get install -y \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libv4l-dev \
    libxvidcore-dev \
    libx264-dev \
    libgtk-3-dev \
    libtbb-dev \
    && rm -rf /var/lib/apt/lists/*

# 5. Build and install OpenCV with OpenGL support
# Using version 4.10.0; tests and examples are disabled to speed up the container build.
WORKDIR /opt
RUN git clone --depth 1 --branch 4.10.0 https://github.com/opencv/opencv.git && \
    mkdir -p opencv/build && cd opencv/build && \
    cmake -D CMAKE_BUILD_TYPE=Release \
          -D CMAKE_INSTALL_PREFIX=/usr/local \
          -D WITH_OPENGL=ON \
          -D BUILD_EXAMPLES=OFF \
          -D BUILD_TESTS=OFF \
          -D BUILD_PERF_TESTS=OFF \
          .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd /opt && rm -rf opencv

# 6. Install MAVSDK
# We use the pre-built Debian package for v3.0.0 to save significant compile time.
RUN wget https://github.com/mavlink/MAVSDK/releases/download/v3.0.0/libmavsdk-dev_3.0.0_ubuntu24.04_amd64.deb && \
    dpkg -i libmavsdk-dev_3.0.0_ubuntu24.04_amd64.deb && \
    rm libmavsdk-dev_3.0.0_ubuntu24.04_amd64.deb

# Set the working directory for your application code
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
