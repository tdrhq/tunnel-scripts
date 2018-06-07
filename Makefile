

FILES=io_loop.c \
      lcat.c \
      my_socket.c

HEADERS=io_loop.h \
      my_socket.h

all: lcat

lcat: $(FILES) $(HEADERS)
	gcc $(FILES) -o lcat

clean:
	rm lcat
