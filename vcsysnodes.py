from vcirbase import *
from vcpnnodes import *
from vcdpnodes import *

# Nodes that represent the environment (these neither come from DP nor from PN,
# but may build arcs with DP/PN nodes)

# This module does not need custom arc type. It reuses PN and DP arcs

# During construction of sysdp, can't extract it from vcir, hence stored separately
class SysNode(Node):
    cntr = 0
    def dotprops(self): return [ ('color','gray') ]
    def optype(self): return None
    def nodeClass(self): return None
    def isSys(self): return True
    def idstr(self): return 'sys_' + str(self.nodeid)
    def __init__(self,sysdp,vcir,props):
        self.nodeid = SysNode.cntr
        super().__init__(self.nodeid,vcir,props)
        sysdp.nodes[self.nodeid] = self
        SysNode.cntr = SysNode.cntr + 1
        self.sysdp = sysdp

class PortNode(SysNode):
    def dotprops(self): return [
        ('label',self.direction+'port:'+self.name()),
        ('shape','trapezium'),
        ]
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

# ClsArgsList: a list of tuples in Cls,Args form where Cls is a Port class and
# Args is a list of its constructor arguments in positional order.
# The list should contain 1 direction class, 1 source class and 1 role class
def createPort(sysdp, vcir, ClsArgsList):
    ClsList,ArgsList = list(zip(*ClsArgsList))
    class Port(*ClsList,PortNode):
        def __init__(self):
            for cls,args in ClsArgsList: cls.__init__(self,*args)
            PortNode.__init__(self,sysdp,vcir,{})
    port = Port()
    sysdp.ports.append(port)
    return port

# Note Aggreg Nodes are not arbiters, arbitration is handled by Petri net

class AggrNode(SysNode):
    def dotprops(self): return [
        ('color','blue'),
        ('shape','rectangle'),
        ('label',self.nodeClass()+':'+self.name),
        ]
    def __init__(self,sysdp,vcir,props): super().__init__(sysdp,vcir,props)
class WriteAggrNode(AggrNode):
    def nodeClass(self): return 'WriteAggrNode'
    def __init__(self,sysdp,vcir,props): super().__init__(sysdp,vcir,props)

class ReadAggrNode(AggrNode):
    def nodeClass(self): return 'ReadAggrNode'
    def __init__(self,sysdp,vcir,props): super().__init__(sysdp,vcir,props)

# nodeClass = DPNode gives a vcInterconnect wrapper automatically, which suits
# most operators with combinational implementation. Such wrapper is not needed
# for some system nodes such as Pipe, ReadAggr, WriteAggr etc. So we use their
# own name as nodeClass. They get a separate handler in vcNode vhdl
class PipeNode(SysNode):
    def isSysOutPipe(self): return self.name not in self.vcir.dp.pipereads
    def isSysInPipe(self): return self.name not in self.vcir.dp.pipefeeds
    def nodeClass(self): return 'PipeNode'
    def dotprops(self): return [
        ('color','blue'),
        ('shape','cds'),
        ('label','pipe:'+self.name),
        ]
    def createSysReqAck(self):
        reqport = createPort(self.sysdp, self.vcir, [
            (InPort, []), (PipePort,[self.name]), (ReqPort,[]) ])
        PNArc( reqport, self, {} )
        ackport = createPort(self.sysdp, self.vcir, [
            (OutPort,[]), (PipePort,[self.name]), (AckPort,[]) ])
        PNArc( self, ackport, {} )
    def createSysReadArcs(self):
        dataport = createPort(self.sysdp, self.vcir, [
            (OutPort,[]),
            (PipePort,[self.name]),
            (DataPort,[self.width]),
            ])
        DPArc( self, dataport, { 'rel':'data' } )
        self.createSysReqAck()
    def createSysFeedArcs(self):
        dataport = createPort(self.sysdp, self.vcir, [
            (InPort,[]),
            (PipePort,[self.name]),
            (DataPort,[self.width]),
            ])
        DPArc( dataport, self, { 'rel':'data' } )
        self.createSysReqAck()
    def createInternalReadArcs(self):
        readpoints = self.vcir.dp.pipereads[self.name]
        if len(readpoints) > 1:
            raggr = ReadAggrNode(self.sysdp,self.vcir,{'name':self.name})
            raggr.iwidths.append(self.width)
            # Ensure that arcs with pipe come before those with DPE
            PNArc(raggr, self, {})
            PNArc(self, raggr, {})
            DPArc(self, raggr, {'rel':'data','width':self.width})
        else: raggr = self
        for dpe in readpoints:
            # dpe <-> pipe bi-directional PN arcs
            PNArc(raggr,dpe,{})
            PNArc(dpe,raggr,{})

            # pipe -> dpe data arc
            DPArc(raggr,dpe,{'rel': 'bind', 'width': self.width})
    def createInternalFeedArcs(self):
        writepoints = self.vcir.dp.pipefeeds[self.name]
        if len(writepoints) > 1:
            waggr = WriteAggrNode(self.sysdp,self.vcir,{'name':self.name})
            waggr.owidths.append(self.width)
            # Ensure that arcs with pipe come before those with DPE
            PNArc(waggr, self, {})
            PNArc(self, waggr, {})
            DPArc(waggr, self, {'rel':'data','width':self.width})
        else: waggr = self
        for dpe in writepoints:
            # dpe <-> pipe bi-directional PN arcs
            PNArc(waggr,dpe,{})
            PNArc(dpe,waggr,{})

            # dpe -> pipe data arc
            DPArc(dpe,waggr,{'rel': 'bind', 'width': self.width})
    def __init__(self,sysdp,vcir,props):
        super().__init__(sysdp,vcir,props)
        self.iwidths = [ self.width ]
        self.owidths = [ self.width ]

        if self.isSysOutPipe(): self.createSysReadArcs()
        else: self.createInternalReadArcs()

        if self.isSysInPipe():  self.createSysFeedArcs()
        else: self.createInternalFeedArcs()

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
            PipeNode(self,vcir,{**props,**{
                'name' : p,
                }})
