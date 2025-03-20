from vcnodeprops import *
from vcirbase import *

# Note:
#  - nodeType: Broad classification into just Place and Transition
#  - nodeClass: Further classification based on fanin-fanout structure of a node

class EntryPlace(NodeClass):
    sign = [ isPlace, isEntryPlace ]

class MutexPlace(NodeClass):
    sign = [ isPlace, isMutex ]
    props = [
        ( eq, (fanin,rev_mutex), (fanout,mutex) ),
        ( eq, (fanin,petri),     (fanout,mutex) ),
        ( eq, (fanin,total),     ( mul, (fanout,mutex), 2 ) ),
        ( eq, (fanout,total),    (fanout,mutex) ),
        ( eq, marking,           1 ),
        ]

class PassiveBranchPlace(NodeClass):
    sign  = [ isPlace, isPassiveBranch ]
    props = [
        ( eq, (fanin,rev_passivebranch), (fanout,passivebranch) ),
        ( eq, (fanin,total), ( add, (fanout,passivebranch), (fanin,petri) ) ),
        ( eq, (fanout,total), (fanout,passivebranch) ),
        ]

class BranchPlace(NodeClass):
    sign  = [ isPlace, isBranch ]
    props = [
        ( eq, (fanout,branch), 2 ),
        ( eq, (fanin,petri),   1 ),
        ( eq, (fanout,total),  2 ),
        ( eq, (fanin,total),   1 ),
        ]

class MergePlace(NodeClass):
    sign  = [
        isPlace,
        ( not_, isEntryPlace ),
        ( eq, (fanin,petri),  2 ),
        ( eq, (fanout,petri), 1 ),
        ]
    props = [
        ( eq, (fanin,total),  2 ),
        ( eq, (fanout,total), 1 ),
        ]

class PassThroughPlace(NodeClass):
    sign = [
        isPlace,
        ( not_, isEntryPlace ),
        ( eq, (fanin,petri),  1 ),
        ( eq, (fanout,petri), 1 ),
        ]
    props = [
        ( le, marking, 1 ),
        ]

class VolatileExitPlace(NodeClass):
    sign = [
        isPlace,
        isExitPlace,
        ( eq, (fanin,petri),  0),
        ( eq, (fanout,petri), 0),
        ]
    props = [
        ( le, marking, 1 ),
        ]

class MiscTransition(NodeClass):
    sign = [
        isTransition,
        ( gt, (fanin,petri),  0 ),
        ( gt, (fanout,petri), 0 ),
        ]
    props = [
        ( le, (fanin,mutex),          1 ),
        ( eq, (fanin,mutex),          (fanout,rev_mutex) ),
        ( eq, (fanout,mutex),         0 ),
        ( eq, (fanin,passivebranch),  (fanout,rev_passivebranch) ),
        ( eq, (fanout,passivebranch), 0 ),
        ( le, (fanin,branch),         1 ),
        ( eq, (fanout,branch),        0 ),
        #( le, (fanin,petri),          4 ), # LUT inps - 2 (for FPGAs)
        ( or_, ( eq, (fanin,branch), 0 ), ( eq, (fanin,total), 1 ) ),
        ]

#################################################################################################

class PNArc(Arc):
    def dotprops(self): return \
        [ ('color','green') ] if self.rel == 'mutex' else \
        [ ('color','green'), ('style','dotted') ] if self.rel == 'rev_mutex' else \
        [ ('color','lightblue') ] if self.rel == 'passivebranch' else \
        [ ('color','lightblue'), ('style','dotted') ] if self.rel == 'rev_passivebranch' else []
    def reversedArc(self): return PNArc(self.tgtnode,self.srcnode,{
        'wt'  : self.wt,
        'rel' : 'rev_' + self.rel,
        })
    def _inferRel(self,srcnode): return (
        'mutex' if srcnode.isMutex() else \
        'passivebranch' if srcnode.isPassiveBranch() else \
        'branch' if srcnode.isBranch() else \
        'petri'
        ) if srcnode.isPlace() else \
        'petri'
    def __init__(self,srcnode,tgtnode,props):
        if 'rel' not in props: props['rel'] = self._inferRel(srcnode)
        super().__init__(srcnode,tgtnode,props)

class PNNode(Node):
    def pnmarking(self): return self.marking if self.isPlace() else None
    def pncapacity(self): return self.capacity if self.isPlace() else None
    def nodeClass(self): return self.classname
    def optype(self): return None
    def idstr(self): return 'pn_' + str(self.nodeid)
    def classify(self):
        clss = list( nc for nc in NodeClass.__subclasses__() if nc.checkSign(self) )
        if len(clss) > 1:
            print('MultiClassification',clss,self.nodeinfo())
            return
        elif len(clss) == 0:
            # skip reporting this error if node is not connected anywhere
            if self.fanin('total') != 0 or self.fanout('total') != 0:
                print('NoClassification',self.nodeinfo())
            return
        nodeclass = clss[0]
        self.classname = nodeclass.__name__
        propvios = [ str(propobj) for prop in nodeclass.props for propobj in [Functor.create(prop)] if not propobj.eval(self) ]
        if len(propvios) > 0 :
            for p in propvios:
                print('PROPVIO',self.classname,p,self.nodeinfo())
        # else: print('CLASSOK',self.classname,self.nodeinfo())
    def __init__(self,nodeid,vcir,props):
        super().__init__(nodeid,vcir,props)

class Transition(PNNode):
    def dotprops(self): return [ ( 'shape','rectangle' ), ( 'label', self.idstr() + ':' + self.label ) ]
    def isTransition(self): return True
    def nodeType(self): return 'Transition'
    def isFork(self): return self.fanout('petri') > 1
    def isJoin(self): return self.fanin('petri') > 1
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class Place(PNNode):
    def dotprops(self): return [
        ('color', 'green' if self.isMutex() else \
        'blue' if self.isBranch() else \
        'lightblue' if self.isPassiveBranch() else 'black' ),
        ( 'label', self.idstr() + ':' + self.label )
        ]
    def isPlace(self): return True
    def nodeType(self): return 'Place'
    def isEntryPlace(self): return self.nodeid in self.vcir.module_entries
    def isExitPlace(self): return self.nodeid in self.vcir.module_exits
    def isMutex(self): return self.nodeid in self.vcir.mutexes
    def isBranch(self): return self.nodeid in self.vcir.branches
    def isPassiveBranch(self): return self.nodeid in self.vcir.passive_branches
    def isMerge(self): return self.fanin('petri') > 1
    def isHighCapacity(self): return self.capacity > 1 or self.capacity == 0
    def __init__(self,nodeid,vcir,props):
        super().__init__(nodeid,vcir,props)
        if self.isBranch(): self.iwidths = [1]

class VcPetriNet:
    def isSimuOnlyArc(self,srcid,tgtid): return self.isSimuOnlyNode(srcid) or self.isSimuOnlyNode(tgtid)
    def isSimuOnlyNode(self,nodeid): return nodeid in self.vcir.simu_only
    # classify should be called after all layers of vcir are built
    def classify(self):
        for node in self.nodes.values(): node.classify()
    def __init__(self,pnobj,vcir):
        self.vcir = vcir
        self.places = {
            int(nodeid) : Place(int(nodeid),vcir,props)
            for (nodeid,props) in pnobj['places'].items() if not self.isSimuOnlyNode(int(nodeid))
            }
        self.transitions = {
            int(nodeid) : Transition(int(nodeid),vcir,props)
            for (nodeid,props) in pnobj['transitions'].items() if not self.isSimuOnlyNode(int(nodeid))
            }
        self.nodes = {**self.places,**self.transitions}
        self.rawarcs = pnobj['arcs']
        self.arcs = [] # TODO: Not populated, a rule in vcir.py depends on this, search pn.arcs
        for arc in self.rawarcs:
            srcid = arc['src']
            tgtid = arc['tgt']
            if self.isSimuOnlyArc(srcid,tgtid): continue
            srcnode = self.nodes[ srcid ]
            tgtnode = self.nodes[ tgtid ]
            arcobj = PNArc(srcnode,tgtnode, { 'wt': arc['wt'] })
            if srcnode.isPlace() and arcobj.rel in {'mutex','passivebranch'} : arcobj.reversedArc()

