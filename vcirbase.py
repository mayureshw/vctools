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
    _arcrels = [ 'petri', 'mutex', 'passivebranch', 'branch' ]
    _rev_arcrels = [ 'rev_mutex', 'rev_passivebranch' ]
    _metric_arcrels = [ 'total' ]
    @classmethod
    def basic_arcrels(cls): return cls._arcrels
    @classmethod
    def rev_arcrels(cls): return cls._rev_arcrels
    @classmethod
    def basic_and_rev_arcrels(cls): return cls._arcrels + cls._rev_arcrels
    @classmethod
    def all_arcrels_with_metrics(cls): return cls.basic_and_rev_arcrels() + cls._metric_arcrels

class Arc: pass

