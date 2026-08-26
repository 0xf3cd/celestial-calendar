ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ENV CXX=g++
ENV CC=gcc

# Create a working directory
COPY . /app
WORKDIR /app

RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Gate track of the #69 dual-track policy: pin the seed inside the image too — a docker
# build layer does not inherit the GitHub Actions env context (default matches
# DEFAULT_SEED in src/util/random.hpp).
ARG CELESTIAL_TEST_SEED=42
ENV CELESTIAL_TEST_SEED=${CELESTIAL_TEST_SEED}
# The whole suite: `-k integration` was justified by the eight-platform QEMU matrix, retired in
# 2026-07 for two native runners (#46), and these two legs are CI's only Linux arm64 coverage
# (#72) -- the macOS leg in build_and_test.yml is arm64 as well, but not Linux.
RUN /opt/venv/bin/python ./project.py --clean --cmake --cores all --build --test -v 1

# Save the build information
ARG DOCKER_PLATFORM=""
RUN /opt/venv/bin/python ./toolbox/build_info.py --save-to . --docker "${DOCKER_PLATFORM}"

CMD ["bash"]
