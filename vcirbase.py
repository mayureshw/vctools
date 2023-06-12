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
    _datarels = [ 'data' ]
    _controlrels = [ 'petri', 'mutex', 'passivebranch', 'branch', 'rev_mutex', 'rev_passivebranch', 'pndp' ]
    _metricrels = [ 'total' ]
    @classmethod
    def all_arcrels_with_metrics(cls): return cls._controlrels + cls._metricrels + cls._datarels
    def iarcrels(self): return [
        ( rel, self.idstr(), arc.tgtpos, rel, arc.srcnode.idstr(), arc.srcpos )
            for rel in self._controlrels for arc in self.iarcs[rel]
        ] + [
        ( 'data'+str(arc.tgtpos), self.idstr(), None, 'data'+str(arc.srcpos), arc.srcnode.idstr(), None )
            for arc in self.iarcs['data']
        ]
    def constvals(self): return [
        ( self.idstr(), 'data' + str(pos), int(val) )
        for pos,val in self.constinps.items()
        ]
    def portwidths(self): return [
        ( self.idstr(), rel, 'in', self.fanin(rel) )
            for rel in self._controlrels if self.fanin(rel) > 0
        ] + [
        ( self.idstr(), rel, 'out', self.fanout(rel) )
            for rel in self._controlrels if self.fanout(rel) > 0
        ] + [
        ( self.idstr(), 'data' + str(i), 'in', w )
            for i,w in enumerate(self.iwidths)
        ] + [
        ( self.idstr(), 'data' + str(i), 'out', w )
            for i,w in enumerate(self.owidths)
        ]
    # Note: fanin/out and in/out arc count are in general different properties
    # however in case of Petri nets, this difference is relevant only for forks
    # Hence, for simplicity we treat fanin/out and arc in/out count as same and we
    # replicate output singal on all ports for fork transitions. However for data path
    # we treat these notions as different.
    def fanin(self,rel): return len(self.iwidths if rel == 'data' else self.iarcs[rel])
    def fanout(self,rel): return len(self.owidths if rel == 'data' else self.oarcs[rel])
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
    def __init__(self,d):
        self.__dict__.update(d)

