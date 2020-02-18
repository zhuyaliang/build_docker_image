CC=gcc  
MAINC = main.c
EXEC = mkdimage 

CFLAGS = `pkg-config --cflags --libs gtk+-3.0 json-c`
main:
	$(CC)  $(MAINC)  -o $(EXEC) $(CFLAGS)
clean:
	rm $(EXEC) -rf
