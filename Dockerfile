ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ENV CXX=g++
ENV CC=gcc

# Create a working directory
COPY . /app
WORKDIR /app

RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# The whole suite: `-k integration` was justified by the eight-platform QEMU matrix, retired in
# 2026-07 for two native runners (#46), and these two legs are CI's only Linux arm64 coverage
# (#72) -- the macOS leg in build_and_test.yml is arm64 as well, but not Linux.
RUN /opt/venv/bin/python ./project.py --clean --cmake --cores all --build --test -v 1

# Save the build information
ARG DOCKER_PLATFORM=""
RUN /opt/venv/bin/python ./toolbox/build_info.py --save-to . --docker "${DOCKER_PLATFORM}"

CMD ["bash"]
