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
        [ ('color','magenta') ] if self.rel == 'dpsync' else []
    def __init__(self,srcnode,tgtnode,props):
        super().__init__(srcnode,tgtnode,props)
        # if either src or tgt node is a SysNode, the arc doesn't originate
        # from json input and is not represented by iwidths and owidths of the
        # node, so we add it here.
        if self.rel == 'data' and ( srcnode.isSys() or tgtnode.isSys() ):
            srcnode.owidths.append(self.width)
            tgtnode.iwidths.append(self.width)

class DPNode(Node):
    # Operators that do not need AHIR split protocol interconnect
    # (vcInterconnect) wrapped around them by default.
    nonInterconnectOps = {
        'Inport', 'Outport'
        }
    def dotprops(self): return [
        ('color','red'),
        ('shape','triangle'),
        ('label',self.idstr() + ':' + self.label)
        ]
    def isDP(self): return True
    def nodeClass(self): return 'DPNode' if self.optyp not in self.nonInterconnectOps else self.optyp
    def optype(self): return self.optyp
    def idstr(self): return 'dp_' + str(self.nodeid)
    def sreq(self): return self.vcir.pn.nodes[ self.reqs[0] ]
    def sack(self): return self.vcir.pn.nodes[ self.acks[0] ]
    def ureq(self): return self.vcir.pn.nodes[ self.reqs[1] ]
    def uack(self): return self.vcir.pn.nodes[ self.acks[1] ]
    def createArcs(self):
        feedspipe = getattr(self,'feedspipe',None)
        if feedspipe != None: self.vcir.dp.addPipeFeed(feedspipe,self)
        readspipe = getattr(self,'readspipe',None)
        if readspipe != None: self.vcir.dp.addPipeRead(readspipe,self)
        loads = getattr(self,'loads',None)
        if loads != None: self.vcir.dp.addLoads(loads,self)
        stores = getattr(self,'stores',None)
        if stores != None: self.vcir.dp.addStores(stores,self)

        for tgtpos,srcinfo in self.dpinps.items():
            srcnode = self.vcir.dp.nodes[srcinfo['id']]
            DPArc(srcnode,self,{
                'srcpos'  : srcinfo['oppos'],
                'tgtpos'  : tgtpos,
                'rel'     : 'data'
                })
        for tgtpos,srcinfo in self.fpinps.items():
            srcnode = self.vcir.pn.nodes[srcinfo['id']]
            DPArc(srcnode,self,{
                'srcpos'  : srcinfo['oppos'],
                'tgtpos'  : tgtpos,
                'rel'     : 'data'
                })
        for req in self.reqs + self.greqs:
            srcnode = self.vcir.pn.nodes[req]
            DPArc(srcnode,self,{'rel' : 'petri'})
        for ack in self.acks + self.gacks:
            tgtnode = self.vcir.pn.nodes[ack]
            DPArc(self,tgtnode,{'rel' : 'petri'})
        if self.optyp == 'Call':
            # Call -> Entry : single bind for all inpparams
            tgtnode = self.vcir.pn.nodes[self.callentry]
            DPArc(self,tgtnode,{ 'rel': 'bind', 'width': sum(self.iwidths) })
            # Exit -> Call : single bind for all opparams
            srcnode = self.vcir.pn.nodes[self.callexit]
            DPArc(srcnode,self,{ 'rel': 'bind', 'width': sum(self.owidths) })
            # CallAck -> Call : to trigger latching the result and Call -> CallAck, to ack the same
            acknode = self.uack()
            DPArc(acknode,self,{ 'rel': 'dpsync' })
            DPArc(self,acknode,{ 'rel': 'dpsync' })
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VcDP:
    def addPipeFeed(self,pipe,node): self.pipefeeds.setdefault(pipe,[]).append(node)
    def addPipeRead(self,pipe,node): self.pipereads.setdefault(pipe,[]).append(node)
    def addLoads(self,store,node): self.storeloads.setdefault(store,[]).append(node)
    def addStores(self,store,node): self.storestores.setdefault(store,[]).append(node)
    def sysInPipes(self): return [ p for p in self.pipereads if p not in self.pipefeeds ]
    def sysOutPipes(self): return [ p for p in self.pipefeeds if p not in self.pipereads ]
    def createArcs(self):
        for n in self.nodes.values(): n.createArcs()
    def __init__(self,dpes,vcir):
        self.pipereads = {}
        self.pipefeeds = {}
        self.storeloads = {}
        self.storestores = {}
        self.nodes = { int(id):DPNode(int(id),vcir,dpe) for id,dpe in dpes.items()}
