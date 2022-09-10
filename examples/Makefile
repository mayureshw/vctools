# Instructions:
#
# - Use same basename for 3 files - namely the Aa file, vC file and the cpp
#   test bench file. It helps automate the build. On running make, a .out file
#   with the same basename will be generated.
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
