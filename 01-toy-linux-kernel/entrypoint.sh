#!/bin/bash
echo "========================================"
echo "  Toy Linux Kernel Development Container"
echo "========================================"
echo ""
echo "To build and run xv6 (RISC-V):"
echo "  git clone https://github.com/mit-pdos/xv6-riscv.git"
echo "  cd xv6-riscv"
echo "  make qemu"
echo ""
echo "To exit QEMU: Ctrl-A then X"
echo ""

exec "$@"
