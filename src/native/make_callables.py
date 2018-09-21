#!/usr/bin/env python3

# Tool to autogenerate some simpler CallableIntrinsics.

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import functools
import os
import shlex
import subprocess
import sys
import tokenize
import io


def getCallables():
    return [
        Callable('HhvmInterop.CopyFromHhvm', targs=['targT'], args=['handle'], retType='targT', intrinsic=True),
        Callable('HhvmInterop.CopyOptionToHhvm', targs=['targT'], args=['obj'], retType='?readonly HhvmHandle'),
        Callable('HhvmInterop.CopyToHhvm', targs=['targT'], args=['obj'], retType='readonly HhvmHandle', intrinsic=True),
        Callable('HhvmInterop.CreateFromProxyPointer', targs=['targT'], args=['value'], retType='targT', intrinsic=True),
        Callable('HhvmInterop.CreateFromProxyPointerAndType', targs=['targT'], args=['handle', 'hhvmType'], retType='targT', intrinsic=True),
        Callable('HhvmInterop.FetchProxyPointer', targs=['targT'], args=['ptr'], retType='HhvmHandle', intrinsic=True),
        Callable('HhvmInterop.HhvmVariantFactoryFromNullable', targs=['targT'], args=['variant', 'value']),
        Callable('HhvmInterop.InternalBitcastToRetValue', targs=['targT'], args=['i'], retType='(Int, Int)', intrinsic=True),
        Callable('HhvmInterop.InternalBitcastLambdaToRetValue', args=['i'], retType='(Int, Int)'),
        Callable('HhvmInterop.InternalCreateFrozenFromItems', targs=['t', 'i'], args=['items'], retType='t', intrinsic=True),
        Callable('HhvmInterop.InternalCreateFrozenFromIterator', targs=['t', 'i'], args=['items'], retType='t', intrinsic=True),
        Callable('HhvmInterop.InternalCreateMutableFromItems', targs=['t', 'i'], args=['items'], retType='t', intrinsic=True),
        Callable('HhvmInterop.InternalCreateMutableFromIterator', targs=['t', 'i'], args=['items'], retType='t', intrinsic=True),
        Callable('HhvmInterop.InternalCreateParamTupleFromNullableObject', targs=['targT'], args=['nullable'], retType='targT'),
        Callable('HhvmInterop.InternalCreateParamTupleFromNullableString', args=['nullable'], retType='Unsafe.RawStorage<String>'),
        # NOTE: The return type here should really be
        # '(Int, Unsafe.RawStorage<targT>)' but that errors with:
        # Expected scalar type, but got Unsafe.RawStorage<Bool>
        Callable('HhvmInterop.InternalCreateParamTupleFromNullableT', targs=['targT'], args=['nullable'], retType='(Int, targT)'),
        Callable('HhvmInterop.InternalCreateParamTupleFromOption', targs=['targT'], args=['nullable'], retType='(Int, targT)'),
        Callable('HhvmInterop.InternalCreateParamTupleFromOptionObject', targs=['targT'], args=['option'], retType='targT'),
        Callable('HhvmInterop.InternalCreateParamTupleFromOptionString', args=['option'], retType='Unsafe.RawStorage<String>'),
        Callable('HhvmInterop.InternalCreateRetValueFromMixed', args=['m'], retType='(Int, Int)'),
        Callable('HhvmInterop.InternalCreateVarrayFromItems', targs=['t', 'i'], args=['items'], retType='Vector.HH_varray2<t>'),
        Callable('HhvmInterop.InternalMapGetItems', targs=['targK', 'targV'], args=['src'], retType='mutable Iterator<(targK, targV)>'),
        Callable('HhvmInterop.InternalSetGetValues', targs=['t'], args=['src'], retType='mutable Iterator<t>'),
        Callable('HhvmInterop.PropertyGetHelper', targs=['targT', 'targU'], args=['ptr', 'field'], retType='targU', intrinsic=True),
        Callable('HhvmInterop.PropertySetHelper', targs=['targT', 'targU'], args=['ptr', 'field', 'value'], intrinsic=True),
        Callable('HhvmInterop.ThrowUnknownHhvmTypeError', args=['actual', 'name'], retType=False),
        Callable('HhvmInterop.TupleHelperCreate', args=[], retType='mutable HhvmInterop.TupleHelper'),
        Callable('HhvmInterop.TupleHelperAppend', args=['obj', 'value']),
        Callable('HhvmInterop.TupleHelperGet', args=['obj', 'index'], retType='(Int, Int)'),

        Callable('HhvmInterop_ObjectCons.Create', args=['classId'], retType='Runtime.HhvmHandle'),
        Callable('HhvmInterop_ObjectCons.SetFieldMixed', args=['obj', 'slot', 'value']),
        Callable('HhvmInterop_ObjectCons.Finish', args=['obj']),

        Callable('HhvmInterop_Gather.GatherConvert', targs=['t'], args=['data', 'offset'], retType='(t, Int)', intrinsic=True),
        Callable('HhvmInterop_Gather.GatherCollect', args=['object', 'classId'], retType='(Runtime.NonGCPointer, Runtime.NonGCPointer)'),
        Callable('HhvmInterop_Gather.GatherCleanup', args=['handle']),
        Callable('HhvmInterop_Gather.Gather', targs=['t'], args=['heapObject', 'classId'], retType='t'),
        Callable('HhvmInterop_Gather.FetchRawScalar', targs=['t'], args=['data', 'offset'], retType='(t, Int)', intrinsic=True),
        Callable('HhvmInterop_Gather.DeserializeBool', args=['data', 'offset'], retType='(Bool, Int)'),
        Callable('HhvmInterop_Gather.DeserializeOption', targs=['t'], args=['data', 'offset'], retType='(?t, Int)'),
        Callable('HhvmInterop_Gather.DeserializeOptionOption', targs=['t'], args=['data', 'offset'], retType='(??t, Int)'),
        Callable('HhvmInterop_Gather.DeserializeProxy', targs=['t'], args=['data', 'offset'], retType='(t, Int)'),
        Callable('HhvmInterop_Gather.DeserializeKBase', args=['data', 'offset'], retType='(mutable Runtime.GCPointer, Int)', intrinsic=True),
        Callable('HhvmInterop_Gather.MDeserializeKBase', args=['data', 'offset'], retType='(mutable Runtime.GCPointer, Int)', intrinsic=True),
        Callable('HhvmInterop_Gather.DeserializeArray', targs=['t'], args=['data', 'offset'], retType='(Array<t>, Int)'),
        Callable('HhvmInterop_Gather.MDeserializeArray', targs=['t'], args=['data', 'offset'], retType='(mutable Array<t>, Int)'),
        Callable('HhvmInterop_Gather.DeserializeMap', targs=['tk', 'tv'], args=['data', 'offset'], retType='(Map<tk, tv>, Int)'),
        Callable('HhvmInterop_Gather.MDeserializeMap', targs=['tk', 'tv'], args=['data', 'offset'], retType='(mutable Map<tk, tv>, Int)'),
        Callable('HhvmInterop_Gather.DeserializeUnorderedMap', targs=['tk', 'tv'], args=['data', 'offset'], retType='(UnorderedMap<tk, tv>, Int)'),
        Callable('HhvmInterop_Gather.MDeserializeUnorderedMap', targs=['tk', 'tv'], args=['data', 'offset'], retType='(mutable UnorderedMap<tk, tv>, Int)'),
        Callable('HhvmInterop_Gather.DeserializeVector', targs=['t'], args=['data', 'offset'], retType='(Vector<t>, Int)'),
        Callable('HhvmInterop_Gather.MDeserializeVector', targs=['t'], args=['data', 'offset'], retType='(mutable Vector<t>, Int)'),
        Callable('HhvmInterop_Gather.DeserializeSet', targs=['t'], args=['data', 'offset'], retType='(Set<t>, Int)'),
        Callable('HhvmInterop_Gather.MDeserializeSet', targs=['t'], args=['data', 'offset'], retType='(mutable Set<t>, Int)'),
        Callable('HhvmInterop_Gather.DeserializeUnorderedSet', targs=['t'], args=['data', 'offset'], retType='(UnorderedSet<t>, Int)'),
        Callable('HhvmInterop_Gather.MDeserializeUnorderedSet', targs=['t'], args=['data', 'offset'], retType='(mutable UnorderedSet<t>, Int)'),
        Callable('HhvmInterop_Gather.DeserializeTuple2', targs=['t0', 't1'], args=['data', 'offset'], retType='((t0, t1), Int)'),
        Callable('HhvmInterop_Gather.DeserializeTuple3', targs=['t0', 't1', 't2'], args=['data', 'offset'], retType='((t0, t1, t2), Int)'),
        Callable('HhvmInterop_Gather.DeserializeTuple4', targs=['t0', 't1', 't2', 't3'], args=['data', 'offset'], retType='((t0, t1, t2, t3), Int)'),
        Callable('HhvmInterop_Gather.DeserializeMixed', args=['data', 'offset', 'validTypeMask'], retType='(HH.Mixed, Int)'),
        Callable('HhvmInterop_Gather.DeserializeLambda', args=['data', 'offset'], retType='(HH.Lambda, Int)'),
        Callable('HhvmInterop_Gather.ThrowUnknownClassIdError', args=['classId']),

        Callable('HhvmInterop_ShapeCons.Create', args=['shapeId'], retType='Runtime.HhvmShapeHandle'),
        Callable('HhvmInterop_ShapeCons.SetFieldMixed', args=['obj', 'slot', 'value']),
        Callable('HhvmInterop_ShapeCons.Finish', args=['obj']),

        Callable('HhvmInterop_PropertyGetHelper.CheckRetValueType', args=['tup', 'validTypeMask']),
        Callable('HhvmInterop_PropertyGetHelper.InternalBitcastFromInt', targs=['targT'], args=['i'], retType='targT', intrinsic=True),
        Callable('HhvmInterop_PropertyGetHelper.InternalCreateNonNullableFromRetValue', targs=['targT'], args=['value'], retType='targT', intrinsic=True),
        Callable('HhvmInterop_PropertyGetHelper.InternalCreateFromRetValue', targs=['t'], args=['v'], retType='t', intrinsic=True),
        Callable('HhvmInterop_PropertyGetHelper.InternalCreateNullableFromRetValue', targs=['targT'], args=['t'], retType='Nullable<targT>'),
        Callable('HhvmInterop_PropertyGetHelper.InternalCreateOptionFromRetValue', targs=['targT'], args=['t'], retType='?targT'),
        Callable('HhvmInterop_PropertyGetHelper.InternalCreateOptionOptionFromRetValue', targs=['targT'], args=['t'], retType='??targT'),
        Callable('HhvmInterop_PropertyGetHelper.InternalCreateRetValueFromOption', targs=['targT'], args=['option'], retType='(Int, Int)'),
        Callable('HhvmInterop_PropertyGetHelper.InternalCreateRetValueFromOptionOption', targs=['targT'], args=['option'], retType='(Int, Int)'),
        Callable('HhvmInterop_PropertyGetHelper.InternalCreateLambdaFromRetValue', args=['t'], retType='HH.Lambda'),

        Callable('HhvmInterop_PropertySetHelper.InternalSetLambdaProperty', targs=['targT'], args=['obj', 'field', 'value']),
        Callable('HhvmInterop_PropertySetHelper.InternalSetMixedProperty', targs=['targT'], args=['obj', 'field', 'value']),
        Callable('HhvmInterop_PropertySetHelper.InternalSetNullableProperty', targs=['targT', 'targU'], args=['obj', 'field', 'value']),
        Callable('HhvmInterop_PropertySetHelper.InternalSetOptionOptionProperty', targs=['targT', 'targU'], args=['obj', 'field', 'value']),
        Callable('HhvmInterop_PropertySetHelper.InternalSetOptionProperty', targs=['targT', 'targU'], args=['obj', 'field', 'value']),
        Callable('HhvmInterop_PropertySetHelper.InternalSetProperty', targs=['targT', 'targU'], args=['obj', 'field', 'value'], intrinsic=True),

        Callable('SkipRuntime.GetObjectType', 'SKIP_HHVM_Object_getType', namedCall=True, args=['handle'], retType='String', allocAmount='AllocBounded()'),
        Callable('SkipRuntime.HhvmArrayRetCreate', 'SKIP_HhvmArrayRet_create', namedCall=True, args=['var', 'value']),
        Callable('SkipRuntime.HhvmObjectRetCreate', 'SKIP_HhvmObjectRet_create', namedCall=True, args=['var', 'value']),
        Callable('SkipRuntime.HhvmVariantFromMixed', 'SKIP_HhvmVariant_fromMixed', namedCall=True, args=['variant', 'value'], allocAmount='AllocBounded()'),
        Callable('SkipRuntime.HhvmVariantToRetValue', 'SKIP_HHVM_Nullable_getMixed', namedCall=True, args=['obj'], retType='(Int, Int)'),
        Callable('SkipRuntime.MaybeConvertToArray', 'SKIP_HHVM_MaybeConvertToArray', namedCall=True, args=['obj'], retType='(Int, Int)'),
        Callable('SkipRuntime.ObjectGetFieldMixed', 'SKIP_HHVM_Object_getField_Mixed', namedCall=True, args=['handle', 'field'], retType='(Int, Int)'),
        Callable('SkipRuntime.ShapeGetFieldMixed', 'SKIP_HHVM_Shape_getField_Mixed', namedCall=True, args=['handle', 'field'], retType='(Int, Int)'),
        Callable('SkipRuntime.StringExtractData', 'SKIP_string_extractData', namedCall=True, args=['handle'], retType='String'),
        Callable('SkipRuntime.WrapHhvmArray', 'SKIP_Obstack_wrapHhvmArray', namedCall=True, args=['array'], retType='HhvmShapeHandle', canThrow=False),
        Callable('SkipRuntime.WrapHhvmObject', 'SKIP_Obstack_wrapHhvmObject', namedCall=True, args=['obj'], retType='HhvmHandle', canThrow=False),
    ]


def tokenizeString(s):
    g = tokenize.tokenize(io.BytesIO(s.encode('utf-8')).readline)
    # the first token is always the encoding
    # the last token is always empty
    tokens = [tokval for _, tokval, _, _, _ in g][1:-1]
    return tokens


# parseType() returns type
# The type return assumes that type variables are Type.
#
def parseType(t, forRequest):
    if not t:
        return None

    # convert something like 'Foo<tBar, tBaz>' to
    # fs.specializedType("Foo", DeepFrozen(), Array[tBar, tBaz])
    tokens = tokenizeString(t)

    # Quick BNF:
    #   EXPR: MUT T
    #       | MUT T < EXPR-SEQ >
    #       | MUT ( EXPR-SEQ )
    #       | MUT ? EXPR
    #   MUT:
    #      | mutable
    #      | readonly
    #   EXPR-SEQ: EXPR
    #           | EXPR , EXPR-SEQ
    #   T: Type
    #    | Module.Type
    #

    def peek():
        if tokens:
            return {
                '<<': '<',
                '>>': '>',
            }.get(tokens[0], tokens[0])

    def pop():
        if tokens[0] == '<<':
            tokens[0] = '<'
            return '<'
        elif tokens[0] == '>>':
            tokens[0] = '>'
            return '>'
        else:
            return tokens.pop(0)

    # return True if t looks like a Type rather than a variable
    def isType(t):
        if '.' in t:
            t = t.split('.')[1]
        return t[0].isupper()

    def frozenKW(f):
        return {
            'frozen': 'DeepFrozen()',
            'mutable': 'Mutable()',
            'readonly': 'Readonly()',
        }[f]
        return f[0].upper() + f[1:] + '()'

    def simpleType(t):
        return {
            'Bool': 'tBool',
            'Int': 'tInt',
            'Float': 'tFloat',
            'String': 'tString',
            'HhvmHandle': 'tHhvmHandle',
            'HhvmShapeHandle': 'tHhvmShapeHandle'
        }.get(t)

    def parseExprSeq(seq):
        while True:
            seq.append(parseExpr())
            if peek() != ',':
                break
            pop()

    def parseExpr():
        mut = parseMut()

        if peek() == '(':
            pop()
            args = []
            parseExprSeq(args)
            assert pop() == ')'
            generic = f'FrontEndLazyGClass("Tuple{len(args)}")'
            targs = ', '.join(args)
            if forRequest:
                return f's.getTclass2({generic}, Array[{targs}], {frozenKW(mut)})'
            else:
                return f'fs.specializer.specializeType2({generic}, {frozenKW(mut)}, Array[{targs}])'

        if peek() == '?':
            pop()
            targ = parseExpr()
            generic = 'FrontEndLazyGClass("Option")'
            if forRequest:
                return f's.getTclass2({generic}, Array[{targ}], {frozenKW(mut)})'
            else:
                return f'fs.specializer.specializeType2({generic}, {frozenKW(mut)}, Array[{targ}])'

        t = parseT()
        targs = []
        if peek() == '<':
            pop()
            parseExprSeq(targs)
            assert pop() == '>'
        if not isType(t):
            assert not targs, \
                'cannot construct a parametric Type from a variable'
            return t

        if not targs:
            simple = simpleType(t)
            if simple:
                return simple

        targs = ', '.join(targs)

        generic = f'FrontEndLazyGClass("{t}")'
        if forRequest:
            return f's.getTclass2({generic}, Array[{targs}], {frozenKW(mut)})'
        else:
            return f'fs.specializer.specializeType2({generic}, {frozenKW(mut)}, Array[{targs}])'

    def parseMut():
        if peek() in ('mutable', 'readonly'):
            return pop()
        else:
            return 'frozen'

    def parseT():
        t = pop()
        if peek() == '.':
            pop()
            return t + '.' + pop()
        else:
            return t

    expr = parseExpr()
    assert not tokens

    return expr


class Callable(object):
    def __init__(self, className, name=None, targs=(), args=(), retType=None,
                 intrinsic=False, namedCall=False, canThrow=True,
                 allocAmount=None):

        parts = className.split('.', 1)
        if len(parts) == 1:
            parts = ('', parts[0])
        moduleName, className = parts

        if not name:
            name = moduleName + '.' + className[0].lower() + className[1:]

        assert not (namedCall and targs), 'NamedCalls cannot have targs'

        self.name = name
        self.className = className
        self.moduleName = moduleName
        self.targs = targs
        self.args = args
        self.intrinsic = intrinsic
        self.namedCall = namedCall
        self.canThrow = canThrow
        self.allocAmount = allocAmount
        self.retType = retType
        self.returns = retType != False

    def sortKey(self):
        return (self.moduleName, self.className, self.name)

    def write(self, out):
        out((f'class {self.className}{"()" * (not self.intrinsic)} '
               'extends O.CallableIntrinsic {'))
        out(f'  const name: String = "{self.name}";')
        out()

        # call()
        out('  static fun call(')
        out('    fs: mutable O.FunSpecializer,')
        for targ in self.targs:
            out(f'    {targ}: Type,')
        for arg in self.args:
            out(f'    {arg}: O.MaybeExists<O.InstrTree>,')
        out(f'    pos: Pos,')
        out('  ): %s {' % (
            "O.MaybeExists<O.InstrTree>" if self.retType else "void",))

        if self.retType:
            retType = parseType(self.retType, False)
        else:
            retType = 'tVoid'

        funcName, retName = (('emitNamedCall', 'retType') if self.namedCall
                             else ('emitCallFunction', 'typ'))

        out(f'    {"_ = " if not self.retType else ""}static::{funcName}{{')
        out('      fs,')
        out(f'      {retName} => {retType},')

        if self.targs:
            out(f'      targs => Array[{", ".join(self.targs)}],')

        out(f'      args => Array[{", ".join(self.args)}],')
        out('      pos,')

        if self.allocAmount:
            out(f'      allocAmount => {self.allocAmount},')

        if not self.returns:
            out(f'      returns => false,')

        if not self.canThrow:
            out('      canThrow => false,')

        out('    }')
        out('  }')
        out()

        # request()
        targParams = ''.join((map(lambda x: f', {x}: Tclass', self.targs)))

        out(f'  static fun request(s: mutable O.Specializer{targParams}): void {{')
        out('    _ = s;') # avoid complaints about unused parameter

        isVoid = True
        if not self.namedCall:
            isVoid = False
            targParams = ', '.join(self.targs)
            out(f'    s.requestFunction(static::funName(), '
                  f'Array[{targParams}]);')

        if self.retType:
            out('    _ = ')
            retType = parseType(self.retType, True)
            out(retType)
            out(';')

        out('  }')

        out('}')
        out()


outfile = os.path.join(os.path.dirname(__file__), 'callables.sk')
f = open(outfile, 'w')
out = functools.partial(print, file=f)

out("// AUTOGENERATED FILE - DO NOT EDIT")
out("// Generated with: python3", ' '.join(map(shlex.quote, sys.argv)))
out()
out('module alias O = OuterIstToIR;')
out()


currentModule = None
for intrinsic in sorted(getCallables(), key=Callable.sortKey):
    if intrinsic.moduleName != currentModule:
        if currentModule:
            out('module end;')
            out()
        currentModule = intrinsic.moduleName
        out(f'module {currentModule};')
        out()

    intrinsic.write(out)

f.close()

# run the pretty-printer on our output
printer = os.path.normpath(os.path.join(__file__,
                                        "../../../build/bin/skip_printer"))
printer = printer if os.path.exists(printer) else "skip_printer"
subprocess.check_call((printer, '--write', outfile))
