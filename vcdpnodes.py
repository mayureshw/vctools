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
    def __init__(self,d):
        super().__init__(d)

class DPNode(Node):
    opclss = { c.__name__ for c in OpClass.__subclasses__() }
    def nodeClass(self): return 'DPNode'
    def optype(self): return self.optyp
    def idstr(self): return 'dp_' + str(self.nodeid)
    def __init__(self,nodeid,vcir,props):
        super().__init__(nodeid,vcir,props)

class VcDP:
    def isPNDPTrans(self,tid): return tid in self.pndpTrans
    def isDPPNTrans(self,tid): return tid in self.dppnTrans
    def getTransSet(self,dpes,keys):
        return { t for dpe in dpes.values() for tk in keys for t in dpe[tk] }
    def __init__(self,dpes,vcir):
        self.nodes = { int(id):DPNode(int(id),vcir,dpe) for id,dpe in dpes.items()}
        pndpkeys = [ 'reqs', 'greqs', 'ftreq' ]
        dppnkeys = [ 'acks', 'gacks', 'ftack' ]
        self.pndpTrans = self.getTransSet( dpes, pndpkeys )
        self.dppnTrans = self.getTransSet( dpes, dppnkeys )

