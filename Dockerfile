# Dockerfile for building EFR32MG1P Zigbee projects using Silicon Labs tooling
# Based on NabuCasa/silabs-firmware-builder approach
FROM debian:trixie-slim

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV HOME=/root
ENV DEBIAN_FRONTEND=noninteractive

# Install base dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    aria2 \
    ca-certificates \
    git \
    libarchive-tools \
    bzip2 \
    unzip \
    jq \
    make \
    libstdc++6 \
    libgl1 \
    libpng16-16 \
    libpcre2-16-0 \
    libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Install slt (Silicon Labs Tooling) CLI
RUN aria2c --checksum=sha-256=8c2dd5091c15d5dd7b8fc978a512c49d9b9c5da83d4d0b820cfe983b38ef3612 -o slt.zip \
        https://www.silabs.com/documents/public/software/slt-cli-1.1.0-linux-x64.zip \
    && bsdtar -xf slt.zip -C /usr/bin && rm slt.zip \
    && chmod +x /usr/bin/slt \
    && slt --non-interactive install conan

# Install Silicon Labs toolchain and SDK via slt
RUN slt --non-interactive install \
        cmake/3.30.2 \
        ninja/1.12.1 \
        commander/1.22.0 \
        slc-cli/6.0.15 \
        simplicity-sdk/2025.6.2 \
    # Create stable symlinks
    && mkdir -p /root/.silabs/slt/bin \
    && ln -s "$(slt where commander)/commander" /root/.silabs/slt/bin/commander \
    && ln -s "$(slt where cmake)/bin/cmake" /root/.silabs/slt/bin/cmake \
    && ln -s "$(slt where ninja)/ninja" /root/.silabs/slt/bin/ninja \
    && printf '#!/bin/sh\nexec "%s/slc" "$@"\n' "$(slt where slc-cli)" > /root/.silabs/slt/bin/slc \
    && chmod +x /root/.silabs/slt/bin/slc \
    # Clean up caches
    && rm -rf /root/.silabs/slt/installs/archive/*.zip \
              /root/.silabs/slt/installs/archive/*.tar.* \
              /root/.silabs/slt/installs/conan/p/*/d/

# Download Gecko SDK 4.5.0 (for Series 1 support)
RUN aria2c --checksum=sha-256=b5b2b2410eac0c9e2a72320f46605ecac0d376910cafded5daca9c1f78e966c8 -o sdk.zip \
        https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
    && mkdir /gecko_sdk_4.5.0 && bsdtar -xf sdk.zip -C /gecko_sdk_4.5.0 \
    && rm sdk.zip

# Create wrapper script to add GCC to PATH at runtime
RUN echo '#!/bin/bash\n\
GCC_BIN=$(find /root/.silabs/slt/installs/conan/p -type d -name "gcc-*" -path "*/p/bin" | head -1)\n\
export PATH="$GCC_BIN:/root/.silabs/slt/bin:$PATH"\n\
exec "$@"' > /usr/local/bin/entrypoint.sh \
    && chmod +x /usr/local/bin/entrypoint.sh

# Add tools to PATH
ENV PATH="/root/.silabs/slt/bin:$PATH"

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

WORKDIR /workspace

CMD ["/bin/bash"]
