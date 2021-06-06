#
# Makefile for libmynet
#
OBJS = mynet/init_tcpserver.o mynet/init_tcpclient.o mynet/other.o
AR = ar -qc

libmynet.a : ${OBJS}
	${RM} $@
	${AR} mynet/$@ ${OBJS}

${OBJS}: mynet/mynet.h

task% : libmynet.a
	gcc $@/$@.c mynet/libmynet.a -o $@/$@ -Imynet

clean:
	${RM} mynet/*.o
