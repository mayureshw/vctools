from vcnodeprops import *
from vcirbase import *

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
lambda n: e( v(n,'total','fanin'),             eq, e( v(n,'passivebranch','fanout'), add, v(n,'petri','fanin') ) ),
lambda n: e( v(n,'total','fanout'),            eq, v(n,'passivebranch','fanout') ),
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
        ]
    props = [
lambda n: e( v(n,'total','fanin'),  eq, c(2) ),
lambda n: e( v(n,'total','fanout'), eq, c(1) ),
        ]

class PassThroughPlace(NodeClass):
    sign = [
lambda n: f(n,'isPlace'),
lambda n: e( v(n,'total','fanin'),  eq, c(1) ),
lambda n: e( v(n,'total','fanout'), eq, c(1) ),
lambda n: e( v(n,'petri','fanin'),  eq, c(1) ),
lambda n: e( v(n,'petri','fanout'), eq, c(1) ),
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
    def reversedArc(self): return PNArc({
        'srcnode'   : self.tgtnode,
        'tgtnode'   : self.srcnode,
        'wt'        : self.wt,
        'rel'       : 'rev_' + self.rel,
        })
    def inferRel(self): return (
        'mutex' if self.srcnode.isMutex() else \
        'passivebranch' if self.srcnode.isPassiveBranch() else \
        'branch' if self.srcnode.isBranch() else \
        'petri'
        ) if self.srcnode.isPlace() else \
        'petri'
    def __init__(self,d):
        self.__dict__.update(d)
        if 'rel' not in self.__dict__: self.rel = self.inferRel()

class PNNode(Node):
    arcrels = [ 'petri', 'mutex', 'passivebranch', 'branch', 'total', 'rev_mutex', 'rev_passivebranch' ]
    def nodeClass(self): return self.classname
    def isPlace(self): return False
    def isTransition(self): return False
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
        for rel in self.arcrels if self.fanin(rel) > 0 or self.fanout(rel) > 0
        },
        **{'nodeid' : self.nodeid},
        **{ 'typ' : self.nodeType() },
        })
    def processValidations(self,vs):
        for msg,valf in vs:
            if valf(): print(msg, self.nodeinfo(), '\n')
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
        self.vcir = vcir
        self.oarcs = { r:[] for r in self.arcrels }
        self.iarcs = { r:[] for r in self.arcrels }
        self.nodeid = nodeid
        self.classname = None # set by classify
        self.__dict__.update(props)

class Transition(PNNode):
    def isTransition(self): return True
    def nodeType(self): return 'Transition'
    def isFork(self): return self.fanout('petri') > 1
    def isJoin(self): return self.fanin('petri') > 1
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class Place(PNNode):
    def isPlace(self): return True
    def nodeType(self): return 'Place'
    def isMutex(self): return self.nodeid in self.vcir.mutexes
    def isBranch(self): return self.nodeid in self.vcir.branches
    def isPassiveBranch(self): return self.nodeid in self.vcir.passive_branches
    def isMerge(self): return self.fanin('petri') > 1
    def isHighCapacity(self): return self.capacity > 1 or self.capacity == 0
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

