from vcnodeprops import *
from vcirbase import *

class OpClass: pass

class RotateL(OpClass): pass
class RotateR(OpClass): pass
class ShiftRA(OpClass): pass
class Not(OpClass): pass
class S2S(OpClass): pass
class CCast(OpClass): pass
class Select(OpClass): pass
class Slice(OpClass): pass
class Assign(OpClass): pass
class Branch(OpClass): pass
class Phi(OpClass): pass
class Load(OpClass): pass
class Store(OpClass): pass
class Inport(OpClass): pass
class Outport(OpClass): pass
class Call(OpClass): pass
class Plus(OpClass): pass
class Minus(OpClass): pass
class Mult(OpClass): pass
class Div(OpClass): pass
class And(OpClass): pass
class Or(OpClass): pass
class Xor(OpClass): pass
class Nand(OpClass): pass
class Nor(OpClass): pass
class Xnor(OpClass): pass
class Lt(OpClass): pass
class Le(OpClass): pass
class Gt(OpClass): pass
class Ge(OpClass): pass
class Ne(OpClass): pass
class Eq(OpClass): pass
class Concat(OpClass): pass
class SLt(OpClass): pass
class SLe(OpClass): pass
class SGt(OpClass): pass
class SGe(OpClass): pass
class Bitsel(OpClass): pass
class ShiftL(OpClass): pass
class ShiftR(OpClass): pass

class DPArc(Arc):
    def dotprops(self): return \
        [ ('color','red') ] if self.rel == 'data' else \
        [ ('color','orange') ] if self.rel == 'bind' else \
        [ ('color','magenta') ] if self.rel == 'callack' else []
    def __init__(self,d):
        super().__init__(d)

class DPNode(Node):
    opclss = { c.__name__ for c in OpClass.__subclasses__() }
    def dotprops(self): return [
        ('color','red'),
        ('shape','triangle'),
        ('label',self.idstr() + ':' + self.label)
        ]
    def isDP(self): return True
    def nodeClass(self): return 'DPNode'
    def optype(self): return self.optyp
    def idstr(self): return 'dp_' + str(self.nodeid)
    def createArcs(self):
        for tgtpos,srcinfo in self.dpinps.items():
            srcnode = self.vcir.dp.nodes[srcinfo['id']]
            arcobj = DPArc({
                'srcnode' : srcnode,
                'srcpos' : srcinfo['oppos'],
                'tgtnode' : self,
                'tgtpos' : tgtpos,
                'rel' : 'data'
                })
            self.addIarc(arcobj,False)
            srcnode.addOarc(arcobj,False)
        for tgtpos,srcinfo in self.fpinps.items():
            srcnode = self.vcir.pn.nodes[srcinfo['id']]
            arcobj = DPArc({
                'srcnode' : srcnode,
                'srcpos' : srcinfo['oppos'],
                'tgtnode' : self,
                'tgtpos' : tgtpos,
                'rel' : 'data'
                })
            self.addIarc(arcobj,False)
            srcnode.addOarc(arcobj,False)
        for req in self.reqs + self.greqs:
            srcnode = self.vcir.pn.nodes[req]
            arcobj = DPArc({
                'srcnode' : srcnode,
                'tgtnode' : self,
                'rel' : 'petri'
                })
            self.addIarc(arcobj)
            srcnode.addOarc(arcobj)
        for ack in self.acks + self.gacks:
            tgtnode = self.vcir.pn.nodes[ack]
            arcobj = DPArc({
                'srcnode' : self,
                'tgtnode' : tgtnode,
                'rel' : 'petri'
                })
            tgtnode.addIarc(arcobj)
            self.addOarc(arcobj)
        if self.optyp == 'Call':
            # Call -> Entry : single bind for all inpparams
            tgtnode = self.vcir.pn.nodes[self.callentry]
            width = sum(self.iwidths)
            arcobj = DPArc({
                'srcnode' : self,
                'tgtnode' : tgtnode,
                'rel' : 'bind',
                'width' : width
                })
            tgtnode.addIarc(arcobj)
            self.addOarc(arcobj)
            # Exit -> Call : single bind for all opparams
            srcnode = self.vcir.pn.nodes[self.callexit]
            width = sum(self.owidths)
            arcobj = DPArc({
                'srcnode' : srcnode,
                'tgtnode' : self,
                'rel' : 'bind',
                'width' : width
                })
            self.addIarc(arcobj)
            srcnode.addOarc(arcobj)
            # CallAck -> Call : to trigger latching the result and Call -> CallAck, to ack the same
            acknode = self.vcir.pn.nodes[self.callack]
            ack_call_arc = DPArc({
                'srcnode' : acknode,
                'tgtnode' : self,
                'rel' : 'callack'
                })
            self.addIarc(ack_call_arc)
            acknode.addOarc(ack_call_arc)
            call_ack_arc = DPArc({
                'srcnode' : self,
                'tgtnode' : acknode,
                'rel' : 'callack'
                })
            self.addOarc(call_ack_arc)
            acknode.addIarc(call_ack_arc)
    def __init__(self,nodeid,vcir,props):
        super().__init__(nodeid,vcir,props)

class VcDP:
    def sysInPipes(self): return [ p for p in self.pipereads if p not in self.pipefeeds ]
    def sysOutPipes(self): return [ p for p in self.pipefeeds if p not in self.pipereads ]
    def createArcs(self):
        self.pipereads = {}
        self.pipefeeds = {}
        for n in self.nodes.values():
            n.createArcs()
            feedspipe = getattr( n, 'feedspipe', None )
            if feedspipe != None:
                self.pipefeeds[feedspipe] = self.pipefeeds.get(feedspipe,0) + 1
            readspipe = getattr( n, 'readspipe', None )
            if readspipe != None:
                self.pipereads[readspipe] = self.pipefeeds.get(readspipe,0) + 1
        print('pipereads',self.pipereads)
        print('pipefeeds',self.pipefeeds)
    def __init__(self,dpes,vcir):
        self.nodes = { int(id):DPNode(int(id),vcir,dpe) for id,dpe in dpes.items()}
