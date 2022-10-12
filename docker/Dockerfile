# Build the image
#   docker build -t vcsim <path to the docker folder in vctools>
# Running the image
#   docker run -it vcsim
# Testing if everything is ok
#   cd $VCTOOLSDIR/examples
#   make -j4
#   This should produced executables vcsim*.out On running, they should produce
#   the output and log files as described in the usage
FROM gcc:12.2.0
ENV GITHUBHOME=/usr/src
ENV VCTOOLSDIR=$GITHUBHOME/vctools
ENV PETRISIMUDIR=$GITHUBHOME/petrisimu
ENV AHIRDIR=$GITHUBHOME/ahir
ENV PATH=$PATH:$AHIRDIR/prebuilt_ubuntu_16.04_release/bin
ENV PATH=$PATH:$VCTOOLSDIR
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$VCTOOLSDIR
RUN apt update && apt install -y g++ make antlr libantlr-dev libboost-dev
WORKDIR $GITHUBHOME
RUN git clone --depth 1 https://github.com/mayureshw/vctools
RUN git clone --depth 1 https://github.com/mayureshw/petrisimu
RUN git clone --depth 1 https://github.com/mayureshw/ahir
WORKDIR $VCTOOLSDIR
RUN make -j4
