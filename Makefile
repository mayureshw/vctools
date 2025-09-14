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
LDFLAGS	+=	-L/usr/pkg/lib -L. -lantlr -ldl
CXXFLAGS+=	-I/usr/pkg/include
ANTLR	=	antlr
endif

AHIRHDRDIRS	=	$(AHIRDIR)/v2/libAhirV2/include $(AHIRDIR)/v2/BGLWrap/include
AHIRSRCDIR	=	$(AHIRDIR)/v2/libAhirV2/src
AHIRGRAMDIR	=	$(AHIRDIR)/v2/libAhirV2/grammar

VCGRAMMAR	=	$(AHIRGRAMDIR)/vc.g
AHIRSRCS	=	$(wildcard $(AHIRSRCDIR)/*.cpp)
PARSERSRCS	=	vcParser.cpp vcLexer.cpp
VCTOOLSRCS	=	vcsim.cpp
PARSERHDRS	=	$(PARSERSRCS:.cpp=.hpp) vcParserTokenTypes.hpp
MISCFILES	=	vcParserTokenTypes.txt
AHIROBJS	=	$(notdir $(AHIRSRCS:.cpp=.o))
PARSEROBJS	=	$(PARSERSRCS:.cpp=.o)
VCTOOLOBJS	=	$(VCTOOLSRCS:.cpp=.o)
OBJS		=	$(AHIROBJS) $(PARSEROBJS) $(VCTOOLOBJS)
DFILES		=	$(notdir $(AHIRSRCS:.cpp=.d)) $(PARSERSRCS:.cpp=.d) $(VCTOOLSRCS:.cpp=.d)

CXXFLAGS	+=	$(addprefix -I,$(AHIRHDRDIRS)) -I.
CXXFLAGS	+=	-std=c++17 -fPIC -MMD -MP -g
VPATH		+=	$(AHIRSRCDIR)

BINS		=	libahirvc.a libvcsim.so
EXCLUDEOPT	=	libahirvc.a
OPTBINS		=	$(filter-out $(EXCLUDEOPT), $(BINS))

ifeq ($(USECEP),y)
BINS		+=	vcexport.out
VCTOOLSRCS	+=	vcexport.cpp
endif

ifeq ($(BUILD_CPR_CHECK),y)
BINS		+=	cprcheck.out
VCTOOLSRCS	+=	cprcheck.cpp
endif

vcexport.out:LDFLAGS	+=	-L $(VCTOOLSDIR) -lvcsim -lahirvc
$(OPTBINS):CXXFLAGS		+=	-O3

# Need to touch the file as antlr doesn't change the timestamp if the file wasn't changed
%Parser.cpp %Lexer.cpp %Parser.hpp %Lexer.hpp: $(AHIRGRAMDIR)/%.g
	$(ANTLR) $< && touch $@

all:	$(BINS)

libahirvc.a:	$(AHIROBJS) $(PARSEROBJS)
	ar rv $@ $?

$(OBJS):	$(PARSERHDRS)

$(PARSERSRCS) $(PARSERHDRS):	$(VCGRAMMAR)

libvcsim.so:	vcsim.o libahirvc.a
	$(CXX) -shared $^ $(LDFLAGS) -o $@

ifeq ($(BUILD_CPR_CHECK),y)
cprcheck.out:	cprcheck.o libahirvc.a
	$(CXX) $^ $(LDFLAGS) -o $@
endif

vcexport.out:	vcexport.o libvcsim.so libahirvc.a
	$(CXX) $< $(LDFLAGS) -o $@

ifdef XSBDIR
opf.h:	opf.P
	echo [opf]. | xsb > $@
endif

clean:
	rm -f $(OBJS) $(BINS) $(PARSERSRCS) $(PARSERHDRS) $(MISCFILES) $(DFILES)

CONFOPTS	=	USECEP USESEQNO DATUMDBG PIPEDBG OPDBG PNDBG GEN_CPDOT GEN_DPDOT GEN_PETRIDOT GEN_PETRIJSON GEN_PETRIPNML PN_PLACE_CAPACITY_EXCEPTION
CXXFLAGS	+=	$(foreach OPT, $(CONFOPTS), $(if $(filter y, $($(OPT))), -D$(OPT)))
CXXFLAGS	+=	-DWIDEUINTSZ=$(WIDEUINTSZ)

ifeq ($(USECEP),y)
include $(XSBCPPIFDIR)/Makefile.xsbcppif
endif

ifeq ($(USECEP),y)
include $(CEPTOOLDIR)/Makefile.ceptool
endif

include $(PETRISIMUDIR)/Makefile.petrisimu
-include $(DFILES)
