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
        [ ('color','red') ] if self.rel == 'data' else []
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
        for req in self.reqs + self.greqs + self.ftreq:
            srcnode = self.vcir.pn.nodes[req]
            arcobj = DPArc({
                'srcnode' : srcnode,
                'tgtnode' : self,
                'rel' : 'petri'
                })
            self.addIarc(arcobj)
            srcnode.addOarc(arcobj)
        for ack in self.acks + self.gacks + self.ftack:
            tgtnode = self.vcir.pn.nodes[ack]
            arcobj = DPArc({
                'srcnode' : self,
                'tgtnode' : tgtnode,
                'rel' : 'petri'
                })
            tgtnode.addIarc(arcobj)
            self.addOarc(arcobj)
    def __init__(self,nodeid,vcir,props):
        super().__init__(nodeid,vcir,props)

class VcDP:
    def createArcs(self):
        for n in self.nodes.values(): n.createArcs()
    def __init__(self,dpes,vcir):
        self.nodes = { int(id):DPNode(int(id),vcir,dpe) for id,dpe in dpes.items()}
