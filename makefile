.SUFFIXES:.c .o

CC=gcc

SRCS=tmis_server.c log.c threadpool.c tmis_io.c tmis_enc_denc.c
OBJS=$(SRCS:.c=.o)
EXEC=tmis_server



start: $(OBJS) 
	$(CC) -o $(EXEC) $(OBJS) -lpthread -lcrypto -L/usr/local/lib/ -lgmp -lpbc -I/usr/include/mysql/ -L/usr/lib64/mysql/ -lmysqlclient
	@echo "------------------ok---------------"

.c.o:
	$(CC) -g -Wall -o $@ -c $<

clean:
	rm -rf $(EXEC) $(OBJS) 