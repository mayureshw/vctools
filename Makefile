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
PREFIX  ?=  /usr/pkg
else
PREFIX  ?=  /usr/local
endif

INSTALL ?=  install


LDFLAGS	+=	-L$(PREFIX)/lib -lantlr
CXXFLAGS+=	-I$(PREFIX)/include
ANTLR	=	antlr

XSB_VERSION	=	5.0.0
XSBDIR		=	$(PREFIX)/xsb-$(XSB_VERSION)
XSB			=	$(XSBDIR)/bin/xsb

INSTBINDIR  =   $(PREFIX)/bin
INSTLIBDIR  =   $(PREFIX)/lib
INSTINCDIR  =   $(PREFIX)/include
INSTXWAMDIR	=	$(XSBDIR)/lib
INSTEXAMPDIR=	$(PREFIX)/share/examples/ahir/vcsim

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
GENHDRS		=	$(PARSERHDRS) vcsimconf.h
INSTHDRS	=	vcsim.h vcsimconf.h datum.h opf.h $(CEPTOOLDIR)/stateif.h $(CEPTOOLDIR)/exprf.h
INSTLIBS	=	libvcsim.so
INSTEXAMPS	=	examples/Makefile $(wildcard examples/*.cpp) $(wildcard examples/*.aa) $(wildcard examples/*.uprops)

CXXFLAGS	+=	$(addprefix -I,$(AHIRHDRDIRS)) -I. -I$(AHIRDIR)/v2/libAhirV2/src
CXXFLAGS	+=	-std=c++17 -fPIC -MMD -MP -g
VPATH		+=	$(AHIRSRCDIR)

BINS		=	libahirvc.a libvcsim.so
EXCLUDEOPT	=	libahirvc.a
OPTBINS		=	$(filter-out $(EXCLUDEOPT), $(BINS))

ifeq ($(USECEP),y)
BINS		+=	vcexport.out
INSTBINS	+=	vcexport.out
VCTOOLSRCS	+=	vcexport.cpp
PFILES		=	$(wildcard $(CEPTOOLDIR)/*.P) $(wildcard *.P)
XWAMFILES	=	$(PFILES:.P=.xwam)
endif

ifeq ($(BUILD_CPR_CHECK),y)
BINS		+=	cprcheck.out
INSTBINS	+=	cprcheck.out
VCTOOLSRCS	+=	cprcheck.cpp
endif

vcexport.out:LDFLAGS	+=	-lvcsim -L. -lahirvc
$(OPTBINS):CXXFLAGS		+=	-O3

# Need to touch the file as antlr doesn't change the timestamp if the file wasn't changed
%Parser.cpp %Lexer.cpp %Parser.hpp %Lexer.hpp: $(AHIRGRAMDIR)/%.g
	$(ANTLR) $< && touch $@

%.xwam:	%.P
	$(XSB) -e "compile('$<'),halt."

all:	$(BINS) $(XWAMFILES)

libahirvc.a:	$(AHIROBJS) $(PARSEROBJS)
	ar rv $@ $?

$(OBJS):	$(GENHDRS)

$(PARSERSRCS) $(PARSERHDRS):	$(VCGRAMMAR)

libvcsim.so:	vcsim.o libahirvc.a
	$(CXX) -shared $^ $(LDFLAGS) -o $@

ifeq ($(BUILD_CPR_CHECK),y)
cprcheck.out:	cprcheck.o libahirvc.a
	$(CXX) $^ $(LDFLAGS) -o $@
endif

vcexport.out:	vcexport.o libvcsim.so libahirvc.a
	$(CXX) $< $(LDFLAGS) -o $@

# opf.h is committed, hence this generation is required only for vcsim developers if opf.P changes
ifdef GENOPF
opf.h:	opf.P
	echo [opf]. | $(XSB) > $@
endif

clean:
	rm -f $(OBJS) $(BINS) $(PARSERSRCS) $(GENHDRS) $(MISCFILES) $(DFILES)

CONFOPTS	=	USECEP USESEQNO DATUMDBG PIPEDBG OPDBG PNDBG GEN_CPDOT GEN_DPDOT GEN_PETRIDOT GEN_PETRIJSON GEN_PETRIPNML PN_PLACE_CAPACITY_EXCEPTION
SELECTOPTS	=	$(foreach OPT, $(CONFOPTS),$(if $(filter y,$($(OPT))),$(OPT)))

vcsimconf.h	:	Makefile.conf
	@echo "Generating $@"
	@( echo "#ifndef _VCSIMCONF_H"; \
	echo "#define _VCSIMCONF_H"; \
	echo "#define WIDEUINTSZ $(WIDEUINTSZ)"; \
	$(foreach O,$(SELECTOPTS),echo "#define $(O)";) \
	echo "#endif" \
	) > $@

install:	all
	$(INSTALL) -d -m 755 $(DESTDIR)$(INSTBINDIR) $(DESTDIR)$(INSTLIBDIR) $(DESTDIR)$(INSTINCDIR) $(DESTDIR)$(INSTXWAMDIR) $(DESTDIR)$(INSTEXAMPDIR)
	$(INSTALL) -m 755 $(INSTBINS) $(DESTDIR)$(INSTBINDIR)
	$(INSTALL) -m 755 $(INSTLIBS) $(DESTDIR)$(INSTLIBDIR)
	$(INSTALL) -m 644 $(INSTHDRS) $(DESTDIR)$(INSTINCDIR)
	$(INSTALL) -m 644 $(XWAMFILES) $(DESTDIR)$(INSTXWAMDIR)
	$(INSTALL) -m 644 $(INSTEXAMPS) $(DESTDIR)$(INSTEXAMPDIR)

ifeq ($(USECEP),y)
include $(XSBCPPIFDIR)/Makefile.xsbcppif
endif

ifeq ($(USECEP),y)
include $(CEPTOOLDIR)/Makefile.ceptool
endif

include $(PETRISIMUDIR)/Makefile.petrisimu
-include $(DFILES)
