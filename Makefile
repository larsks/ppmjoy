CFLAGS=-Wno-unused-but-set-variable -Wall -g

ppm: ppm.o
	gcc ppm.o -o ppm -lasound

clean:
	rm -f ppm.o ppm
