from vcirbase import *

# Nodes that represent the environment (these neither come from DP nor from PN,
# but may build arcs with DP/PN nodes)

class VirtArc(Arc):
    def dotprops(self): return [
        ('style','dashed'),
        ('color','gray'),
        ]
    def __init__(self,d):
        super().__init__(d)

class VirtNode(Node):
    def dotprops(self): return [ ('color','gray') ]
    def isVirt(self): return True
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VirtCPNode(VirtNode):
    def nodeClass(self): return 'VirtCPNode'
    def idstr(self): return 'virtCP_' + str(self.nodeid)
    def createArcs(self):
        pass
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VCVirtDP:
    def __init__(self,vcir):
        nodeid = 0
        self.nodes = {}
        # create virtual calls for non-daemon non-called modules
        for en in vcir.nonCalledNonDaemonEns():
            node = VirtCPNode( nodeid, vcir, {} )
            self.nodes[nodeid] = node
            nodeid = nodeid + 1
