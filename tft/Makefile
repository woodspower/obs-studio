DEPLIB = ../build/deps/jansson/lib
DEPH = ../build/deps/jansson/include

main: tft.c
	cc -g -L $(DEPLIB) -I . -I $(DEPH) -o main simple_hash.c long_hash.c tft.c -ljansson

	
simple: simple_parse.c
	cc -g -L $(DEPLIB) -I $(DEPH) -o simple simple_parse.c -ljansson

clean:
	rm main simple
    
