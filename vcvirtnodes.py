from vcirbase import *
from vcpnnodes import *
from vcdpnodes import *

# Nodes that represent the environment (these neither come from DP nor from PN,
# but may build arcs with DP/PN nodes)

# This module does not need custom arc type. It reuses PN and DP arcs

class VirtNode(Node):
    def dotprops(self): return [ ('color','gray') ]
    def optype(self): return None
    def nodeClass(self): return 'VirtNode'
    def isVirt(self): return True
    def idstr(self): return 'virt_' + str(self.nodeid)
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VirtCPNode(VirtNode):
    def createArcs(self):
        # similar to vcdpnodes handling of if self.optyp == 'Call'
        # But no callacks are needed for virtual CP

        # Call -> Entry : bind and Petri arcs
        tgtnode = self.vcir.pn.nodes[self.callentry]
        width = sum(tgtnode.owidths)
        self.owidths = [ width ]
        arcobj = DPArc({
            'srcnode' : self,
            'tgtnode' : tgtnode,
            'rel'     : 'bind',
            'width'   : width
            })
        tgtnode.addIarc(arcobj)
        self.addOarc(arcobj)

        arcobj = PNArc({
            'srcnode'   : self,
            'tgtnode'   : tgtnode,
            'wt'        : 1,
            })
        tgtnode.addIarc(arcobj)
        self.addOarc(arcobj)

        # Call -> SysExit data and Petri arc # TODO check if tgtpos/srcpos is needed (compare with real CP)
        tgtnode = self.vcir.virtdp.sysExitNode
        arcobj = DPArc({
            'srcnode' : self,
            'tgtnode' : tgtnode,
            'rel'     : 'data',
            })
        tgtnode.addIarc(arcobj)
        self.addOarc(arcobj)

        # Exit -> Call : bind and Petri arcs
        srcnode = self.vcir.pn.nodes[self.callexit]
        width = sum(srcnode.iwidths)
        self.iwidths = [ width ]
        arcobj = DPArc({
            'srcnode' : srcnode,
            'tgtnode' : self,
            'rel' : 'bind',
            'width' : width
            })
        self.addIarc(arcobj)
        srcnode.addOarc(arcobj)

        arcobj = PNArc({
            'srcnode'   : srcnode,
            'tgtnode'   : self,
            'wt'        : 1,
            })
        self.addIarc(arcobj)
        srcnode.addOarc(arcobj)

        # SysEntry -> Call data and Petri arc # TODO check if tgtpos/srcpos is needed (compare with real CP)
        srcnode = self.vcir.virtdp.sysEntryNode
        arcobj = DPArc({
            'srcnode' : srcnode,
            'tgtnode' : self,
            'rel' : 'data',
            })
        self.addIarc(arcobj)
        srcnode.addOarc(arcobj)

    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VirtSysIfNode(VirtNode):
    def createArcs(self): pass
    def addParams(self,modulename,paramnames,widths):
        self.module_params[modulename] = zip(paramnames,widths)
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VirtSysEntryNode(VirtSysIfNode):
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VirtSysExitNode(VirtSysIfNode):
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VCVirtDP:
    def createArcs(self):
        for n in self.nodes.values(): n.createArcs()
    def __init__(self,vcir):
        self.sysEntryNode = VirtSysEntryNode(0,vcir,{
            'module_params' : {}
            })
        self.sysExitNode = VirtSysExitNode(1,vcir,{
            'module_params' : {}
            })
        self.nodes = {
            0 : self.sysEntryNode,
            1 : self.sysExitNode,
            }
        nodeid = 2
        # create virtual calls for non-daemon non-called modules
        for en in vcir.nonCalledNonDaemonEns():
            moduledict = vcir.module_entries[en]
            node = VirtCPNode( nodeid, vcir, {
                'callentry' : en,
                'callexit' : moduledict['exit'],
                } )
            self.nodes[nodeid] = node
            nodeid = nodeid + 1
            self.sysEntryNode.addParams( moduledict['name'], moduledict['inames'], moduledict['iwidths'] )
            self.sysExitNode.addParams( moduledict['name'], moduledict['onames'], moduledict['owidths'] )
