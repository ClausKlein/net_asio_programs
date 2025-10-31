# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# use ubuntu-24.04 (noble)
FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

USER vscode

# -------------------------------
# Install latest CMake and Boost
# -------------------------------
RUN sudo apt-get update -qq \
    && sudo apt-get install -y -qq wget gpg software-properties-common \
    && wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | gpg --dearmor \
        | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null \
    && echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main" \
        | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null \
    && sudo apt-get update -qq \
    && sudo apt-get install -y -qq cmake libboost-all-dev \
    && sudo rm -rf /var/lib/apt/lists/*

# -------------------------------
# Install pre-commit, gcovr, ninja
# -------------------------------
RUN sudo apt-get update -qq \
    && sudo apt-get install -y -qq python3-pip \
    && pip3 install --no-cache-dir pre-commit gcovr ninja cmake \
    && sudo rm -rf /var/lib/apt/lists/*

# -------------------------------
# Avoid ASAN stalling
# -------------------------------
# Reduces mmap randomization slightly so AddressSanitizer works reliably
RUN sudo sysctl -w vm.mmap_rnd_bits=28
