from vcirbase import *
from vcpnnodes import *
from vcdpnodes import *

# Nodes that represent the environment (these neither come from DP nor from PN,
# but may build arcs with DP/PN nodes)

# This module does not need custom arc type. It reuses PN and DP arcs

# During construction of sysdp, can't extract it from vcir, hence stored separately
class SysNode(Node):
    cntr = 0
    def dotprops(self): return [
        ('label',self.idstr()),
        ('color','gray')
        ]
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
        self.label = self.name

class PortNode(SysNode):
    def dotprops(self): return [
        ('label',self.idstr()+':'+self.direction+'port:'+self.name()),
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

class SysopPlace(SysNode):
    def dotprops(self): return [
        ('label', self.idstr()+':'+self.name),
        ]
    def __init__(self,sysdp,vcir,props) : super().__init__(sysdp,vcir,props)

class SysopEnPlace(SysopPlace):
    def nodeClass(self): return 'EntryPlace'
    def __init__(self,sysdp,vcir,props):
        super().__init__(sysdp,vcir,props)

class SysopExPlace(SysopPlace):
    def nodeClass(self): return 'PassiveBranchPlace' if self.multi else 'PassThroughPlace'
    def __init__(self,sysdp,vcir,props): super().__init__(sysdp,vcir,props)

class SysopInprogressPlace(SysopPlace):
    def nodeClass(self): return 'PassThroughPlace'
    def __init__(self,sysdp,vcir,props): super().__init__(sysdp,vcir,props)

# Note that the controller Petri net already ensures that the requests are Mutexed
# This class just builds the net for the return path
class ArbiteredSysNode(SysNode):
    def buildDPPetriArcs(self,en,ex,entrig,extrig,label):
        PNArc( entrig, en, {} )
        if ex.multi:
            inprogressPlace = SysopInprogressPlace(self.sysdp,self.vcir,{'name':'inprogress:'+label})
            PNArc( entrig, inprogressPlace, {} )
            PNArc( inprogressPlace, extrig, {} )
            exuackarc = PNArc( ex, extrig, {'rel':'passivebranch'} )
            exuackarc.reversedArc()
        else: PNArc( ex, extrig, {} )
    def __init__(self,sysdp,vcir,props): super().__init__(sysdp,vcir,props)

class PipeNode(ArbiteredSysNode):
    def isSysOutPipe(self): return self.name not in self.vcir.dp.pipereads
    def isSysInPipe(self): return self.name not in self.vcir.dp.pipefeeds
    def nodeClass(self): return 'PipeNode'
    def dotprops(self): return [
        ('color','blue'),
        ('shape','cds'),
        ('label',self.idstr()+':pipe:'+self.name),
        ]
# TODO: Also need to add return path for Sys arcs
    def createSysReqAck(self,en,ex):
        trigplace = self.vcir.pn.nodes[self.trigplace]
        trigreq = self.vcir.pn.nodes[self.trigreq]
        trigack = self.vcir.pn.nodes[self.trigack]
        reqport = createPort(self.sysdp, self.vcir, [
            (InPort, []), (PipePort,[self.name]), (ReqPort,[]) ])
        PNArc( reqport, trigplace, {} )
        ackport = createPort(self.sysdp, self.vcir, [
            (OutPort,[]), (PipePort,[self.name]), (AckPort,[]) ])
        ackdelaynode = SysAckDelayNode(self.sysdp,self.vcir,{'name':ackport.name()+'_delay'})
        PNArc( trigack, ackdelaynode, {} )
        PNArc( ackdelaynode, ackport, {} )
        self.buildDPPetriArcs(en,ex,trigreq,trigack,self.idstr())
    def createSysReadArcs(self):
        dataport = createPort(self.sysdp, self.vcir, [
            (OutPort,[]),
            (PipePort,[self.name]),
            (DataPort,[self.width]),
            ])
        DPArc( self.r_ex, dataport, { 'rel':'bind', 'width':self.width } )
        self.createSysReqAck(self.r_en,self.r_ex)
    def createSysFeedArcs(self):
        dataport = createPort(self.sysdp, self.vcir, [
            (InPort,[]),
            (PipePort,[self.name]),
            (DataPort,[self.width]),
            ])
        DPArc( dataport, self.w_en, { 'rel':'bind', 'width':self.width } )
        self.createSysReqAck(self.w_en,self.w_ex)
    def createInternalReadArcs(self):
        for dpe in self.vcir.dp.pipereads[self.name]:
            ureqnode = dpe.ureq()
            uacknode = dpe.uack()
            self.buildDPPetriArcs(self.r_en,self.r_ex,ureqnode,uacknode,dpe.idstr())
            DPArc(self.r_ex,dpe,{'rel': 'bind', 'width': self.width})
            DPArc(dpe,uacknode,{ 'rel': 'dpsync' })
            DPArc(uacknode,dpe,{ 'rel': 'dpsync' })
    def createInternalFeedArcs(self):
        for dpe in self.vcir.dp.pipefeeds[self.name]:
            sreqnode = dpe.sreq()
            sacknode = dpe.sack()
            self.buildDPPetriArcs(self.w_en,self.w_ex,sreqnode,sacknode,dpe.idstr())
            DPArc(dpe,self.w_en,{'rel': 'bind', 'width': self.width})
    def __init__(self,sysdp,vcir,props):
        super().__init__(sysdp,vcir,props)

        haveMultReads = len(self.vcir.dp.pipereads.get(self.name,[])) > 1
        haveMultFeeds = len(self.vcir.dp.pipefeeds.get(self.name,[])) > 1

        self.r_en = SysopEnPlace(sysdp,vcir,{'name':'r_en:'+self.name})
        self.r_ex = SysopExPlace(sysdp,vcir,{'name':'r_ex:'+self.name,'multi':haveMultReads})
        self.w_en = SysopEnPlace(sysdp,vcir,{'name':'w_en:'+self.name})
        self.w_ex = SysopExPlace(sysdp,vcir,{'name':'w_ex:'+self.name,'multi':haveMultFeeds})

        # Ensure that arcs with pipe come before those with DPE
        # First write then read aggr arcs
        PNArc(self.w_en, self, {})
        DPArc(self.w_en, self, {'rel':'data','width':self.width})
        PNArc(self, self.w_ex, {})
        PNArc(self.r_en, self, {})
        PNArc(self, self.r_ex, {})
        DPArc(self, self.r_ex, {'rel':'data','width':self.width})

        if self.isSysOutPipe(): self.createSysReadArcs()
        else: self.createInternalReadArcs()

        if self.isSysInPipe():  self.createSysFeedArcs()
        else: self.createInternalFeedArcs()

class StorageNode(ArbiteredSysNode):
    def nodeClass(self): return 'StorageNode'
    def dotprops(self): return [
        ('color','blue'),
        ('shape','cylinder'),
        ('label',self.idstr()+':storage:'+self.name),
        ]
    def inferAWidth(self):
        if len( self.vcir.dp.storeloads[self.name] ) ==  0:
            print('Store has no Load operations. This is not handled.',self.name)
            sys.exit(1)
        return self.vcir.dp.storeloads[self.name][0].iwidths[0]
    def __init__(self,sysdp,vcir,props):
        super().__init__(sysdp,vcir,props)

        self.awidth = self.inferAWidth()

        haveMultLoads = len(self.vcir.dp.storeloads[self.name]) > 1
        haveMultStores = len(self.vcir.dp.storestores[self.name]) > 1

        self.r_en = SysopEnPlace(sysdp,vcir,{'name':'r_en:'+self.name})
        self.r_ex = SysopExPlace(sysdp,vcir,{'name':'r_ex:'+self.name,'multi':haveMultLoads})

        self.w_en = SysopEnPlace(sysdp,vcir,{'name':'w_en:'+self.name})
        self.w_ex = SysopExPlace(sysdp,vcir,{'name':'w_ex:'+self.name,'multi':haveMultStores})

        # w_en -> self
        PNArc(self.w_en, self, {})
        DPArc(self.w_en, self, {'rel':'data','width':self.awidth})
        DPArc(self.w_en, self, {'rel':'data','width':self.width})

        # self -> w_ex
        PNArc(self, self.w_ex, {})

        # r_en -> self
        PNArc(self.r_en, self, {})
        DPArc(self.r_en, self, {'rel':'data','width':self.awidth})

        # self -> r_ex
        PNArc(self, self.r_ex, {})
        DPArc(self, self.r_ex, {'rel':'data','width':self.width})

        for dpe in self.vcir.dp.storeloads[self.name]:
            sacknode = dpe.sack()
            uacknode = dpe.uack()
            self.buildDPPetriArcs(self.r_en,self.r_ex,sacknode,uacknode,dpe.idstr())
            # dpe -> r_en bind arc for address
            DPArc(dpe,self.r_en,{'rel': 'bind', 'width': self.awidth})
            # r_ex -> dpe bind arc for data
            DPArc(self.r_ex,dpe,{'rel': 'bind', 'width': self.width})
            # dpe <-> uack 2 way dpsync
            DPArc(dpe,uacknode,{ 'rel': 'dpsync' })
            DPArc(uacknode,dpe,{ 'rel': 'dpsync' })

        for dpe in self.vcir.dp.storestores[self.name]:
            sacknode = dpe.sack()
            uacknode = dpe.uack()
            self.buildDPPetriArcs(self.w_en,self.w_ex,sacknode,uacknode,dpe.idstr())
            # dpe -> w_en bind arc for data+address
            DPArc(dpe,self.w_en,{'rel': 'bind', 'width': self.width+self.awidth})

class SysAckDelayNode(SysNode):
    def nodeClass(self): return 'SysAckDelay'
    def __init__(self,sysdp,vcir,props): super().__init__(sysdp,vcir,props)

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
        ackdelaynode = SysAckDelayNode(self,vcir,{'name':ackport.name()+'_delay'})
        PNArc( exitplace, ackdelaynode, {} )
        PNArc( ackdelaynode, ackport, {} )
        for paramname,width in zip( moduledict['inames'], moduledict['iwidths'] ):
            iparamport = createPort(self, vcir, [
                (InPort,[]),
                (ModuleParamPort,[modulename,paramname]),
                (DataPort,[width]),
                ])
            DPArc( iparamport, entryplace, {'rel':'data','width':width} )
        for paramname,width in zip( moduledict['onames'], moduledict['owidths'] ):
            oparamport = createPort(self, vcir, [
                (OutPort,[]),
                (ModuleParamPort,[modulename,paramname]),
                (DataPort,[width]),
                ])
            DPArc( exitplace, oparamport, {'rel':'data','width':width} )
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
        for s,props in vcir.storage.items():
            StorageNode(self,vcir,{**props,**{
                'name' : s,
                }})
