import sys
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
        not_ : 'not',
        mul  : '*',
        add  : '+',
        }
    def eval(self) : return self.op(*[a.eval() for a in self.args])
    def __str__(self) : return \
        ' '.join([ '(', str(self.args[0]), self.oplabel[self.op], str(self.args[1]), ')' ]) \
            if self.arity == 2 else \
        ' '.join([ '(', self.oplabel[self.op], str(self.args[0]), ')' ])
    def __init__(self,*args):
        if len(args) == 3:
            self.op = args[1]
            self.args = [ args[0], args[2] ]
        elif len(args) == 2:
            self.op = args[0]
            self.args = [ args[1] ]
        else:
            print('Expression object invalid argument coutnt',args)
            sys.exit(1)
        self.arity = len(args) - 1

class f(NodePropExpr):
    def eval(self) : return getattr(self.node,self.f)()
    def __str__(self) : return self.f
    def __init__(self,node,f):
        self.node = node
        self.f = f

