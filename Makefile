all: compiler.cpp
	g++ -std=c++11 compiler.cpp  `llvm-config --cppflags --ldflags --libs --system-libs all` -o compiler

test: test.cpp addRecursive.o
	g++ test.cpp addRecursive.o -o test

addRecursive.o: addRecursive.ll
	llc -filetype=obj addRecursive.ll

clean:
	rm -f compiler test *.o
