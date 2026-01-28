# Dockerfile for building EFR32MG1P Zigbee projects with Silicon Labs tools
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    git \
    git-lfs \
    make \
    python3 \
    python3-pip \
    wget \
    curl \
    unzip \
    openjdk-11-jre-headless \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install GNU Arm Embedded Toolchain
RUN wget -q https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 \
    && tar -xjf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 -C /opt/ \
    && rm gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2

# Add ARM toolchain to PATH
ENV PATH="/opt/gcc-arm-none-eabi-10.3-2021.10/bin:${PATH}"

# Install Silicon Labs SLC CLI
RUN wget -q https://www.silabs.com/documents/public/software/slc_cli_linux.zip -O slc_cli.zip \
    && mkdir -p /opt/slc_cli \
    && unzip -q slc_cli.zip -d /opt/slc_cli \
    && rm slc_cli.zip

# Find and symlink the SLC executable
RUN SLC_EXEC=$(find /opt/slc_cli -type f -executable -name "slc-cli" | head -n 1) \
    && if [ -n "$SLC_EXEC" ]; then \
         SLC_DIR=$(dirname "$SLC_EXEC"); \
         ln -sf "$SLC_EXEC" "$SLC_DIR/slc"; \
         echo "export PATH=\"$SLC_DIR:\$PATH\"" >> /etc/profile.d/slc.sh; \
       fi

# Make SLC available in PATH
ENV PATH="/opt/slc_cli/slc_cli/bin/slc-cli:${PATH}"

# Set up Java options for SLC CLI
ENV JAVA_TOOL_OPTIONS="-Duser.home=/tmp"

# Verify installations
RUN arm-none-eabi-gcc --version || echo "ARM GCC not found" \
    && java -version || echo "Java not found"

# Create working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
