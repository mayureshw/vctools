class NodeClass :
    props = []
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

