# vctools
Virtual Circuit simulator and some other tools for the AHIR framework

## Installation using docker

Please see docker/Dockerfile for automated installation using docker.

## Manual installation

### C++ compiler that supports 2014 standard

(Most recent systems should have a suitable c++ compiler by default.)

gcc 12 was used for development of this component, though it should work with a
C++ compiler that supports 2017 standard.

(Note for Ubuntu users: On Ubuntu 22, default gcc version is 11.2.0 and is
known to work. On lower versions of Ubuntu 11.1.0 is available but it's not
found working.)

If your stock compiler (typically /usr/bin/c++) does not support this and you
install a newer version at some other location, do set the following
envrionment variable preferably in your shell's rc file.

    export CXX=<Path to your c++ compiler that supports 2014 standard>

### GNU make

The development environment uses version 4.3. On most systems, the default make
command should work.

### Java

Java (JRE) is required for antlr parser generator to run. These instructions
have been tested with OpenJDK 17, though any recent Java implementation should
work.

Type "java" to check and ensure that Java is working.

### antlr

Install the antlr tool as well as its run time libraries. On Ubuntu the
packages are named the following (Note: Package names may vary across distros.)

    1. antlr
    2. libantlr-dev

To cross check:

    1. Run the antlr tool and make sure that it works. On Ubuntu the command to
       run antlr is "runantlr". On some systems the command is "antlr". If so,
       define the ANTLR variable in the vctools Makefile accordingly.

    2. Ensure that directory /usr/include/antlr and file
       /usr/lib/libantlr-pic.a exist.

### boost

AHIR compilation requires the boost library. The package on Ubuntu is
libboost-dev. To cross check the installation: /usr/include/boost directory
should exist.

### vctools (this component)

Designate some directory to check out github components (e.g. $HOME/programs).
Create this directory and cd to it.

    git clone --depth 1 https://github.com/mayureshw/vctools

### Environment settings

Paste these lines in your shell's rc file, such as ~/.bashrc or ~/.profile.

    # Choose and create a directory to checkout all github components and set
    # the following variable. (Replace $HOME/programs with whatever directory
    # you have chosen and created.)
    export GITHUBHOME=$HOME/programs

    # See instructions under `Optional components'. If you install XSB Prolog
    # as per those instructions then uncomment and set this variable.
    # export XSBDIR=/usr/local/xsb

    # Set the path of vctools component and source vctoolsrc from it, which
    # will set the required variables for all the dependencies
    export VCTOOLSDIR=$GITHUBHOME/vctools
    . $VCTOOLSDIR/vctoolsrc


Make sure that these settings reflect in your environment (Typically open a new
terminal or just source the rc file)

### AHIR (fork)

Check out the following fork of AHIR in the installation area:

    cd $GITHUBHOME
    git clone --depth 1 https://github.com/mayureshw/ahir

This fork has some minor changes, with respect to AHIR, such as addition of
some Get methods in vC IR classes.

Cross check 1: This command should show the source code of the component:

    ls $AHIRDIR

Cross check 2: AHIR supplies pre-compiled binaries for Ubuntu. If you are using
a compatible distribution, the following command should show a usage message.

    Aa2VC

### petrisimu : Petri net simulator

Check out:

    cd $GITHUBHOME
    git clone --depth 1 https://github.com/mayureshw/petrisimu

Cross check: This command should show the source code of the component:

    ls $PETRISIMUDIR

### Optional components

If you want to use the CEP (Complex Event Processing) tool for verification
checks on event sequences you need to install the following additional
components.

    1. XSB Prolog

    See http://xsb.sourceforge.net for installation instructions.

    In the environment settings mentioned above set the XSB installation
    directory.

        export XSBDIR=<Directory where you have installed XSB Prolog>

    Ensure that you have sourced the new settings so that vctoolsrc derives
    additional variables from XSBDIR.

    To cross check type "xsb" and see if it launches Prolog interpreter. Use
    Ctrl-D to exit the interpreter.

    2. XSB C++ interface

        cd $GITHUBHOME
        git clone --depth 1 https://github.com/mayureshw/xsbcppif

    Cross check: This command should show the source code of the component:

        ls $XSBCPPIFDIR

    Copy $XSBCPPIFDIR/xsbrc.P $HOME/.xsb

        cp $XSBCPPIFDIR/xsbrc.P $HOME/.xsb

    3. CEP tool

        cd $GITHUBHOME
        git clone --depth 1 https://github.com/mayureshw/ceptool/

    Cross check: This command should show the source code of the component:

        ls $CEPTOOLDIR

## Setting up vctools

Environment settings should have defined variable $VCTOOLSDIR in previous setps

    cd $VCTOOLSDIR

Before running make you may want to check and alter some configurable options
in Makefile.conf. They are documented in the same file.

In particular, the CEP event sequence validation capabilities have some
additional dependencies. If you do not wish to use them set:

    USECEP  =   n

Once configured, run make. Use -j = number of CPU cores you have to speed up
compilation.

    make -j4

## Various tools and their usage

### 1. vC Simulator

#### How to use the simulator

    The simulator is available in the form of a shared object library that can
    be linked with a test driver. Please browse the following resources to
    understand the usage:

    1. vcsim.h : Details the test bench interface of the vC simulator

    2. The examples directory contains examples of test drivers and a Makefile
       that shows how to compile them.

       One important thing to notice from these examples is the mapping between
       Aa data types and the templatate argument passed to Datum. You should
       use nearest C type for the given with matching choice of signed /
       unsigned.

#### Simulator performance tuning

To get better simulator performance:

    1. Check Makefile.conf and minimize the amount of logging.

    2. See README.md of the petrisimu component for further fine tuning of
       multi-threaded runs.

#### WIP

Following items are a work in progress:

    1. A compatiblity bridge between Aa simulator test drivers and vC simulator
       is to be developed.

### 2. cprcheck : A dependency verification tool for Control Path Reduction transformation in AHIR

    AHIR applies a control path reduction transformation to optimize the
    control path. This tool compares the pre and post transformation
    dependencies between events and reports the differences.

    Assuming you have added $VCTOOLSDIR to PATH, just type command "cprcheck"
    and it will show its usage message.

### 3. Complex Event Processing (CEP) tool to validate event sequences

    WIP
