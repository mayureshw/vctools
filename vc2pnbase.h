#ifndef _VC2PNBASE_H_
#define _VC2PNBASE_H_

class CPElement;
class DPElement;
class ModuleBase;

class SystemBase
{
public:
    virtual PetriNet* pn() = 0;
    virtual DatumBase* valueDatum(vcValue*) = 0;
    virtual vector<DatumBase*>& storageDatums(vcStorageObject*) = 0;
    virtual Pipe* pipeMap(vcPipe*) = 0;
    virtual ModuleBase* getModule(vcModule*) = 0;
    virtual Operator* createOperator(vcDatapathElement*) = 0;
    virtual VCtyp vctyp(string) = 0;
};

class ModuleBase
{
public:
    virtual string name() = 0;
    virtual SystemBase* sys() = 0;
    virtual DatumBase* inparamDatum(string) = 0;
    virtual CPElement* getCPE(vcCPElement*) = 0;
    virtual CPElement* getCPE(vcCPElementGroup*) = 0;
    virtual CPElement* getCPE(vcPhiSequencer*) = 0;
    virtual CPElement* getCPE(vcLoopTerminator*) = 0;
    virtual CPElement* getCPE(vcTransitionMerge*) = 0;
    virtual DPElement* getDPE(vcDatapathElement*) = 0;
    virtual PNPlace* mutexPlace() = 0;
    virtual PNPlace* entryPlace() = 0;
    virtual PNPlace* exitPlace() = 0;
    virtual vector<DatumBase*> iparamV() = 0;
    virtual vector<DatumBase*> oparamV() = 0;
    virtual DatumBase* opregForWire(vcWire*) = 0;
};

#endif
