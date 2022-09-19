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

AASRCS		=	$(wildcard *.aa)
VCFILES		=	$(AASRCS:.aa=.vc)
TESTSRCS	=	$(AASRCS:.aa=.cpp)
TESTEXES	=	$(TESTSRCS:.cpp=.out)
DFILES		=	$(TESTSRCS:.cpp=.d)
CXXFLAGS	+=	-DWIDEUINTSZ=128


%.out:	%.cpp %.vc
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

%.vc:	%.aa
	Aa2VC -O $< | vcFormat > $@
	if [ ! -s $@ ]; then rm -f $@; false; fi

all:	$(TESTEXES)

clean:
	rm -f $(TESTEXES) $(VCFILES)

.PRECIOUS:	$(VCFILES)

%.d:	%.cpp
	set -e; $(CXX) -M $(CXXFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
		[ -s $@ ] || rm -f $@

include $(VCTOOLSDIR)/Makefile.vctools
include $(DFILES)
