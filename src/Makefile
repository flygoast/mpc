DEBUG = -g 						\
		-DWITH_DEBUG			\
		-DWITH_MPC_RESOLVER
CC = gcc
CFLAGS = $(DEBUG)  -Wall -Werror -rdynamic
INC = -I . -I /usr/local/include
LIB = -L /home/fenggu/slib -lpthread -lcares -lrt
OO = mpc.o 				\
	 mpc_core.o			\
	 mpc_signal.o		\
	 mpc_util.o 		\
	 mpc_rbtree.o 		\
	 mpc_string.o 		\
	 mpc_log.o 			\
	 mpc_array.o 		\
	 mpc_event.o 		\
	 mpc_net.o			\
	 mpc_url.o			\
	 mpc_buf.o 			\
	 mpc_connection.o 	\
	 mpc_conf.o 		\
	 mpc_http.o			\
	 mpc_stat.o
	 
TARGETS = mpc

all: $(TARGETS)

$(TARGETS): $(OO)
	$(CC) $(CFLAGS) $(OO) -o $@ $(LIBDIR) $(LIB)

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@ $(INC) $(MYSQLCFLAGS)

clean:
	rm -f *.o
	rm -f $(TARGETS)
