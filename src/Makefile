DEBUG = -g -DWITH_DEBUG -DWITH_MPC_RESOLVER
CC = gcc
CFLAGS = $(DEBUG)  -Wall -Werror
INC = -I .
LIB = -lpthread -lcares
OO = mpc.o 				\
	 mpc_core.o			\
	 mpc_util.o 		\
	 mpc_log.o 			\
	 mpc_array.o 		\
	 mpc_event.o 		\
	 mpc_net.o			\
	 mpc_url.o			\
	 mpc_buf.o 			\
	 mpc_connection.o 	\
	 mpc_http.o
	 
TARGETS = mpc

all: $(TARGETS)

$(TARGETS): $(OO)
	$(CC) $(CFLAGS) $(OO) -o $@ $(LIBDIR) $(LIB)

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@ $(INC) $(MYSQLCFLAGS)

clean:
	rm -f *.o
	rm -f $(TARGETS)
