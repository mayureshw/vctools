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

class VirtSysIfNode(VirtNode):
    def addParams(self,modulename,paramnames,widths):
        self.module_params[modulename] = list(zip(paramnames,widths))
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VirtSysEntryNode(VirtSysIfNode):
    def ithEntry(self,i): return self.oarcs['petri'][i].tgtnode
    def createArcs(self):
        for en in self.vcir.virtdp.nonCalledNonDaemonEns:
            entrynode = self.vcir.pn.nodes[en]
            width = sum(entrynode.owidths)
            arcobj = PNArc({
                'srcnode'   : self,
                'tgtnode'   : entrynode,
                'wt'        : 1,
                })
            self.addOarc(arcobj)
            entrynode.addIarc(arcobj)
            arcobj = DPArc({
                'srcnode' : self,
                'tgtnode' : entrynode,
                'rel' : 'bind',
                'width' : width
                })
            self.addOarc(arcobj)
            entrynode.addIarc(arcobj)
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VirtSysExitNode(VirtSysIfNode):
    def ithExit(self,i): return self.iarcs['passivebranch'][i].srcnode
    def createArcs(self):
        for en in self.vcir.virtdp.nonCalledNonDaemonEns:
            ex = self.vcir.module_entries[en]['exit']
            exitnode = self.vcir.pn.nodes[ex]
            width = sum(exitnode.iwidths)
            arcobj = PNArc({
                'srcnode'   : exitnode,
                'tgtnode'   : self,
                'wt'        : 1,
                })
            self.addIarc(arcobj)
            exitnode.addOarc(arcobj)
            arcobj = DPArc({
                'srcnode' : exitnode,
                'tgtnode' : self,
                'rel' : 'bind',
                'width' : width
                })
            self.addIarc(arcobj)
            exitnode.addOarc(arcobj)
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
        # They cease to meet the fanin criteria as virtual nodes get added, so keep a copy
        self.nonCalledNonDaemonEns = vcir.nonCalledNonDaemonEns()
        # create virtual calls for non-daemon non-called modules
        for en in self.nonCalledNonDaemonEns:
            moduledict = vcir.module_entries[en]
            self.sysEntryNode.addParams( moduledict['name'], moduledict['inames'], moduledict['iwidths'] )
            self.sysExitNode.addParams( moduledict['name'], moduledict['onames'], moduledict['owidths'] )
