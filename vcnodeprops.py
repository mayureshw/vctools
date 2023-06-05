from operator import *

# Purpose of making NodeProp is to make sure that the property and its message always remain in sync
class NodePropExpr: pass

class c(NodePropExpr):
    def eval(self) : return self.val
    def __str__(self) : return str(self.val)
    def __init__(self,c): self.val = c

class v(NodePropExpr):
    def eval(self) : return self.node.fanin(self.rel) if self.fantype == 'fanin' else self.node.fanout(self.rel)
    def __str__(self) : return self.rel + '-' + self.fantype
    def __init__(self,node,rel,fantype):
        self.node = node
        self.rel = rel
        self.fantype = fantype

class e(NodePropExpr):
    oplabel = {
        gt   : '>',
        le   : '<=',
        eq   : '=',
        ne   : '!=',
        and_ : 'and',
        or_  : 'or',
        mul  : '*',
        add  : '+',
        }
    def eval(self) : return self.op(self.e1.eval(),self.e2.eval())
    def __str__(self) : return '( ' + str(self.e1) + ' ' + self.oplabel[self.op] + ' ' + str(self.e2) + ' )'
    def __init__(self,e1,op,e2):
        self.e1 = e1
        self.op = op
        self.e2 = e2

class f(NodePropExpr):
    def eval(self) : return getattr(self.node,self.f)()
    def __str__(self) : return self.f
    def __init__(self,node,f):
        self.node = node
        self.f = f

