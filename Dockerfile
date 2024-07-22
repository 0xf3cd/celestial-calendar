# Dockerfile
FROM ubuntu:24.04

# Install dependencies
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository universe

RUN apt-get install -y python3.12 python3-pip

RUN apt-get install -y g++-14 gcc-14 cmake

RUN apt-get install -y tree

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

# Entry point for the Docker container
CMD ["./build.py", "--clean", "-cmk", "-b", "-t"]
