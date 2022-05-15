test: test.o archive.o
	clang++ -std=c++20 -o test test.o archive.o

test.o: test.cc archive.hh
	clang++ -std=c++20 -c test.cc

archive.o: archive.cc archive.hh fileutils.hh
	clang++ -std=c++20 -c archive.cc

