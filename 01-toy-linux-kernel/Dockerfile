FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Build tools
    build-essential \
    gcc-riscv64-linux-gnu \
    binutils-riscv64-linux-gnu \
    # QEMU for RISC-V emulation
    qemu-system-misc \
    qemu-system-x86 \
    # Debugging
    gdb-multiarch \
    # Utilities
    git \
    make \
    perl \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Set up cross-compiler symlinks
RUN ln -sf /usr/bin/riscv64-linux-gnu-gcc /usr/local/bin/riscv64-unknown-elf-gcc && \
    ln -sf /usr/bin/riscv64-linux-gnu-ld /usr/local/bin/riscv64-unknown-elf-ld && \
    ln -sf /usr/bin/riscv64-linux-gnu-objcopy /usr/local/bin/riscv64-unknown-elf-objcopy && \
    ln -sf /usr/bin/riscv64-linux-gnu-objdump /usr/local/bin/riscv64-unknown-elf-objdump

WORKDIR /workspace

# Entry point: interactive shell to build and run kernel
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
