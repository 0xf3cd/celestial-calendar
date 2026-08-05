ARG BASE_IMAGE=debian:latest
FROM ${BASE_IMAGE}

# Add the Debian Sid repository for GCC 14
RUN echo "deb http://deb.debian.org/debian sid main" >> /etc/apt/sources.list && \
    echo "deb-src http://deb.debian.org/debian sid main" >> /etc/apt/sources.list

# Install dependencies
RUN apt-get update && \
apt-get upgrade -y && \
apt-get install -y aptitude && \
aptitude update

# Install required packages
RUN aptitude install -y wget curl tree git gnupg software-properties-common build-essential

# Install GCC 14 and cmake
RUN aptitude install -y gcc-14 g++-14 cpp-14 gcc-14-base && \
    aptitude install -y cmake

# Set environment variables for GCC 14
ENV CXX=g++-14
ENV CC=gcc-14

# Create a working directory
COPY . /app
WORKDIR /app

# Install Python3
RUN aptitude install -y python3 python3-pip python3-venv python3-full
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Build and test the project
# Gate track of the #69 dual-track policy: pin the seed inside the image too — a docker
# build layer does not inherit the GitHub Actions env context (default matches
# DEFAULT_SEED in src/util/random.hpp).
ARG CELESTIAL_TEST_SEED=42
ENV CELESTIAL_TEST_SEED=${CELESTIAL_TEST_SEED}
RUN if [ -f Requirements.txt ]; then /opt/venv/bin/pip install -r Requirements.txt; fi

# The whole suite: `-k integration` was justified by the eight-platform QEMU matrix, retired in
# 2026-07 for two native runners (#46), and these two legs are CI's only arm64 coverage (#72).
RUN /opt/venv/bin/python ./project.py --clean --cores all --all -v 1

# Save the build information
ARG DOCKER_PLATFORM=""
RUN /opt/venv/bin/python ./toolbox/build_info.py --save-to . --docker "${DOCKER_PLATFORM}"

CMD ["bash"]
