FROM ubuntu:24.04

# Install dependencies
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    apt-get install -y python3 python3-pip && \
    apt-get install -y g++-14 gcc-14 cmake && \
    apt-get install -y tree && \
    rm -rf /var/lib/apt/lists/*

# Set environment variables
ENV CXX=g++-14
ENV CC=gcc-14

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
