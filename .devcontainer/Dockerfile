FROM mcr.microsoft.com/devcontainers/base:3-ubuntu26.04

ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN sed -i \
        -e 's|http://archive.ubuntu.com|https://archive.ubuntu.com|g' \
        -e 's|http://ports.ubuntu.com|https://ports.ubuntu.com|g' \
        -e 's|http://security.ubuntu.com|https://security.ubuntu.com|g' \
        /etc/apt/sources.list.d/ubuntu.sources \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        gdb \
        git \
        jq \
        less \
        libcurl4-openssl-dev \
        liblua5.1-0-dev \
        liblua5.2-dev \
        liblua5.3-dev \
        liblua5.4-dev \
        liblua5.5-dev \
        libyyjson-dev \
        locales \
        lsof \
        lua5.1 \
        lua5.2 \
        lua5.3 \
        lua5.4 \
        lua5.5 \
        luarocks \
        pkg-config \
        procps \
        strace \
        tzdata \
        valgrind \
        vim \
    && locale-gen en_US.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8 \
    LWS_WORKSPACE=/workspaces/lambda-lws \
    LWS_RUNTIME_HOST=host.docker.internal \
    PATH=/workspaces/lambda-lws/.devcontainer/bin:${PATH}
