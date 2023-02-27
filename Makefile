ifndef VCTOOLSDIR
$(error VCTOOLSDIR not set)
endif

include Makefile.conf

NON_STPN_MODES	=	FAST
STPN_MODES		= 	STPN RANDOMPRIO RANDOMPICK
SIMU_MODES		=	$(NON_STPN_MODES) $(STPN_MODES)

ifeq ($(findstring $(SIMU_MODE),$(SIMU_MODES)),)
$(error SIMU_MODE must be set to one of $(SIMU_MODES) got $(SIMU_MODE))
endif

CXXFLAGS	+=	-DSIMU_MODE_$(SIMU_MODE)

ifneq ($(findstring $(SIMU_MODE),$(STPN_MODES)),)
USECEP	=	y
endif

OS	=	$(shell uname -s)

ifeq ($(OS),NetBSD)
LDFLAGS	+=	-L/usr/pkg/lib -lantlr
CXXFLAGS+=	-I/usr/pkg/include
ANTLR	=	antlr
else
LDFLAGS	+=	-lantlr-pic
ANTLR	=	runantlr
endif

AHIRHDRDIRS	=	$(AHIRDIR)/v2/libAhirV2/include $(AHIRDIR)/v2/BGLWrap/include
AHIRSRCDIR	=	$(AHIRDIR)/v2/libAhirV2/src
AHIRGRAMDIR	=	$(AHIRDIR)/v2/libAhirV2/grammar

VCGRAMMAR	=	$(AHIRGRAMDIR)/vc.g
AHIRSRCS	=	$(wildcard $(AHIRSRCDIR)/*.cpp)
PARSERSRCS	=	vcParser.cpp vcLexer.cpp
VCTOOLSRCS	=	vcsim.cpp cprcheck.cpp
PARSERHDRS	=	$(PARSERSRCS:.cpp=.hpp) vcParserTokenTypes.hpp
MISCFILES	=	vcParserTokenTypes.txt
AHIROBJS	=	$(notdir $(AHIRSRCS:.cpp=.o))
PARSEROBJS	=	$(PARSERSRCS:.cpp=.o)
VCTOOLOBJS	=	$(VCTOOLSRCS:.cpp=.o)
DFILES		=	$(notdir $(AHIRSRCS:.cpp=.d)) $(PARSERSRCS:.cpp=.d) $(VCTOOLSRCS:.cpp=.d)

CXXFLAGS	+=	$(addprefix -I,$(AHIRHDRDIRS)) -I.
CXXFLAGS	+=	-fPIC -g
VPATH		+=	$(AHIRSRCDIR)

BINS		=	libahirvc.a libvcsim.so cprcheck.out
EXCLUDEOPT	=	libahirvc.a
OPTBINS		=	$(filter-out $(EXCLUDEOPT), $(BINS))

ifeq ($(USECEP),y)
BINS		+=	vc2p.out
VCTOOLSRCS	+=	vc2p.cpp
endif

vc2p.out:LDFLAGS	+=	-L $(VCTOOLSDIR) -lvcsim -lahirvc
$(OPTBINS):CXXFLAGS	+=	-O3

# Need to touch the file as antlr doesn't change the timestamp if the file wasn't changed
%Parser.cpp %Lexer.cpp %Parser.hpp %Lexer.hpp: $(AHIRGRAMDIR)/%.g
	$(ANTLR) $< && touch $@

%.d:	%.cpp
	set -e; $(CXX) -M $(CXXFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
		[ -s $@ ] || rm -f $@

all:	$(BINS)

libahirvc.a:	$(AHIROBJS) $(PARSEROBJS)
	ar rv $@ $?

$(AHIROBJS):	$(PARSERHDRS)
$(DFILES):		$(PARSERHDRS)

$(PARSERSRCS) $(PARSERHDRS):	$(VCGRAMMAR)

libvcsim.so:	vcsim.o libahirvc.a
	$(CXX) -shared $^ $(LDFLAGS) -o $@

cprcheck.out:	cprcheck.o libahirvc.a
	$(CXX) $^ $(LDFLAGS) -o $@

vc2p.out:	vc2p.o libvcsim.so libahirvc.a
	$(CXX) $< $(LDFLAGS) -o $@

ifdef XSBDIR
opf.h:	opf.P
	echo [opf]. | xsb > $@
endif

clean:
	rm -f $(AHIROBJS) $(PARSEROBJS) $(VCTOOLOBJS) $(BINS) $(PARSERSRCS) $(PARSERHDRS) $(MISCFILES) $(DFILES)

CONFOPTS	=	USECEP USESEQNO DATUMDBG PIPEDBG OPDBG PNDBG GEN_CPDOT GEN_DPDOT GEN_PETRIDOT GEN_PETRIJSON GEN_PETRIPNML
CXXFLAGS	+=	$(foreach OPT, $(CONFOPTS), $(if $(filter y, $($(OPT))), -D$(OPT)))
CXXFLAGS	+=	-DWIDEUINTSZ=$(WIDEUINTSZ)

ifeq ($(USECEP),y)
include $(XSBCPPIFDIR)/Makefile.xsbcppif
endif

ifeq ($(USECEP),y)
include $(CEPTOOLDIR)/Makefile.ceptool
endif

include $(PETRISIMUDIR)/Makefile.petrisimu
include $(DFILES)
