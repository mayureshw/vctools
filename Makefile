OS=$(shell uname -s)
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
VCTOOLSRCS	=	vc2pn.cpp cprcheck.cpp vc2p.cpp
PARSERHDRS	=	$(PARSERSRCS:.cpp=.hpp) vcParserTokenTypes.hpp
MISCFILES	=	vcParserTokenTypes.txt
AHIROBJS	=	$(notdir $(AHIRSRCS:.cpp=.o))
PARSEROBJS	=	$(PARSERSRCS:.cpp=.o)
VCTOOLOBJS	=	$(VCTOOLSRCS:.cpp=.o)
DFILES		=	$(notdir $(AHIRSRCS:.cpp=.d)) $(PARSERSRCS:.cpp=.d) $(VCTOOLSRCS:.cpp=.d)

CXXFLAGS	+=	$(addprefix -I,$(AHIRHDRDIRS)) -I.
CXXFLAGS	+=	-fPIC -g -O3
VPATH		+=	$(AHIRSRCDIR)

BINS	=	libahirvc.a $(VCTOOLSDIR)/libvcsim.so cprcheck.out vc2p.out

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

$(VCTOOLSDIR)/libvcsim.so:	vc2pn.o libahirvc.a
	$(CXX) -shared $^ $(LDFLAGS) -o $@

cprcheck.out:	cprcheck.o libahirvc.a
	$(CXX) $^ $(LDFLAGS) -o $@

vc2p.out:	vc2p.o $(VCTOOLSDIR)/libvcsim.so libahirvc.a
	$(CXX) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(AHIROBJS) $(PARSEROBJS) $(VCTOOLOBJS) $(BINS) $(PARSERSRCS) $(PARSERHDRS) $(MISCFILES)

include $(CEPTOOLDIR)/Makefile.ceptool
include $(PETRISIMUDIR)/Makefile.petrisimu
include $(DFILES)
