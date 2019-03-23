DEPLIB = ../../build/deps/jansson/lib
DEPH = ../../build/deps/jansson/include

main: tft.c shash.c lhash.c
	cc -g -L $(DEPLIB) -I . -I $(DEPH) -o main shash.c lhash.c tft.c -ljansson

clean:
	rm main simple
    
