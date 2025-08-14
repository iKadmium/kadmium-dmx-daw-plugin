FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="none"

# Optionally install the cmake for vcpkg
COPY ./reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

# [Optional] Uncomment this section to install additional packages.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    libx11-dev \
    libxcomposite-dev \
    libxext-dev \
    libxinerama-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxrender-dev \
    autoconf \
    libtool \
    pkg-config \
    libasound2-dev \
    libfreetype6-dev \
    libfontconfig1-dev \
    libopengl-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    mesa-common-dev \
    automake \
    linux-libc-dev

# [Optional] Uncomment this section to install additional vcpkg ports.
RUN su vscode -c "${VCPKG_ROOT}/vcpkg install juce"
RUN su vscode -c "${VCPKG_ROOT}/vcpkg install paho-mqtt"
