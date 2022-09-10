# vctools
Virtual Circuit simulator and some other tools for the AHIR framework

## Installing the required components

### C++ compiler that supports 2014 standard

(Most recent systems should have a suitable c++ compiler by default.)

gcc 12 was used for development of this component, though it should work with a
C++ compiler that supports 2014 standard.

If your stock compiler (typically /usr/bin/c++) does not support this and you
install a newer version at some other location, do set the following
envrionment variable preferably in your shell's rc file.

    export CXX=<Path to your c++ compiler that supports 2014 standard>

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

### AHIR (fork)

Check out the following fork of AHIR in the installation area:

    git clone --depth 1 https://github.com/mayureshw/ahir

This fork has some minor changes, with respect to AHIR, such as addition of
some Get methods in vC IR classes.

Define the following environment variables, preferably in your shell's rc file:

    export AHIRDIR=<Directory where you checked out the above AHIR fork>

Cross check. This command should show the source code of the component:

    ls $AHIRDIR

AHIR supplies pre-compiled binaries for Ubuntu. Append the path of these
executables to your PATH environment variable:

    export PATH=$AHIRDIR/prebuilt_ubuntu_16.04_release/bin:$PATH

Cross check. Type Aa2VC command - it should show a usage message:

    Aa2VC

### vctools (this component)

Check out:

    git clone --depth 1 https://github.com/mayureshw/vctools

Define the following environment variables, preferably in your shell's rc file.

    export VCTOOLSDIR=<Directory where you checked out this component>

Cross check. This command should show the source code of the component:

    ls $VCTOOLSDIR

### petrisimu : Petri net simulator

Check out:

    git clone --depth 1 https://github.com/mayureshw/petrisimu

See the package's README.md for instructions.

Cross check. This command should show the source code of the component:

    ls $PETRISIMUDIR

### Optional components

If you want to use the CEP (Complex Event Processing) tool for verification
checks on event sequences you need to install the following additional
components.

    1. XSB Prolog

    See http://xsb.sourceforge.net for installation instructions.

    Define the following environment variables, preferably in your shell's rc file:

        export XSBDIR=<Directory where you have installed XSB Prolog>

    Append the xsb executable's directory to your PATH environment variable:

        export PATH=$XSBDIR/bin:$PATH

    To cross check type "xsb" and see if it launches Prolog interpreter.  Use
    Ctrl-D to exit.

    2. XSB C++ interface

        git clone --depth 1 https://github.com/mayureshw/xsbcppif

    See the package's README.md for instructions.

    Cross check. This command should show the source code of the component:

        ls $XSBCPPIFDIR

    3. CEP tool

        git clone --depth 1 https://github.com/mayureshw/ceptool/

    See the package's README.md for instructions.

    Cross check. This command should show the source code of the component:

        ls $CEPTOOLDIR

## Setting up vctools

In above step you should have set the VCTOOLSDIR environment variable. Now cd
to it.

    cd $VCTOOLSDIR

Before running make you may want to check and alter some configurable options
in vcsimconf.h. They are documented in the same file.

If you do not wish to use the CEP capabilities mentioned above, please make the
following changes:

    1. Please comment out the following option in vcsimconf.h as:

        //#define PN_USE_EVENT_LISTENER

    2. Please comment out the following line in the Makefile by placing '#' at
       the start of the line.

        #include $(CEPTOOLDIR)/Makefile.ceptool

Once configured, run make. Use -j = number of CPU cores you have to speed up
compilation.

    make -j4

You may want to add VCTOOLSDIR to your PATH variable so that some of the
executables / script in vctools can be used without explicitly qualifying their
path.

    export PATH=$PATH:$VCTOOLSDIR

## Various tools and their usage

### 1. vC Simulator

#### How to use the simulator

    The simulator is available in the form of a shared object library that can
    be linked with a test driver. Please browse the following resources to
    understand the usage:

    1. vcsim.h : Details the test bench interface of the vC simulator

    2. The examples directory contains examples of test drivers and a Makefile
       that shows how to compile them.

#### Simulator performance tuning

To get better simulator performance:

    1. Check vcsimconf.h and minimize the amount of logging.

    2. See README.md of the petrisimu component for further fine tuning of
       multi-threaded runs.

#### WIP

Following items are a work in progress:

    1. At present the simulator treats non-blocking AHIR pipes as non-blocking
       for both read and write operations. AHIR non-blocking pipe semantics
       require the pipe to be blocking for writes and non-blocking for reads.

    2. The simulator supports integer arithmetic and logic operations for
       arbitrary widths limited up to 64 bits. AHIR does not have such limit.

    3. A compatiblity bridge between Aa simulator test drivers and vC simulator
       is to be developed.

### 2. cprcheck : A dependency verification tool for Control Path Reduction transformation in AHIR

    AHIR applies a control path reduction transformation to optimize the
    control path. This tool compares the pre and post transformation
    dependencies between events and reports the differences.

    Assuming you have added $VCTOOLSDIR to PATH, just type command "cprcheck"
    and it will show its usage message.

### 3. Complex Event Processing (CEP) tool to validate event sequences

    WIP
