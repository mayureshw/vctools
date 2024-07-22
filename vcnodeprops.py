import sys
import operator

class Functor:
    @staticmethod
    def isconst(c): return isinstance(c,int)
    @classmethod
    def create(cls,e): return c(e) if cls.isconst(e) else e[0](e[1:]) if isinstance(e,tuple) else e([])
    def __str__(self) :
        return self.__class__.__name__ if self.args == [] else (
        self.__class__.__name__ + '( ' + \
        ', '.join(str(a) for a in self.args) + \
        ' )' )
    def __init__(self,args): self.args = [ self.create(a) for a in args ]

class Unary(Functor):
    def eval(self,n): return self.op( self.args[0].eval(n) )
    def __str__(self): return '( ' + \
        self.opstr + ' ' + str(self.args[0]) + \
        ' )'
class not_(Unary):
    opstr = 'not'
    op = operator.not_

class c(Functor):
    def eval(self,n): return self.val
    def __str__(self): return str(self.val)
    def __init__(self,val): self.val = val

class Infix(Functor):
    def eval(self,n): return self.op( self.args[0].eval(n), self.args[1].eval(n) )
    def __str__(self): return '( ' + \
        str(self.args[0]) + ' ' + self.opstr + ' ' + str(self.args[1]) + \
        ' )'
class eq(Infix):
    opstr = '='
    op = operator.eq
class gt(Infix):
    opstr = '>'
    op = operator.gt
class le(Infix):
    opstr = '<='
    op = operator.le
class ne(Infix):
    opstr = '!='
    op = operator.ne
class and_(Infix):
    opstr = 'and'
    op = operator.and_
class or_(Infix):
    opstr = 'or'
    op = operator.or_
class mul(Infix):
    opstr = '*'
    op = operator.mul
class add(Infix):
    opstr = '+'
    op = operator.add

class fanin(Functor):
    def eval(self,n) : return n.fanin(str(self.args[0]))
class fanout(Functor):
    def eval(self,n) : return n.fanout(str(self.args[0]))
class isPlace(Functor):
    def eval(self,n) : return n.isPlace()
class isEntryPlace(Functor):
    def eval(self,n) : return n.isEntryPlace()
class isMutex(Functor):
    def eval(self,n) : return n.isMutex()
class isPassiveBranch(Functor):
    def eval(self,n) : return n.isPassiveBranch()
class isBranch(Functor):
    def eval(self,n) : return n.isBranch()
class isTransition(Functor):
    def eval(self,n) : return n.isTransition()

class petri(Functor): pass
class branch(Functor): pass
class mutex(Functor): pass
class rev_mutex(Functor): pass
class passivebranch(Functor): pass
class rev_passivebranch(Functor): pass
class total(Functor): pass
