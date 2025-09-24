# Use Dockerfile.incr for local builds, particularly if you are facing unreliable
# downloads

FROM ubuntu:latest
ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y --no-install-recommends \
    curl wget bzip2 gzip tar xz-utils git ca-certificates build-essential \
    pkg-config libssl-dev libbz2-dev libz-dev libncurses5-dev

ENV PATH=/usr/pkg/bin:/usr/pkg/sbin:$PATH

RUN git clone --depth 1 https://github.com/NetBSD/pkgsrc /usr/pkgsrc && \
    git clone --depth 1 https://github.com/NetBSD/pkgsrc-wip /usr/pkgsrc/wip && \
    cd /usr/pkgsrc/bootstrap && \
    ./bootstrap --prefer-pkgsrc yes --make-jobs `/usr/bin/nproc` --workdir /usr/pkgsrc/work && \
    cd /usr/pkgsrc/wip/ahir && bmake MAKE_JOBS=`/usr/bin/nproc` install && \
    rm -rf /usr/pkgsrc

CMD ["/bin/bash"]
