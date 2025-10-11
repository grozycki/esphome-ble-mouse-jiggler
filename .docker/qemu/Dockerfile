# syntax=docker/dockerfile:1

# Use the latest LTS base image
FROM ubuntu:24.04

# Install dependencies required to build QEMU from source, including dev packages for all runtime libs
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    git \
    build-essential \
    pkg-config \
    libglib2.0-dev \
    libpixman-1-dev \
    ninja-build \
    ca-certificates \
    python3-venv \
    python3-pip \
    python3-tomli \
    libgcrypt20-dev \
    libsdl2-dev \
    libslirp-dev \
    && rm -rf /var/lib/apt/lists/*

# Clone and build QEMU, using a cache mount to persist the git repo between builds
RUN --mount=type=cache,target=/root/qemu \
    if [ -d /root/qemu/.git ]; then \
        echo "QEMU repo found in cache, pulling latest changes..." && \
        cd /root/qemu && git pull; \
    else \
        echo "Cloning QEMU repo for the first time..." && \
        git clone --recursive --branch esp-develop https://github.com/espressif/qemu.git /root/qemu; \
    fi && \
    echo "Configuring and building QEMU..." && \
    cd /root/qemu && \
    ./configure --target-list=xtensa-softmmu && \
    make -j$(nproc) && \
    make install

# Copy the run script and make it executable
COPY run_qemu.sh /usr/local/bin/run_qemu.sh
RUN chmod +x /usr/local/bin/run_qemu.sh

# Set the entrypoint to the script
ENTRYPOINT ["bash", "/usr/local/bin/run_qemu.sh"]
