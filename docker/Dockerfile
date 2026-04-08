FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    libssl-dev \
    libsqlcipher-dev \
    valgrind \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

COPY . /app
RUN make clean && make

CMD ["bash"]