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
    def fanin(self,rel): return len(self.iarcs[rel])
    def fanout(self,rel): return len(self.oarcs[rel])
    def successors(self,rel): return [ a.tgtnode for a in self.oarcs[rel] ]
    def predecessors(self,rel): return [ a.srcnode for a in self.iarcs[rel] ]
    def addOarc(self,arc):
        arc.srcpos = len(self.oarcs[arc.rel])
        self.oarcs[arc.rel] += [arc]
        self.oarcs['total'] += [arc]
    def addIarc(self,arc):
        arc.tgtpos = len(self.iarcs[arc.rel])
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
        # iwidths and owidths come from props for DPNodes
        self.iwidths = []
        self.owidths = []
        self.__dict__.update(props)

class Arc:
    def __init__(self,d):
        self.__dict__.update(d)

