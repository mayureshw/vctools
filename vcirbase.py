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
    _arcrels = [ 'petri', 'mutex', 'passivebranch', 'branch', 'pndp' ]
    _rev_arcrels = [ 'rev_mutex', 'rev_passivebranch' ]
    _metric_arcrels = [ 'total' ]
    @classmethod
    def basic_arcrels(cls): return cls._arcrels
    @classmethod
    def rev_arcrels(cls): return cls._rev_arcrels
    @classmethod
    def basic_and_rev_arcrels(cls): return cls._arcrels + cls._rev_arcrels
    @classmethod
    def all_arcrels_with_metrics(cls): return cls.basic_and_rev_arcrels() + cls._metric_arcrels + cls._datarels
    @classmethod
    def datarels(cls): return cls._datarels
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
        self.__dict__.update(props)

class Arc:
    def __init__(self,d):
        self.__dict__.update(d)

