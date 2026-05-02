# syntax=docker/dockerfile:1
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    ca-certificates \
    git \
    wget \
    unzip \
    file \
    && pip3 install --break-system-packages cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake --preset llvm-mingw && cmake --build --preset llvm-mingw -- -j1

FROM ubuntu:24.04 AS runtime
WORKDIR /opt/airstrike3d-tools
COPY --from=builder /src/build/llvm-mingw/2_06/Release/ /opt/airstrike3d-tools/2_06/
COPY --from=builder /src/build/llvm-mingw/src/ /opt/airstrike3d-tools/src/
ENV PATH="/opt/airstrike3d-tools/2_06:${PATH}"
ENTRYPOINT ["/bin/bash"]