#!/bin/bash

echo "-------------------------------------------------------------------------------"
echo "- BUILDING SUBMODULES FOR SKIP -"
echo "-------------------------------------------------------------------------------"

export TPDIR="$PWD/subbuild"
mkdir -p $TPDIR

rm -f Makefile.config
touch Makefile.config

echo "-------------------------------------------------------------------------------"
echo "- Checking that the files are there -"
echo "-------------------------------------------------------------------------------"

if [ ! -f "third-party/jemalloc/src/README" ]; then
    echo "ERROR: looks like the git submodules have not been checked out"
    echo "Try running: git submodule update --init --recursive"
    exit 2
fi

echo "-------------------------------------------------------------------------------"
echo "- Building JEMALLOC -"
echo "-------------------------------------------------------------------------------"

(cd ./third-party/jemalloc/src && autoconf)
(cd ./third-party/jemalloc/src && ./configure --prefix=$TPDIR --with-mangling)
(cd ./third-party/jemalloc/src && make -j 16 build_lib_static)
(cd ./third-party/jemalloc/src && make install_bin install_include install_lib)
cat $TPDIR/include/jemalloc/jemalloc.h | grep -v "define je_posix_memalign posix_memalign" > $TPDIR/include/jemalloc/jemalloc_hack.h
cp $TPDIR/include/jemalloc/jemalloc_hack.h $TPDIR/include/jemalloc/jemalloc.h

echo "-------------------------------------------------------------------------------"
echo "- Building ICU -"
echo "-------------------------------------------------------------------------------"

export ICUFLAGS="-DUNISTR_FROM_CHAR_EXPLICIT=explicit -DUNISTR_FROM_STRING_EXPLICIT=explicit -DU_NO_DEFAULT_INCLUDE_UTF_HEADERS=1 -DU_USING_ICU_NAMESPACE=0 -DU_CHARSET_IS_UTF8=1"
(cd third-party/icu/src/icu4c/source/ && \
  CPPFLAGS="$ICUFLAGS" CFLAGS="$ICUFLAGS" ./runConfigureICU Linux --disable-shared --enable-static --disable-tests --disable-samples --prefix="$TPDIR")
(cd third-party/icu/src/icu4c/source/ && make -j 16 && make install)
(cd third-party/icu/src/icu4c/source/tools/escapesrc/ && rm -f *.o)

echo "-------------------------------------------------------------------------------"
echo "- Building PCRE -"
echo "-------------------------------------------------------------------------------"

# Unfortunately, we have to modify those files for the configuration step to succeed.
# So we just make a copy and then, copy them back after we are done ...
cp third-party/pcre/src/INSTALL third-party/pcre/src/INSTALL.back
cp third-party/pcre/src/Makefile.in third-party/pcre/src/Makefile.in.back
cp third-party/pcre/src/aclocal.m4 third-party/pcre/src/aclocal.m4.back
cp third-party/pcre/src/ar-lib third-party/pcre/src/ar-lib.back
cp third-party/pcre/src/compile third-party/pcre/src/compile.back
cp third-party/pcre/src/config.guess third-party/pcre/src/config.guess.back
cp third-party/pcre/src/config.sub third-party/pcre/src/config.sub.back
cp third-party/pcre/src/configure third-party/pcre/src/configure.back
cp third-party/pcre/src/depcomp third-party/pcre/src/depcomp.back
cp third-party/pcre/src/install-sh third-party/pcre/src/install-sh.back
cp third-party/pcre/src/ltmain.sh third-party/pcre/src/ltmain.sh.back
cp third-party/pcre/src/m4/libtool.m4 third-party/pcre/src/m4/libtool.m4.back
cp third-party/pcre/src/missing third-party/pcre/src/missing.back
cp third-party/pcre/src/test-driver third-party/pcre/src/test-driver.back

(cd third-party/pcre/src/ && autoreconf -f -i && ./configure --prefix="$TPDIR" --enable-static --disable-shared --enable-utf && make -j 16 && make install)


cp third-party/pcre/src/INSTALL.back third-party/pcre/src/INSTALL
cp third-party/pcre/src/Makefile.in.back third-party/pcre/src/Makefile.in
cp third-party/pcre/src/aclocal.m4.back third-party/pcre/src/aclocal.m4
cp third-party/pcre/src/ar-lib.back third-party/pcre/src/ar-lib
cp third-party/pcre/src/compile.back third-party/pcre/src/compile
cp third-party/pcre/src/config.guess.back third-party/pcre/src/config.guess
cp third-party/pcre/src/config.sub.back third-party/pcre/src/config.sub
cp third-party/pcre/src/configure.back third-party/pcre/src/configure
cp third-party/pcre/src/depcomp.back third-party/pcre/src/depcomp
cp third-party/pcre/src/install-sh.back third-party/pcre/src/install-sh
cp third-party/pcre/src/ltmain.sh.back third-party/pcre/src/ltmain.sh
cp third-party/pcre/src/m4/libtool.m4.back third-party/pcre/src/m4/libtool.m4
cp third-party/pcre/src/missing.back third-party/pcre/src/missing
cp third-party/pcre/src/test-driver.back third-party/pcre/src/test-driver


if [ "$(uname)" != "Darwin" ]; then
    echo "-------------------------------------------------------------------------------"
    echo "- Building LIBUNWIND -"
    echo "-------------------------------------------------------------------------------"

    (cd third-party/libunwind && ./autogen.sh && ./configure --prefix="$TPDIR" --disable-minidebuginfo && make -j 16 && make install)
fi
