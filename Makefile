diskprint: src/*.h src/*.c
	gcc src/*.c -o diskprint

run: diskprint
	valgrind --tool=memcheck --leak-check=full ./diskprint disk1

clean:
	rm diskprint