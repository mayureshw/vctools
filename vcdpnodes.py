from operator import *
from vcnodeprops import *

class OpClass: pass

class RotateL(OpClass): pass
class RotateR(OpClass): pass
class ShiftRA(OpClass): pass
class Not(OpClass): pass
class S2S(OpClass): pass
class CCast(OpClass): pass
class Select(OpClass): pass
class Slice(OpClass): pass
class Assign(OpClass): pass
class Branch(OpClass): pass
class Phi(OpClass): pass
class Load(OpClass): pass
class Store(OpClass): pass
class Inport(OpClass): pass
class Outport(OpClass): pass
class Call(OpClass): pass
class Plus(OpClass): pass
class Minus(OpClass): pass
class Mult(OpClass): pass
class Div(OpClass): pass
class And(OpClass): pass
class Or(OpClass): pass
class Xor(OpClass): pass
class Nand(OpClass): pass
class Nor(OpClass): pass
class Xnor(OpClass): pass
class Lt(OpClass): pass
class Le(OpClass): pass
class Gt(OpClass): pass
class Ge(OpClass): pass
class Ne(OpClass): pass
class Eq(OpClass): pass
class Concat(OpClass): pass
class SLt(OpClass): pass
class SLe(OpClass): pass
class SGt(OpClass): pass
class SGe(OpClass): pass
class Bitsel(OpClass): pass
class ShiftL(OpClass): pass
class ShiftR(OpClass): pass

class Op:
    opclss = { c.__name__ for c in OpClass.__subclasses__() }
    def __init__(self,props):
        self.__dict__.update(props)
