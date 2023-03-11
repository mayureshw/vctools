# Instructions:
#
# - Use same basename for Aa file and its test driver. E.g. if your Aa file is
#   mytest.aa, its test driver should be mytest.cpp. It helps automate the
#   build. If build is successful, the vC file and an executable will be
#   generated with the same basename. E.g. mytest.vc and mytest.out
#   respectively.
#
# - On running the .out file, the default logs are produced on stdout, which
#   you may want to redirect. There are various additional logs produced
#   depending on the options you chose in $VCTOOLSDIR/vcsimconf.h. Please refer
#   to the same file for more details of the logging options and log filenames

# This will process all .aa files in the current directory. If you want to
# process only certain files, set this to names of the files. Alternatively
# when invoking make specify suitable target e.g. make test.out where you have
# test.aa and test.cpp
AASRCS		=	$(wildcard *.aa)

# # # # # Usually you won't need to change below this # # # # #

ifndef VCTOOLSDIR
$(error VCTOOLSDIR not set)
endif

VCFILES		=	$(AASRCS:.aa=.vc)
TESTSRCS	=	$(AASRCS:.aa=.cpp)
TESTEXES	=	$(TESTSRCS:.cpp=.out)
DFILES		=	$(TESTSRCS:.cpp=.d)
CEPFILES	=	$(VCFILES:.vc=.cep)
VCIRFILES	=	$(VCFILES:.vc=.vcir)
PROPFILES	=	$(VCFILES:.vc=.props)
IPROPFILES	=	$(VCFILES:.vc=.iprops)
CXXFLAGS	+=	-DWIDEUINTSZ=128 -DUSECEP # Must be consistent with make of vctools
LDFLAGS		+=	-lpthread


%.out:	%.cpp %.vc
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

%.vc:	%.aa
	Aa2VC -O $< | vcFormat > $@
	if [ ! -s $@ ]; then rm -f $@; false; fi

all:	$(TESTEXES) $(CEPFILES)

clean:
	rm -f $(TESTEXES) $(VCFILES) $(CEPFILES) $(VCIRFILES) $(PROPFILES) $(IPROPFILES)

.PRECIOUS:	$(VCFILES) $(VCIRFILES)

%.d:	%.cpp
	set -e; $(CXX) -M $(CXXFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
		[ -s $@ ] || rm -f $@

include $(VCTOOLSDIR)/Makefile.vctools
include $(VCTOOLSDIR)/Makefile.cepspec
include $(CEPTOOLDIR)/Makefile.ceptool
include $(DFILES)
