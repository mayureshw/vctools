# Build the image
#   docker build --progress plain -t vcsim <path to the docker folder in vctools>
# Running the image
#   docker run -it vcsim
# Testing if everything is ok
#   cd $VCTOOLSDIR/examples
#   make -j4
#   This should produced executables vcsim*.out On running, they should produce
#   the output and log files as described in the usage
FROM gcc:12.2.0
ENV GITHUBHOME=/usr/src
ENV XSBDIR=/usr/local/XSB
ENV VCTOOLSDIR=$GITHUBHOME/vctools
ENV PETRISIMUDIR=$GITHUBHOME/petrisimu
ENV AHIRDIR=$GITHUBHOME/ahir
ENV XSBCPPIFDIR=$GITHUBHOME/xsbcppif
ENV CEPTOOLDIR=$GITHUBHOME/ceptool
ENV PATH=$PATH:$AHIRDIR/prebuilt_ubuntu_16.04_release/bin
ENV PATH=$PATH:$VCTOOLSDIR
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$VCTOOLSDIR
ENV XSBLIBPATH=$CEPTOOLDIR,$VCTOOLSDIR
ENV PATH=$PATH:$XSBDIR/bin:$CEPTOOLDIR
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$XSBDIR/config/`$XSBDIR/build/config.guess`/bin
RUN mkdir -p $GITHUBHOME /usr/local $HOME/.xsb
RUN apt update && apt install -y g++ make antlr libantlr-dev libboost-dev vim libpcre3-dev wget
WORKDIR /usr/local
RUN wget --content-disposition https://sourceforge.net/projects/xsb/files/xsb/5.0%20%28Green%20Tea%29/XSB-5.0.tar.gz/download && tar xvfz XSB-5.0.tar.gz && cd XSB/build && ./configure && ./makexsb fast packages
WORKDIR $GITHUBHOME
RUN for COMP in vctools ahir petrisimu xsbcppif ceptool; do git clone --depth 1 https://github.com/mayureshw/$COMP; done
RUN cp $XSBCPPIFDIR/xsbrc.P $HOME/.xsb
WORKDIR $VCTOOLSDIR
RUN make -j4
