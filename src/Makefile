OBJECTS = routed.o str_helper.o config.o ht.o buf_ctrl.o parser.o \
          client_info.o socket_op.o rt_ctrl.o dbg_helper.o forward.o \
          resp.o lsa.o dijkstra.o darr.o pft.o sel_timeout.o

CC = gcc -g -Wall -Werror

routed: $(OBJECTS)
	$(CC) -o routed $(OBJECTS)

routed.o: routed.c
	$(CC) -c routed.c

str_helper.o: str_helper.c str_helper.h
	$(CC) -c str_helper.c

dbg_helper.o: dbg_helper.c dbg_helper.h
	$(CC) -c dbg_helper.c

config.o: config.c config.h
	$(CC) -c config.c

ht.o: ht.c ht.h
	$(CC) -c ht.c

buf_ctrl.o: buf_ctrl.c buf_ctrl.h
	$(CC) -c buf_ctrl.c

parser.o: parser.c parser.h
	$(CC) -c parser.c

client_info.o: client_info.c client_info.h
	$(CC) -c client_info.c

socket_op.o: socket_op.c socket_op.h
	$(CC) -c socket_op.c

rt_ctrl.o: rt_ctrl.c rt_ctrl.h
	$(CC) -c rt_ctrl.c

forward.o: forward.c forward.h
	$(CC) -c forward.c

resp.o: resp.c resp.h
	$(CC) -c resp.c

las.o: lsa.c lsa.h
	$(CC) -c lsa.c

dijkstra.o: dijkstra.c dijkstra.h
	$(CC) -c dijkstra.c

darr.o: darr.c darr.h
	$(CC) -c darr.c

pft.o: pft.c pft.h
	$(CC) -c pft.c

sel_timeout.o: sel_timeout.c sel_timeout.h
	$(CC) -c sel_timeout.c 

clean:
	rm $(OBJECTS) routed
 
