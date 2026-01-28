FROM debian:12

RUN apt-get update && apt-get install -y \
    g++ \
    vim \
    openssh-server \
    openssh-client \
    net-tools \
    lsof \
    nano \
    sudo \
    iproute2 \
    libncurses5-dev \
    libncursesw5-dev \
    libsqlite3-dev \
    sqlite3