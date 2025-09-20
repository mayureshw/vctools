FROM ubuntu:latest
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    wget \
    bzip2 \
    gzip \
    tar \
    xz-utils \
    git \
    ca-certificates \
    build-essential \
    pkg-config \
    libssl-dev \
    libbz2-dev \
    libz-dev \
    libncurses5-dev \
    && rm -rf /var/lib/apt/lists/*

RUN cd /usr && git clone --depth 1 https://github.com/NetBSD/pkgsrc
RUN cd /usr/pkgsrc && git clone --depth 1 https://github.com/NetBSD/pkgsrc-wip wip
RUN cd /usr/pkgsrc/bootstrap && ./bootstrap --prefer-pkgsrc yes --make-jobs `/usr/bin/nproc` --workdir /usr/pkgsrc/work
ENV PATH=/usr/pkg/bin:/usr/pkg/sbin:$PATH
RUN cd /usr/pkgsrc/wip/ahir && bmake MAKE_JOBS=`/usr/bin/nproc` update


WORKDIR /workspace
CMD ["/bin/bash"]

