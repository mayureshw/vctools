# vctools
Virtual Circuit simulator and some other tools for the AHIR framework

## Installing the required components

### C++ compiler that supports 2014 standard

(Most recent systems should have a suitable c++ compiler by default.)

gcc 12 was used for development of this component, though it should work with a
C++ compiler that supports 2014 standard.

If your stock compiler (typically /usr/bin/c++) does not support this and you
install a newer version at some other location, do set the following
envrionment variable.

    CXX:    Path to your c++ compiler that supports 2014 standard

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

AHIR compilation requires boost. The package on Ubuntu is libboost-dev. To
cross check the installation: /usr/include/boost directory should exist.

### AHIR (fork)

Check out the following fork of AHIR in the installation area:

    git clone --depth 1 https://github.com/mayureshw/ahir

This fork has some minor changes, with respect to AHIR, such as addition of
some Get methods in vC IR classes.

Define the following environment variables, preferably in your shell's rc file:

    AHIRDIR: Directory where you checked out the above AHIR fork

Cross check. This command should show the source code of the component:

    ls $AHIRDIR

### vctools (this component)

Check out:

    git clone --depth 1 https://github.com/mayureshw/vctools

Define the following environment variables, preferably in your shell's rc file.

    VCTOOLSDIR: Directory where you checked out this component

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

Before running make you may want to check and alter some configurable options
in vcsimconf.h. They are documented in the same file.

    cd $VCTOOLSDIR
    make -j4

## WIP

Following items are a work in progress:

    1. At present the simulator treats non-blocking AHIR pipes as non-blocking
       for both read and write operations. AHIR non-blocking pipe semantics
       require the pipe to be blocking for writes and non-blocking for reads.

    2. The simulator supports integer arithmetic and logic operations for
       arbitrary widths limited up to 64 bits. AHIR does not have such limit.

    3. A compatiblity bridge between Aa simulator test drivers and vC simulator
       is to be developed.
