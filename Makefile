-include Makefile.config

default: build/lkg/bin/skip_to_llvm build/lkg/bin/skip_server build/lkg/bin/skip_printer

clean:
	rm -Rf build

# INSTALLING SK

install: build/lkg/bin/skip_to_llvm build/lkg/bin/skip_server build/lkg/bin/skip_printer
	mkdir -p $(PREFIX)/lib/skip/
	mkdir -p $(PREFIX)/bin/

	cp build/lkg/bin/skip_server $(PREFIX)/bin/skip_server
	cp build/lkg/bin/skip_printer $(PREFIX)/bin/skip_printer

	cp -R src/runtime/prelude $(PREFIX)/lib/skip/prelude

	cp build/lkg/tmp/preamble.ll $(PREFIX)/lib/skip
	cp build/lkg/runtime/native/libskip_runtime.a $(PREFIX)/lib/skip
	cp build/lkg/CMakeFiles/skip_to_llvm.lkg.dir/runtime/native/src/sk_standalone.cpp.o $(PREFIX)/lib/skip
	cp subbuild/lib/libicuuc.a $(PREFIX)/lib/skip
	cp subbuild/lib/libicudata.a $(PREFIX)/lib/skip
	cp subbuild/lib/libpcre.a $(PREFIX)/lib/skip
	cp subbuild/lib/libjemalloc_pic.a $(PREFIX)/lib/skip
	cp subbuild/lib/libunwind.a $(PREFIX)/lib/skip 2> /dev/null || :
	cp sk $(PREFIX)/bin/sk
	chmod 755 $(PREFIX)/bin/sk


# CREATING VERSION.CPP

build/lkg/runtime/native/src/version.cpp:
	mkdir -p build/lkg/runtime/native/src
	echo '#include "skip/String.h"' > build/lkg/runtime/native/src/version.cpp
	echo '#include "skip/System-extc.h"' >> build/lkg/runtime/native/src/version.cpp
	echo 'skip::String SKIP_getBuildVersion() { std::string version_info = "'`date`'";  return skip::String(version_info);}' >> build/lkg/runtime/native/src/version.cpp

# BUILDING THE OBJECTS OF THE RUNTIME

INCLUDES:=-I./lkg/runtime/native/include -I./subbuild/include -I ./lkg/runtime/native/src/ $(INCLUDEPATH)
CCWARNINGS=-Wno-sign-compare -Wvla -Wmissing-declarations -Wno-return-type-c-linkage -Wno-format -Wno-defaulted-function-deleted -Wno-unknown-warning-option
CCOPTS:=-DPIC -DUSE_JEMALLOC -g -DNDEBUG -fPIC -msse4.2 -std=c++17 -fdata-sections -ffunction-sections -O2 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -faligned-new
COMPILE_ROBJECT:=$(CXX) $(INCLUDES) $(CCWARNINGS) $(CCOPTS)

# BUILDING LKG RUNTIME OBJECTS

build/lkg/CMakeFiles/skip_to_llvm.lkg.dir/runtime/native/src/sk_standalone.cpp.o: lkg/runtime/native/src/sk_standalone.cpp subbuild/lib
	mkdir -p build/lkg/CMakeFiles/skip_to_llvm.lkg.dir/runtime/native/src
	$(COMPILE_ROBJECT) -o build/lkg/CMakeFiles/skip_to_llvm.lkg.dir/runtime/native/src/sk_standalone.cpp.o -c lkg/runtime/native/src/sk_standalone.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/AllocProfiler.cpp.o: lkg/runtime/native/src/AllocProfiler.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/AllocProfiler.cpp.o -c lkg/runtime/native/src/AllocProfiler.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Arena-malloc.cpp.o: lkg/runtime/native/src/Arena-malloc.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Arena-malloc.cpp.o -c lkg/runtime/native/src/Arena-malloc.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Arena.cpp.o: lkg/runtime/native/src/Arena.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Arena.cpp.o -c lkg/runtime/native/src/Arena.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Async.cpp.o: lkg/runtime/native/src/Async.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Async.cpp.o -c lkg/runtime/native/src/Async.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/File.cpp.o: lkg/runtime/native/src/File.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/File.cpp.o -c lkg/runtime/native/src/File.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/InternTable.cpp.o: lkg/runtime/native/src/InternTable.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/InternTable.cpp.o -c lkg/runtime/native/src/InternTable.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/LockManager.cpp.o: lkg/runtime/native/src/LockManager.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/LockManager.cpp.o -c lkg/runtime/native/src/LockManager.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Math.cpp.o: lkg/runtime/native/src/Math.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Math.cpp.o -c lkg/runtime/native/src/Math.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Obstack.cpp.o: lkg/runtime/native/src/Obstack.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Obstack.cpp.o -c lkg/runtime/native/src/Obstack.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Process.cpp.o: lkg/runtime/native/src/Process.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Process.cpp.o -c lkg/runtime/native/src/Process.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Reactive.cpp.o: lkg/runtime/native/src/Reactive.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Reactive.cpp.o -c lkg/runtime/native/src/Reactive.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Refcount.cpp.o: lkg/runtime/native/src/Refcount.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Refcount.cpp.o -c lkg/runtime/native/src/Refcount.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Regex.cpp.o: lkg/runtime/native/src/Regex.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Regex.cpp.o -c lkg/runtime/native/src/Regex.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/String.cpp.o: lkg/runtime/native/src/String.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/String.cpp.o -c lkg/runtime/native/src/String.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/System.cpp.o: lkg/runtime/native/src/System.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/System.cpp.o -c lkg/runtime/native/src/System.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Task.cpp.o: lkg/runtime/native/src/Task.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Task.cpp.o -c lkg/runtime/native/src/Task.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Type.cpp.o: lkg/runtime/native/src/Type.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Type.cpp.o -c lkg/runtime/native/src/Type.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/intern.cpp.o: lkg/runtime/native/src/intern.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/intern.cpp.o -c lkg/runtime/native/src/intern.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/leak.cpp.o: lkg/runtime/native/src/leak.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/leak.cpp.o -c lkg/runtime/native/src/leak.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/memoize.cpp.o: lkg/runtime/native/src/memoize.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/memoize.cpp.o -c lkg/runtime/native/src/memoize.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/memory.cpp.o: lkg/runtime/native/src/memory.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/memory.cpp.o -c lkg/runtime/native/src/memory.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/objects.cpp.o: lkg/runtime/native/src/objects.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/objects.cpp.o -c lkg/runtime/native/src/objects.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/parallel.cpp.o: lkg/runtime/native/src/parallel.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/parallel.cpp.o -c lkg/runtime/native/src/parallel.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/stats.cpp.o: lkg/runtime/native/src/stats.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/stats.cpp.o -c lkg/runtime/native/src/stats.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/util.cpp.o: lkg/runtime/native/src/util.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/util.cpp.o -c lkg/runtime/native/src/util.cpp

build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/version.cpp.o: build/lkg/runtime/native/src/version.cpp subbuild/lib
	mkdir -p build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/
	$(COMPILE_ROBJECT) -o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/version.cpp.o -c build/lkg/runtime/native/src/version.cpp

# BUILDING SRC RUNTIME OBJECTS

build/src/runtime/native/CMakeFiles/sk_standalone.src.dir/src/sk_standalone.cpp.o: src/runtime/native/src/sk_standalone.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/sk_standalone.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/sk_standalone.src.dir/src/sk_standalone.cpp.o -c src/runtime/native/src/sk_standalone.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/AllocProfiler.cpp.o: src/runtime/native/src/AllocProfiler.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/AllocProfiler.cpp.o -c src/runtime/native/src/AllocProfiler.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Arena-malloc.cpp.o: src/runtime/native/src/Arena-malloc.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Arena-malloc.cpp.o -c src/runtime/native/src/Arena-malloc.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Arena.cpp.o: src/runtime/native/src/Arena.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Arena.cpp.o -c src/runtime/native/src/Arena.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Async.cpp.o: src/runtime/native/src/Async.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Async.cpp.o -c src/runtime/native/src/Async.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/File.cpp.o: src/runtime/native/src/File.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/File.cpp.o -c src/runtime/native/src/File.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/InternTable.cpp.o: src/runtime/native/src/InternTable.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/InternTable.cpp.o -c src/runtime/native/src/InternTable.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/LockManager.cpp.o: src/runtime/native/src/LockManager.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/LockManager.cpp.o -c src/runtime/native/src/LockManager.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Math.cpp.o: src/runtime/native/src/Math.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Math.cpp.o -c src/runtime/native/src/Math.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Obstack.cpp.o: src/runtime/native/src/Obstack.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Obstack.cpp.o -c src/runtime/native/src/Obstack.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Process.cpp.o: src/runtime/native/src/Process.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Process.cpp.o -c src/runtime/native/src/Process.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Reactive.cpp.o: src/runtime/native/src/Reactive.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Reactive.cpp.o -c src/runtime/native/src/Reactive.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Refcount.cpp.o: src/runtime/native/src/Refcount.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Refcount.cpp.o -c src/runtime/native/src/Refcount.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Regex.cpp.o: src/runtime/native/src/Regex.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Regex.cpp.o -c src/runtime/native/src/Regex.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/String.cpp.o: src/runtime/native/src/String.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/String.cpp.o -c src/runtime/native/src/String.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/System.cpp.o: src/runtime/native/src/System.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/System.cpp.o -c src/runtime/native/src/System.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Task.cpp.o: src/runtime/native/src/Task.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Task.cpp.o -c src/runtime/native/src/Task.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Type.cpp.o: src/runtime/native/src/Type.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Type.cpp.o -c src/runtime/native/src/Type.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/intern.cpp.o: src/runtime/native/src/intern.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/intern.cpp.o -c src/runtime/native/src/intern.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/leak.cpp.o: src/runtime/native/src/leak.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/leak.cpp.o -c src/runtime/native/src/leak.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/memory.cpp.o: src/runtime/native/src/memory.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/memory.cpp.o -c src/runtime/native/src/memory.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/objects.cpp.o: src/runtime/native/src/objects.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/objects.cpp.o -c src/runtime/native/src/objects.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/parallel.cpp.o: src/runtime/native/src/parallel.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/parallel.cpp.o -c src/runtime/native/src/parallel.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/stats.cpp.o: src/runtime/native/src/stats.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/stats.cpp.o -c src/runtime/native/src/stats.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/util.cpp.o: src/runtime/native/src/util.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/util.cpp.o -c src/runtime/native/src/util.cpp

build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/version.cpp.o: build/src/runtime/native/src/version.cpp subbuild/lib
	mkdir -p build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/
	$(COMPILE_ROBJECT) -o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/version.cpp.o -c build/src/runtime/native/src/version.cpp

# BUILDING TEST RUNTIME OBJECTS

build/tests/runtime/native/CMakeFiles/sk_standalone.tests.dir/src/sk_standalone.cpp.o: tests/runtime/native/src/sk_standalone.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/sk_standalone.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/sk_standalone.tests.dir/src/sk_standalone.cpp.o -c tests/runtime/native/src/sk_standalone.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Arena-malloc.cpp.o: tests/runtime/native/src/Arena-malloc.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Arena-malloc.cpp.o -c tests/runtime/native/src/Arena-malloc.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/InternTable.cpp.o: tests/runtime/native/src/InternTable.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/InternTable.cpp.o -c tests/runtime/native/src/InternTable.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/LockManager.cpp.o: tests/runtime/native/src/LockManager.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/LockManager.cpp.o -c tests/runtime/native/src/LockManager.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Math.cpp.o: tests/runtime/native/src/Math.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Math.cpp.o -c tests/runtime/native/src/Math.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Process.cpp.o: tests/runtime/native/src/Process.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Process.cpp.o -c tests/runtime/native/src/Process.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Refcount.cpp.o: tests/runtime/native/src/Refcount.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Refcount.cpp.o -c tests/runtime/native/src/Refcount.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Regex.cpp.o: tests/runtime/native/src/Regex.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Regex.cpp.o -c tests/runtime/native/src/Regex.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/String.cpp.o: tests/runtime/native/src/String.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/String.cpp.o -c tests/runtime/native/src/String.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/System.cpp.o: tests/runtime/native/src/System.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/System.cpp.o -c tests/runtime/native/src/System.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Task.cpp.o: tests/runtime/native/src/Task.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Task.cpp.o -c tests/runtime/native/src/Task.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Type.cpp.o: tests/runtime/native/src/Type.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/Type.cpp.o -c tests/runtime/native/src/Type.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/leak.cpp.o: tests/runtime/native/src/leak.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/leak.cpp.o -c tests/runtime/native/src/leak.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/memory.cpp.o: tests/runtime/native/src/memory.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/memory.cpp.o -c tests/runtime/native/src/memory.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/objects.cpp.o: tests/runtime/native/src/objects.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/objects.cpp.o -c tests/runtime/native/src/objects.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/parallel.cpp.o: tests/runtime/native/src/parallel.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/parallel.cpp.o -c tests/runtime/native/src/parallel.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/stats.cpp.o: tests/runtime/native/src/stats.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/stats.cpp.o -c tests/runtime/native/src/stats.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/util.cpp.o: tests/runtime/native/src/util.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/util.cpp.o -c tests/runtime/native/src/util.cpp

build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/version.cpp.o: build/tests/runtime/native/src/version.cpp subbuild/lib
	mkdir -p build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/
	$(COMPILE_ROBJECT) -o build/tests/runtime/native/CMakeFiles/skip_runtime.tests.dir/src/version.cpp.o -c build/tests/runtime/native/src/version.cpp

# BUILDING THE LKG RUNTIME

LKG_RUNTIME_OBJECTS:=build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/AllocProfiler.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Arena-malloc.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Arena.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Async.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/File.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/InternTable.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/LockManager.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Math.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Obstack.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Process.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Reactive.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Refcount.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Regex.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/String.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/System.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Task.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/Type.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/intern.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/leak.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/memoize.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/memory.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/objects.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/parallel.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/stats.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/util.cpp.o build/lkg/runtime/native/CMakeFiles/skip_runtime.lkg.dir/src/version.cpp.o


build/lkg/runtime/native/libskip_runtime.a: subbuild/lib $(LKG_RUNTIME_OBJECTS)
	mkdir -p build/lkg/runtime/native/
	$(AR) qc build/lkg/runtime/native/libskip_runtime.a $(LKG_RUNTIME_OBJECTS) && $(RANLIB) build/lkg/runtime/native/libskip_runtime.a

# BUILDING THE SRC RUNTIME

SRC_RUNTIME_OBJECTS:=build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/AllocProfiler.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Arena-malloc.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Arena.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Async.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/File.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/InternTable.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/LockManager.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Math.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Obstack.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Process.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Reactive.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Refcount.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Regex.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/String.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/System.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Task.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/Type.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/intern.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/leak.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/memoize.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/memory.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/objects.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/parallel.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/stats.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/util.cpp.o build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/version.cpp.o


build/src/runtime/native/libskip_runtime.a: subbuild/lib $(SRC_RUNTIME_OBJECTS)
	mkdir -p build/src/runtime/native/
	$(AR) qc build/src/runtime/native/libskip_runtime.a $(SRC_RUNTIME_OBJECTS) && $(RANLIB) build/src/runtime/native/libskip_runtime.a

# BUILDING THE LKG BINARIES
LKG_STANDALONE:=build/lkg/CMakeFiles/skip_to_llvm.lkg.dir/runtime/native/src/sk_standalone.cpp.o
LKG_CCOPTS:=-O2 -g -DNDEBUG
LKG_LIBS:=subbuild/lib/libicuuc.a subbuild/lib/libicudata.a subbuild/lib/libpcre.a subbuild/lib/libjemalloc_pic.a -lpthread $(LIBUNWIND) -ldl

# BUILDING THE LKG PREAMBLE

build/lkg/tmp/preamble.ll: lkg/runtime/tools/gen_preamble
	mkdir -p build/lkg/tmp/
	lkg/runtime/tools/gen_preamble preamble --compiler $(CLANGXX) -o build/lkg/tmp/preamble.ll --ndebug

# BUILDING LKG SKIP_TO_LLVM

build/lkg/tmp/skip_to_llvm.gen.ll: build/lkg/tmp/preamble.ll lkg/skip_to_llvm.ll.bz2
	mkdir -p build/lkg/tmp/
	bzcat lkg/skip_to_llvm.ll.bz2 > build/lkg/tmp/skip_to_llvm.gen.body.ll
	cat build/lkg/tmp/preamble.ll > build/lkg/tmp/skip_to_llvm.gen.ll
	cat build/lkg/tmp/skip_to_llvm.gen.body.ll >> build/lkg/tmp/skip_to_llvm.gen.ll

build/lkg/tmp/skip_to_llvm.gen.o: build/lkg/tmp/skip_to_llvm.gen.ll
	mkdir -p build/lkg/tmp/
	$(CLANGXX) -fPIC -c -o build/lkg/tmp/skip_to_llvm.gen.o build/lkg/tmp/skip_to_llvm.gen.ll

build/lkg/bin/skip_to_llvm: build/lkg/tmp/skip_to_llvm.gen.o $(LKG_STANDALONE) build/lkg/runtime/native/libskip_runtime.a
	mkdir -p build/lkg/bin/
	$(CXX) $(LKG_CCOPTS) build/lkg/tmp/skip_to_llvm.gen.o $(LKG_STANDALONE) -o build/lkg/bin/skip_to_llvm build/lkg/runtime/native/libskip_runtime.a $(LKG_LIBS)

# BUILDING LKG SKIP_SERVER

build/lkg/tmp/skip_server.gen.ll: build/lkg/tmp/preamble.ll lkg/skip_server.ll.bz2
	mkdir -p build/lkg/tmp/
	bzcat lkg/skip_server.ll.bz2 > build/lkg/tmp/skip_server.gen.body.ll
	cat build/lkg/tmp/preamble.ll > build/lkg/tmp/skip_server.gen.ll
	cat build/lkg/tmp/skip_server.gen.body.ll >> build/lkg/tmp/skip_server.gen.ll

build/lkg/tmp/skip_server.gen.o: build/lkg/tmp/skip_server.gen.ll
	mkdir -p build/lkg/tmp/
	$(CLANGXX) -fPIC -c -o build/lkg/tmp/skip_server.gen.o build/lkg/tmp/skip_server.gen.ll

build/lkg/bin/skip_server: build/lkg/tmp/skip_server.gen.o $(LKG_STANDALONE) build/lkg/runtime/native/libskip_runtime.a
	mkdir -p build/lkg/bin/
	$(CXX) $(LKG_CCOPTS) build/lkg/tmp/skip_server.gen.o $(LKG_STANDALONE) -o build/lkg/bin/skip_server build/lkg/runtime/native/libskip_runtime.a $(LKG_LIBS)

# BUILDING LKG SKIP_PRINTER

build/lkg/tmp/skip_printer.gen.ll: build/lkg/tmp/preamble.ll lkg/skip_printer.ll.bz2
	mkdir -p build/lkg/tmp/
	bzcat lkg/skip_printer.ll.bz2 > build/lkg/tmp/skip_printer.gen.body.ll
	cat build/lkg/tmp/preamble.ll > build/lkg/tmp/skip_printer.gen.ll
	cat build/lkg/tmp/skip_printer.gen.body.ll >> build/lkg/tmp/skip_printer.gen.ll

build/lkg/tmp/skip_printer.gen.o: build/lkg/tmp/skip_printer.gen.ll
	mkdir -p build/lkg/tmp/
	$(CLANGXX) -fPIC -c -o build/lkg/tmp/skip_printer.gen.o build/lkg/tmp/skip_printer.gen.ll

build/lkg/bin/skip_printer: build/lkg/tmp/skip_printer.gen.o $(LKG_STANDALONE) build/lkg/runtime/native/libskip_runtime.a
	mkdir -p build/lkg/bin/
	$(CXX) $(LKG_CCOPTS) build/lkg/tmp/skip_printer.gen.o $(LKG_STANDALONE) -o build/lkg/bin/skip_printer build/lkg/runtime/native/libskip_runtime.a $(LKG_LIBS)

# UPDATE LKG

build/src/skip_to_llvm.ll: build/lkg/bin/skip_to_llvm $(shell find src/ -type f)
	mkdir -p build/src
	build/lkg/bin/skip_to_llvm --output build/src/skip_to_llvm.ll src/native:skip_to_llvm --export-function-as main=skip_main --nogoto

build/src/skip_server.ll: build/lkg/bin/skip_to_llvm $(shell find src/ -type f)
	mkdir -p build/src
	build/lkg/bin/skip_to_llvm --output build/src/skip_server.ll src/server:skip_server --export-function-as main=skip_main --nogoto

build/src/skip_printer.ll: build/lkg/bin/skip_to_llvm $(shell find src/ -type f)
	mkdir -p build/src
	build/lkg/bin/skip_to_llvm --output build/src/skip_printer.ll src/tools:skip_printer --export-function-as main=skip_main --nogoto

update_lkg: build/src/skip_to_llvm.ll build/src/skip_server.ll build/src/skip_printer.ll
	tools/check_lkg_file --update build/src/skip_to_llvm.ll lkg/skip_to_llvm.ll.bz2
	tools/check_lkg_file --update build/src/skip_server.ll lkg/skip_server.ll.bz2
	tools/check_lkg_file --update build/src/skip_printer.ll lkg/skip_printer.ll.bz2
	rsync -r lkg/ src
