# ==================== Stage 1: Build ====================
FROM gcc:13-bookworm AS builder

# 替换为阿里云镜像源以加速 apt 安装
RUN sed -i 's/deb.debian.org/mirrors.aliyun.com/g' /etc/apt/sources.list.d/debian.sources && \
    sed -i 's/security.debian.org/mirrors.aliyun.com/g' /etc/apt/sources.list.d/debian.sources && \
    apt-get update && \
    apt-get install -y cmake curl xz-utils && \
    rm -rf /var/lib/apt/lists/*

# GitHub 镜像代理（国内用户加速），海外用户覆盖：docker build --build-arg GH_PROXY=
ARG GH_PROXY=https://ghproxy.net/

# ── 层 1：预下载依赖 release tarball ──────────────────
# 用 release tarball 替代 git clone，速度快一个数量级；
# 只要依赖版本不变，这一层就命中缓存
RUN set -eux; \
    mkdir -p /deps && \
    curl -fsSL "${GH_PROXY}https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.2.tar.gz" \
      -o /deps/cli11.tar.gz && \
    curl -fsSL "${GH_PROXY}https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz" \
      -o /deps/spdlog.tar.gz && \
    curl -fsSL "${GH_PROXY}https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz" \
      -o /deps/gtest.tar.gz && \
    curl -fsSL "${GH_PROXY}https://github.com/richgel999/miniz/archive/refs/tags/3.1.2.tar.gz" \
      -o /deps/miniz.tar.gz
RUN set -eux; \
    mkdir -p /deps/cli11 /deps/spdlog /deps/gtest && \
    tar xzf /deps/cli11.tar.gz   -C /deps/cli11   --strip-components=1 && \
    tar xzf /deps/spdlog.tar.gz  -C /deps/spdlog  --strip-components=1 && \
    tar xzf /deps/gtest.tar.gz   -C /deps/gtest   --strip-components=1 && \
    mkdir -p /deps/miniz && \
    tar xzf /deps/miniz.tar.gz   -C /deps/miniz   --strip-components=1 && \
    rm /deps/*.tar.gz

WORKDIR /src

# ── 层 2：拷贝源码与构建配置 ────────────────────
# 拷全源文件和测试文件，cmake configure 需要校验它们存在
COPY CMakeLists.txt .
COPY src/ src/
COPY tests/ tests/

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DFETCHCONTENT_SOURCE_DIR_CLI11=/deps/cli11 \
    -DFETCHCONTENT_SOURCE_DIR_SPDLOG=/deps/spdlog \
    -DFETCHCONTENT_SOURCE_DIR_MINIZ=/deps/miniz \
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/deps/gtest

# ── 层 3：增量编译 ──────────────────────────────────
RUN cmake --build build -j$(nproc)

# ==================== Stage 2: Runtime ====================
FROM debian:bookworm-slim

# 使用 build stage 的 libstdc++。GCC 13 将其 libstdc++ 安装在
# /usr/local/lib64/（而非系统路径），其版本高于 debian:bookworm-slim
# 自带的 version（当前为 libstdc++.so.6.0.32 → GLIBCXX_3.4.32）。
COPY --from=builder /usr/local/lib64/libstdc++.so.6* /usr/lib/x86_64-linux-gnu/
RUN ldconfig

COPY --from=builder /src/build/backer-cli /usr/local/bin/backer

ENTRYPOINT ["/usr/local/bin/backer"]
CMD ["--help"]
