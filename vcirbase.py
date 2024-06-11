# This module contains base classes for both pn and dpe irs. This abstract view
# of pn and dp forms the basis for vhdl code generation.

class NodeClass:
    props = []
    sign = []
    @classmethod
    def checkSign(cls,o): return all( s(o).eval() for s in cls.sign )
    @classmethod
    def printProps(cls):
        for nodecls in cls.__subclasses__():
            print(nodecls.__name__,':')
            print('\t','Signature:')
            for s in nodecls.sign:
                print('\t\t',s(None))
            print('\t','Properties:')
            for propfn in nodecls.props:
                print('\t\t',propfn(None))

class Node:
    maxdataports = 5
    controlrels = { 'petri', 'mutex', 'passivebranch', 'branch', 'callack', 'rev_mutex', 'rev_passivebranch' }
    def _mapstr(self,io,pos):
        mapstr = self.idstr()+'_'+io+'map('+str(pos)+')'
        return mapstr + '(0) downto ' + mapstr + '(1)'
    def _sliceStr(self,io,rel,pos): return self._mapstr(io,pos) \
        if rel == 'data' else str(pos)
    def _iostr(self,io,rel,pos): return self.idstr()+'_' +io+'.'+rel+'('+\
        self._sliceStr(io,rel,pos)+')'
    def istr(self,rel,pos): return self._iostr('i',rel,pos)
    def ostr(self,rel,pos): return self._iostr('o',rel,pos)
    def dotprops(self) : return []
    def nodeinfo(self): return str({
        **{
        rel : ( self.fanin(rel), self.fanout(rel) )
        for rel in set(self.iarcs.keys()).union(self.oarcs.keys())
        },
        **{'nodeid' : self.nodeid},
        **{ 'typ' : self.nodeType() },
        })
    def isDP(self): return False
    def isSys(self): return False
    def isPlace(self): return False
    def isTransition(self): return False
    def isPort(self): return False
    def onreset(self): return 0
    def iArcs(self): return [ a for r,arcs in self.iarcs.items() for a in arcs ]
    def constvals(self): return [ (int(pos),int(val))
        for pos,val in self.constinps.items()
        ]
    def addOarc(self,arc): self.oarcs.setdefault(arc.rel,[]).append(arc)
    def addIarc(self,arc): self.iarcs.setdefault(arc.rel,[]).append(arc)
    # Note: fanin/out and in/out arc count are in general different properties
    # however in case of Petri nets, this difference is relevant only for forks
    # Hence, for simplicity we treat fanin/out and arc in/out count as same and we
    # replicate output singal on all ports for fork transitions. However for data path
    # we treat these notions as different.
    def fanin(self,rel): return sum( len(a) for a in self.iarcs.values() ) \
        if rel == 'total' else len(
            self.iwidths if rel == 'data' else
            self.iarcs.get(rel,[]))
    def fanout(self,rel): return sum( len(a) for a in self.oarcs.values() ) \
        if rel == 'total' else len(
            self.owidths if rel == 'data' else
            self.oarcs.get(rel,[]))
    def iarcnt(self,rel): return len( self.iarcs.get(rel,[]) )
    def oarcnt(self,rel): return len( self.oarcs.get(rel,[]) )
    def inpwidth(self,rel,index): return (
            self.iwidths[index] if index < len(self.iwidths) else 0
        ) if rel == 'data' else (
            self.iarcs['bind'][index].width if index < len(self.iarcs.get('bind',[])) else 0
        ) if rel == 'bind' else None
    def opwidth(self,rel,index): return (
            self.owidths[index] if index < len(self.owidths) else 0
        ) if rel == 'data' else (
            self.oarcs['bind'][index].width if index < len(self.oarcs.get('bind',[])) else 0
        ) if rel == 'bind' else None
    def _widths2offset(self,ws): return [ (l+w-1,l) for i,w in enumerate(ws) for l in [sum(ws[:i])] ]
    def ioffsets(self): return self._widths2offset(self.iwidths)
    def ooffsets(self): return self._widths2offset(self.owidths)
    def successors(self,rel): return [ a.tgtnode for a in self.oarcs[rel] ]
    def predecessors(self,rel): return [ a.srcnode for a in self.iarcs[rel] ]
    def __init__(self,nodeid,vcir,props):
        self.vcir = vcir
        self.oarcs = {}
        self.iarcs = {}
        self.nodeid = nodeid
        self.classname = None # set by classify
        # properties of dpnodes, initialized to empty for non dpnodes
        self.iwidths = []
        self.owidths = []
        self.constinps = {}
        self.__dict__.update(props)

# Note: rel identifies the arc type, but it does not reflect in the class
# hierarchy, because in most cases rel needs to be inferred
class Arc:
    def conn(self): return (self.tgtnode.istr(self.rel,self.tgtpos),
        self.srcnode.ostr(self.rel,self.srcpos))
    def __init__(self,srcnode,tgtnode,props):
        self.srcnode = srcnode
        self.tgtnode = tgtnode
        # Default props (overridden by props if present in props)
        self.srcpos = srcnode.oarcnt(props['rel'])
        self.tgtpos = tgtnode.iarcnt(props['rel'])
        self.wt = 1
        self.__dict__.update(props)
        srcnode.addOarc(self)
        tgtnode.addIarc(self)
