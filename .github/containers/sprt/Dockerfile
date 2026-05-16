FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    ca-certificates \
    curl \
    jq \
    python3 \
    file \
    grep \
    gawk \
    coreutils \
    bash \
 && rm -rf /var/lib/apt/lists/*

# Build FastChess from source.
# Pin this to a known commit once the setup is working.
ARG FASTCHESS_REPO=https://github.com/Disservin/fastchess.git
ARG FASTCHESS_REF=master

RUN git clone --depth 1 --branch "${FASTCHESS_REF}" "${FASTCHESS_REPO}" /tmp/fastchess \
 && make -j -C /tmp/fastchess \
 && find /tmp/fastchess -type f -name fastchess -exec cp {} /usr/local/bin/fastchess \; \
 && chmod +x /usr/local/bin/fastchess \
 && rm -rf /tmp/fastchess

WORKDIR /work

RUN fastchess --version || true
