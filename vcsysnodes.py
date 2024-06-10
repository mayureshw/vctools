from vcirbase import *
from vcpnnodes import *
from vcdpnodes import *

# Nodes that represent the environment (these neither come from DP nor from PN,
# but may build arcs with DP/PN nodes)

# This module does not need custom arc type. It reuses PN and DP arcs

class SysNode(Node):
    cntr = 0
    def dotprops(self): return [ ('color','gray') ]
    def optype(self): return None
    def nodeClass(self): return 'SysNode'
    def isSys(self): return True
    def idstr(self): return 'sys_' + str(self.nodeid)
    def __init__(self,vcir,props):
        super().__init__(vcir,props)
        self.nodeid = SysNode.cntr
        vcir.sysdp.nodes[self.nodeid] = self
        SysNode.cntr = SysNode.cntr + 1

class SysPortNode(SysNode):
    def isPort(self): return True
    def isControlPort(self): return False

# Port directions
class InPort:
    direction = 'in'
class OutPort:
    direction = 'out'

# Port sources
class ModulePort:
    def name(self): return self.modulename + self.suffix
    def __init__(self,modulename): self.modulename = modulename
class ModuleParamPort:
    def name(self): return self.modulename + '_' + self.paramname
    def __init__(self,modulename,paramname):
        self.modulename = modulename
        self.paramname = paramname
class PipePort:
    def name(self): return self.pipename + self.suffix
    def __init__(self,pipename): self.pipename = pipename

# Port roles
class DataPort:
    suffix = ''
    def __init__(self,dwidth): self.dwidth = dwidth
class ControlPort:
    def isControlPort(self): return True
class ReqPort(ControlPort):
    suffix = '_req'
class AckPort(ControlPort):
    suffix = '_ack'

def createPort(sysdp, ClsArgsList):
    ClsList,ArgsList = list(zip(*ClsArgsList))
    class Port(*ClsList,SysPortNode):
        def __init__(self):
            for cls,args in ClsArgsList: cls.__init__(self,*args)
    sysdp.ports.append(Port())

class SysPipeNode(SysNode):
    def optype(self): return 'Pipe'
    def dotprops(self): return [
        ('color','gray'),
        ('label','pipe:'+self.name),
        ]
    def __init__(self,vcir,props): super().__init__(vcir,props)

class VCSysDP:
    def processModuleInterface(self,vcir,en):
        moduledict = vcir.module_entries[en]
        modulename = moduledict['name']
        ex = moduledict['exit']
        createPort(self, [ (InPort, []), (ModulePort,[modulename]), (ReqPort,[]) ])
        createPort(self, [ (OutPort,[]), (ModulePort,[modulename]), (AckPort,[]) ])
        for paramname,width in zip( moduledict['inames'], moduledict['iwidths'] ):
            createPort(self, [
                (InPort,[]),
                (ModuleParamPort,[modulename,paramname]),
                (DataPort,[width]),
                ])
        for paramname,width in zip( moduledict['onames'], moduledict['owidths'] ):
            createPort(self, [
                (OutPort,[]),
                (ModuleParamPort,[modulename,paramname]),
                (DataPort,[width]),
                ])
    def __init__(self,vcir):
        self.nodes = {}
        self.ports = []
        # They cease to meet the fanin criteria as sys nodes get added, so keep a copy
        # TODO: Is this still needed?
        self.nonCalledNonDaemonEns = vcir.nonCalledNonDaemonEns()
        for en in self.nonCalledNonDaemonEns: self.processModuleInterface(vcir,en)
        ## pipe nodes
        #for p,props in vcir.pipes.items():
        #    n = SysPipeNode(vcir,{**props,**{
        #        'name' : p,
        #        }})
