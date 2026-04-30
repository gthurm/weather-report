CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lsqlite3 -lm

SRCS = main.c db.c chart.c kitty.c
OBJS = $(SRCS:.c=.o)

weather-tui: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

main.o:   main.c   db.h chart.h kitty.h
db.o:     db.c     db.h
chart.o:  chart.c  chart.h
kitty.o:  kitty.c  kitty.h

clean:
	rm -f weather-tui $(OBJS)

.PHONY: clean
