CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lsqlite3 -lcurl

SRCS = sensors.c bmp280.c aht20.c ssd1315.c db.c firestore.c
OBJS = $(SRCS:.c=.o)

sensors: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

sensors.o:    sensors.c bmp280.h aht20.h ssd1315.h db.h firestore.h
bmp280.o:     bmp280.c  bmp280.h
aht20.o:      aht20.c   aht20.h
ssd1315.o:    ssd1315.c ssd1315.h
db.o:         db.c      db.h
firestore.o:  firestore.c firestore.h

clean:
	rm -f sensors $(OBJS)

.PHONY: clean
