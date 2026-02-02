FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    qt6-base-dev \
    qt6-base-dev-tools \
    libvte-2.91-dev \
    libgtk-3-dev \
    zip \
    unzip \
    x11-apps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build && \
    cmake --build build -j$(nproc)

ENV QT_QPA_PLATFORM=xcb
ENV DISPLAY=:0
ENV QT_DEBUG_PLUGINS=1

CMD ["./build/dolphin-lite"]
