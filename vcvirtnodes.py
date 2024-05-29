from vcirbase import *

# Nodes that represent the environment (these neither come from DP nor from PN,
# but may build arcs with DP/PN nodes)

class VirtArc(Arc):
    def dotprops(self): return [
        ('style','dashed'),
        ('color','gray'),
        ]
    def __init__(self,d): super().__init__(d)

class VirtNode(Node):
    def dotprops(self): return [ ('color','gray') ]
    def optype(self): return None
    def isVirt(self): return True
    def idstr(self): return 'virt_' + str(self.nodeid)
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VirtCPNode(VirtNode):
    def nodeClass(self): return 'VirtCPNode'
    def createArcs(self):
        # similar to vcdpnodes handling of if self.optyp == 'Call'
        # But no callacks are needed for virtual CP

        # Call -> Entry : single bind for all inpparams
        tgtnode = self.vcir.pn.nodes[self.callentry]
        width = sum(self.iwidths)
        arcobj = VirtArc({
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
        arcobj = VirtArc({
            'srcnode' : srcnode,
            'tgtnode' : self,
            'rel' : 'bind',
            'width' : width
            })
        self.addIarc(arcobj)
        srcnode.addOarc(arcobj)

    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VCVirtDP:
    def createArcs(self):
        for n in self.nodes.values(): n.createArcs()
    def __init__(self,vcir):
        nodeid = 0
        self.nodes = {}
        # create virtual calls for non-daemon non-called modules
        for en in vcir.nonCalledNonDaemonEns():
            node = VirtCPNode( nodeid, vcir, {
                'callentry' : en,
                'callexit' : vcir.module_entries[en]['exit'],
                } )
            self.nodes[nodeid] = node
            nodeid = nodeid + 1
