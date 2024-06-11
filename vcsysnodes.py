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
    def __init__(self,sysdp,vcir,props):
        self.nodeid = SysNode.cntr
        super().__init__(self.nodeid,vcir,props)
        sysdp.nodes[self.nodeid] = self
        SysNode.cntr = SysNode.cntr + 1

class SysPortNode(SysNode):
    def isPort(self): return True
    def isControlPort(self): return False
    def istr(self,rel,pos): return self.name()
    def ostr(self,rel,pos): return self.name()
    def __init__(self,sysdp,vcir,props): super().__init__(sysdp,vcir,props)

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

# During construction of sysdp, can't extract it from vcir, hence separate args
def createPort(sysdp, vcir, ClsArgsList):
    ClsList,ArgsList = list(zip(*ClsArgsList))
    class Port(*ClsList,SysPortNode):
        def __init__(self):
            for cls,args in ClsArgsList: cls.__init__(self,*args)
            SysPortNode.__init__(self,sysdp,vcir,{})
    port = Port()
    sysdp.ports.append(port)
    return port

class SysPipeNode(SysNode):
    def isSysOutPipe(self): return self.name not in self.vcir.dp.pipereads
    def isSysInPipe(self): return self.name not in self.vcir.dp.pipefeeds
    def optype(self): return 'Pipe'
    def dotprops(self): return [
        ('color','gray'),
        ('label','pipe:'+self.name),
        ]
    def __init__(self,sysdp,vcir,props):
        super().__init__(sysdp,vcir,props)
        self.iwidths = [ self.width ]
        self.owidths = [ self.width ]
        if self.isSysOutPipe():
            dataport = createPort(sysdp, vcir, [
                (OutPort,[]),
                (PipePort,[self.name]),
                (DataPort,[self.width]),
                ])
            DPArc( self, dataport, { 'rel':'data' } )
        if self.isSysInPipe():
            dataport = createPort(sysdp, vcir, [
                (InPort,[]),
                (PipePort,[self.name]),
                (DataPort,[self.width]),
                ])
            DPArc( dataport, self, { 'rel':'data' } )
        if dataport != None:
            reqport = createPort(sysdp, vcir, [
                (InPort, []), (PipePort,[self.name]), (ReqPort,[]) ])
            PNArc( reqport, self, {} )
            ackport = createPort(sysdp, vcir, [
                (OutPort,[]), (PipePort,[self.name]), (AckPort,[]) ])
            PNArc( self, ackport, {} )

class VCSysDP:
    def processModuleInterface(self,vcir,en):
        moduledict = vcir.module_entries[en]
        modulename = moduledict['name']
        ex = moduledict['exit']
        entryplace = vcir.pn.nodes[en]
        exitplace = vcir.pn.nodes[ex]
        reqport = createPort(self, vcir, [
            (InPort, []), (ModulePort,[modulename]), (ReqPort,[]) ])
        PNArc( reqport, entryplace, {} )
        ackport = createPort(self, vcir, [
            (OutPort,[]), (ModulePort,[modulename]), (AckPort,[]) ])
        PNArc( exitplace, ackport, {} )
        for paramname,width in zip( moduledict['inames'], moduledict['iwidths'] ):
            iparamport = createPort(self, vcir, [
                (InPort,[]),
                (ModuleParamPort,[modulename,paramname]),
                (DataPort,[width]),
                ])
            DPArc( iparamport, entryplace, {'rel':'data'} )
            entryplace.iwidths.append(width)
        for paramname,width in zip( moduledict['onames'], moduledict['owidths'] ):
            oparamport = createPort(self, vcir, [
                (OutPort,[]),
                (ModuleParamPort,[modulename,paramname]),
                (DataPort,[width]),
                ])
            DPArc( exitplace, oparamport, {'rel':'data'} )
            exitplace.owidths.append(width)
    def __init__(self,vcir):
        self.nodes = {}
        self.ports = []
        # They cease to meet the fanin criteria as sys nodes get added, so keep a copy
        # TODO: Is this still needed?
        self.nonCalledNonDaemonEns = vcir.nonCalledNonDaemonEns()
        for en in self.nonCalledNonDaemonEns: self.processModuleInterface(vcir,en)
        ## pipe nodes
        for p,props in vcir.pipes.items():
            SysPipeNode(self,vcir,{**props,**{
                'name' : p,
                }})
