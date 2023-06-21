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
    datarels = { 'data' }
    fixedWidthDataRels = { 'bind' }
    controlrels = { 'petri', 'mutex', 'passivebranch', 'branch', 'rev_mutex', 'rev_passivebranch' }
    metricrels = { 'total' }
    @classmethod
    def all_arcrels_with_metrics(cls): return cls.controlrels.union(cls.metricrels, cls.datarels, cls.fixedWidthDataRels)
    def dotprops(self) : return []
    def isDP(self): return False
    def isPlace(self): return False
    def isTransition(self): return False
    def onreset(self): return 0
    def iarcsExclMetrics(self): return [ a for r,arcs in self.iarcs.items() if r not in self.metricrels for a in arcs ]
    def nonDataIarcs(self): return [
        ( rel, self.idstr(), arc.tgtpos, arc.srcnode.idstr(), arc.srcpos )
            for rel in self.controlrels.union(self.fixedWidthDataRels) for arc in self.iarcs[rel]
        ]
    def dataIarcs(self): return [
        ( rel, self.idstr(), int(arc.tgtpos), arc.srcnode.idstr(), int(arc.srcpos) )
            for rel in self.datarels for arc in self.iarcs[rel]
        ]
    def constvals(self): return [
        ( self.idstr(), int(pos), int(val) )
        for pos,val in self.constinps.items()
        ]
    # Note: fanin/out and in/out arc count are in general different properties
    # however in case of Petri nets, this difference is relevant only for forks
    # Hence, for simplicity we treat fanin/out and arc in/out count as same and we
    # replicate output singal on all ports for fork transitions. However for data path
    # we treat these notions as different.
    def fanin(self,rel): return len(self.iwidths if rel == 'data' else self.iarcs[rel])
    def fanout(self,rel): return len(self.owidths if rel == 'data' else self.oarcs[rel])
    def inpwidth(self,rel,index): return (
            self.iwidths[index] if index < len(self.iwidths) else 0
        ) if rel == 'data' else (
            self.iarcs['bind'][index].width if index < len(self.iarcs['bind']) else 0
        ) if rel == 'bind' else None
    def opwidth(self,rel,index): return (
            self.owidths[index] if index < len(self.owidths) else 0
        ) if rel == 'data' else (
            self.oarcs['bind'][index].width if index < len(self.oarcs['bind']) else 0
        ) if rel == 'bind' else None
    def _widths2offset(self,ws): return [ (l+w-1,l) for i,w in enumerate(ws) for l in [sum(ws[:i])] ]
    def ioffsets(self): return self._widths2offset(self.iwidths)
    def ooffsets(self): return self._widths2offset(self.owidths)
    def successors(self,rel): return [ a.tgtnode for a in self.oarcs[rel] ]
    def predecessors(self,rel): return [ a.srcnode for a in self.iarcs[rel] ]
    def addOarc(self,arc,inferSrcpos=True):
        if inferSrcpos: arc.srcpos = len(self.oarcs[arc.rel])
        self.oarcs[arc.rel] += [arc]
        self.oarcs['total'] += [arc]
    def addIarc(self,arc,inferTgtpos=True):
        if inferTgtpos: arc.tgtpos = len(self.iarcs[arc.rel])
        self.iarcs[arc.rel] += [arc]
        self.iarcs['total'] += [arc]
    def nodeinfo(self): return str({
        **{
        rel : ( self.fanin(rel), self.fanout(rel) )
        for rel in self.all_arcrels_with_metrics() if self.fanin(rel) > 0 or self.fanout(rel) > 0
        },
        **{'nodeid' : self.nodeid},
        **{ 'typ' : self.nodeType() },
        })
    def __init__(self,nodeid,vcir,props):
        self.vcir = vcir
        self.oarcs = { r:[] for r in self.all_arcrels_with_metrics() }
        self.iarcs = { r:[] for r in self.all_arcrels_with_metrics() }
        self.nodeid = nodeid
        self.classname = None # set by classify
        # properties of dpnodes, initialized to empty for non dpnodes
        self.iwidths = []
        self.owidths = []
        self.constinps = {}

        self.__dict__.update(props)

class Arc:
    def dotprops(self): return []
    def __init__(self,d):
        self.__dict__.update(d)

