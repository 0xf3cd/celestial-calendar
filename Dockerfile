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

# Install Python dependencies if Requirements.txt exists
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Not installing from Requirements.txt as clang-tidy installation fails on arm/v5 and arm/v7
# Since we are not running linters here, only install `requests`
RUN /opt/venv/bin/pip install requests

# Build and test the project
# Build without setting up
RUN /opt/venv/bin/python ./project.py --clean --cmake --cores all --build --test -v 1

# Save the build information
ARG DOCKER_PLATFORM=""
RUN /opt/venv/bin/python ./toolbox/build_info.py --save-dir . --filename build_info.json --docker "${DOCKER_PLATFORM}"

CMD ["bash"]
