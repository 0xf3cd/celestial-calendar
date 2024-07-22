ARG BASE_IMAGE=debian:latest
FROM ${BASE_IMAGE}

# Install dependencies
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y wget gnupg software-properties-common

# Add the Debian Sid repository for GCC 14
RUN echo "deb http://deb.debian.org/debian sid main" >> /etc/apt/sources.list && \
    echo "deb-src http://deb.debian.org/debian sid main" >> /etc/apt/sources.list && \
    apt-get update

# Install GCC 14
RUN apt-get update && \
    apt-get install -y gcc-14 g++-14 cpp-14 gcc-14-base && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Set environment variables for GCC 14
ENV CXX=g++-14
ENV CC=gcc-14

# Install other required packages
RUN apt-get update && \
    apt-get install -y cmake python3 python3-pip tree && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Create a working directory
WORKDIR /app

# Copy the project files
COPY . .

# Install Python dependencies if Requirements.txt exists
RUN if [ -f Requirements.txt ]; then python3 -m pip install -r Requirements.txt; fi

# Make the build script executable
RUN chmod +x build.py

# Build and test the project
RUN ./build.py --clean -cmk -b -t

CMD ["bash"]
