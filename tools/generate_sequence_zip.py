#!/usr/bin/env python3

zipClasses = """class Zip{n}Sequence<{tparams}>({args}) extends Sequence<({tparams})> {{
  fun values(): mutable Iterator<({tparams})> {{
{values}
    loop {{
      ({nexts}) match {{
      | ({somes}) -> yield ({vals})
      | _ -> break void
      }}
    }}
  }}
  fun size(): Int {{
    Vector[{sizes}].min().fromSome()
  }}
}}"""

zipExtensions = """  static fun zip{n}<{tparams}>(
{sequences}
  ): Sequence<({tparams})> {{
    Zip{n}Sequence({ss})
  }}"""

TUPLE_MIN = 2
TUPLE_MAX = 10

def getZipClass(n):
    tparams = ", ".join(("T{}".format(x) for x in range(n)))
    args = ", ".join(("s{}: Sequence<T{}>".format(x, x) for x in range(n)))
    values = "\n".join(("    it{} = this.s{}.values();".format(x, x)
                        for x in range(n)))
    nexts = ", ".join(("it{}.next()".format(x) for x in range(n)))
    somes = ", ".join(("Some(val{})".format(x) for x in range(n)))
    vals = ", ".join(("val{}".format(x) for x in range(n)))
    sizes = ", ".join(("this.s{}.size()".format(x) for x in range(n)))
    code = zipClasses.format(n=n, tparams=tparams, args=args,
                             values=values, nexts=nexts, somes=somes, vals=vals, sizes=sizes)
    return code


def getZipExtension(n):
    tparams = ", ".join(("T{}".format(x) for x in range(n)))
    sequences = "\n".join(
        ("    s{}: Sequence<T{}>,".format(x, x) for x in range(n)))
    ss = ", ".join(("s{}".format(x) for x in range(n)))
    code = zipExtensions.format(
        n=n, tparams=tparams, sequences=sequences, ss=ss)
    return code


def main():
    print("// @generated")
    print("// Use tools/generate_sequence_zip.py to regenerate")
    print("module Sequence;")
    print()
    for n in range(TUPLE_MIN, TUPLE_MAX + 1):
        print(getZipClass(n))
        print()
    print("extension mutable base class .Sequence {")
    for n in range(TUPLE_MIN, TUPLE_MAX + 1):
        print(getZipExtension(n))
    print("}")
    print()
    print("module end;")


if __name__ == '__main__':
    main()
