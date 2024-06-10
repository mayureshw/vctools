from vcnodeprops import *
from vcirbase import *

# Note:
#  - nodeType: Broad classification into just Place and Transition
#  - nodeClass: Further classification based on fanin-fanout structure of a node

class EntryPlace(NodeClass):
    sign = [
lambda n: f(n,'isPlace'),
lambda n: f(n,'isEntryPlace'),
        ]

class MutexPlace(NodeClass):
    sign = [
lambda n: f(n,'isPlace'),
lambda n: f(n,'isMutex'),
        ]
    props = [
lambda n: e( v(n,'rev_mutex','fanin'), eq, v(n,'mutex','fanout') ),
lambda n: e( v(n,'petri','fanin'),     eq, v(n,'mutex','fanout') ),
lambda n: e( v(n,'total','fanin'),     eq, e( v(n,'mutex','fanout'), mul, c(2) ) ),
lambda n: e( v(n,'total','fanout'),    eq, v(n,'mutex','fanout') ),
        ]

class PassiveBranchPlace(NodeClass):
    sign  = [
lambda n: f(n,'isPlace'),
lambda n: f(n,'isPassiveBranch'),
        ]
    props = [
lambda n: e( v(n,'rev_passivebranch','fanin'), eq, v(n,'passivebranch','fanout') ),
        ]

class BranchPlace(NodeClass):
    sign  = [
lambda n: f(n,'isPlace'),
lambda n: f(n,'isBranch'),
        ]
    props = [
lambda n: e( v(n,'branch','fanout'), eq, c(2) ),
lambda n: e( v(n,'petri','fanin'),   eq, c(1) ),
lambda n: e( v(n,'total','fanin'),   eq, c(1) ),
lambda n: e( v(n,'total','fanout'),  eq, c(2) ),
        ]

class MergePlace(NodeClass):
    sign  = [
lambda n: f(n,'isPlace'),
lambda n: e( v(n,'petri','fanin'),  eq, c(2) ),
lambda n: e( v(n,'petri','fanout'), eq, c(1) ),
lambda n: e( not_, f(n,'isEntryPlace') ),
        ]
    props = [
lambda n: e( v(n,'total','fanin'),  eq, c(2) ),
lambda n: e( v(n,'total','fanout'), eq, c(1) ),
        ]

class PassThroughPlace(NodeClass):
    sign = [
lambda n: f(n,'isPlace'),
lambda n: e( v(n,'petri','fanin'),  eq, c(1) ),
lambda n: e( v(n,'petri','fanout'), eq, c(1) ),
lambda n: e( not_, f(n,'isEntryPlace') ),
        ]

class MiscTransition(NodeClass):
    sign = [
lambda n: f(n,'isTransition')
        ]
    props = [
lambda n: e( v(n,'mutex','fanin'), le, c(1) ),
lambda n: e( v(n,'mutex','fanin'), eq, v(n,'rev_mutex','fanout') ),
lambda n: e( v(n,'mutex','fanout'), eq, c(0) ),
lambda n: e( v(n,'passivebranch','fanin'), eq, v(n,'rev_passivebranch','fanout') ),
lambda n: e( v(n,'passivebranch','fanout'), eq, c(0) ),
lambda n: e( v(n,'branch','fanin'), le, c(1) ),
lambda n: e( e( v(n,'branch','fanin'), eq, c(0) ), or_, e( v(n,'total','fanin') ,eq, c(1) ) ),
lambda n: e( v(n,'branch','fanout'), eq, c(0) ),
lambda n: e( v(n,'petri','fanin'), le, c(4) ), # Current limitation in vhdl layer
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
    def onreset(self): return self.marking if self.isPlace() else 0
    def nodeClass(self): return self.classname
    def optype(self): return None
    def idstr(self): return 'pn_' + str(self.nodeid)
    def classify(self):
        clss = list( nc for nc in NodeClass.__subclasses__() if nc.checkSign(self) )
        if len(clss) > 1:
            print('MultiClassification',clss,self.nodeinfo())
            return
        elif len(clss) == 0:
            print('NoClassification',self.nodeinfo())
            return
        nodeclass = clss[0]
        self.classname = nodeclass.__name__
        propvios = [ str(propfn) for prop in nodeclass.props for propfn in [prop(self)] if not propfn.eval() ]
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
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

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

