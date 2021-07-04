#
# Makefile for libmynet
#
OBJS = \
	mynet/init_tcpserver.o \
	mynet/init_tcpclient.o \
	mynet/init_udpserver.o \
	mynet/init_udpclient.o \
	mynet/other.o
AR = ar -qc

libmynet.a : ${OBJS}
	${RM} mynet/$@
	${AR} mynet/$@ ${OBJS}

${OBJS}: mynet/mynet.h

task% : libmynet.a
	gcc $@/$@.c -Lmynet -lmynet -o $@.out -Imynet -pthread

clean:
	${RM} mynet/*.o
