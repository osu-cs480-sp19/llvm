all: compiler

compiler:
	g++ -std=c++11 compiler.cpp  `llvm-config --cppflags --ldflags --libs --system-libs all` -o compiler

foo.o: compiler
	./compiler

test: test.cpp foo.o
	g++ test.cpp foo.o -o test

addRecursive.o: addRecursive.ll
	llc -filetype=obj addRecursive.ll

clean:
	rm -f compiler test *.o
