TARGETS = server client
CC=@CC@ -g -O3 -Wall -Werror -fgnu89-inline
DPDK=@DPDK@
PS=@PSIO@
NETMAP=@NETMAP@
ONVM=@ONVM@
CCP=@CCP@
CFLAGS=@CFLAGS@

LD		= gcc
LDFLAGS = -L/usr/local/lib/ 

# Add arch-specific optimization
ifeq ($(shell uname -m),x86_64)
LIBS += -m64
endif

# mtcp library and header 
MTCP_FLD    =~/yangyihan/mtcp/mtcp/
MTCP_INC    =-I${MTCP_FLD}/include
MTCP_LIB    =-L${MTCP_FLD}/lib
MTCP_TARGET = ${MTCP_LIB}/libmtcp.a

UTIL_FLD = ~/yangyihan/mtcp/util
UTIL_INC = -I${UTIL_FLD}/include
UTIL_OBJ = ${UTIL_FLD}/http_parsing.o ${UTIL_FLD}/tdate_parse.o ${UTIL_FLD}/netlib.o

# util library and header
INC = -I./include/ ${UTIL_INC} ${MTCP_INC} -I${UTIL_FLD}/include
LIBS = ${MTCP_LIB}

# psio-specific variables
ifeq ($(PS),1)
PS_DIR = ~/yangyihan/mtcp/io_engine/
PS_INC = ${PS_DIR}/include
INC += -I{PS_INC}
LIBS += -lmtcp -L${PS_DIR}/lib -lps -lpthread -lnuma -lrt
endif

# netmap-specific variables
ifeq ($(NETMAP),1)
LIBS += -lmtcp -lpthread -lnuma -lrt
endif

# dpdk-specific variables
ifeq ($(DPDK),1)
DPDK_MACHINE_LINKER_FLAGS=$${RTE_SDK}/$${RTE_TARGET}/lib/ldflags.txt
DPDK_MACHINE_LDFLAGS=$(shell cat ${DPDK_MACHINE_LINKER_FLAGS})
LIBS += -g -O3 -pthread -lrt -march=native ${MTCP_FLD}/lib/libmtcp.a -lnuma -lmtcp -lpthread -lrt -ldl -lgmp -L${RTE_SDK}/${RTE_TARGET}/lib ${DPDK_MACHINE_LDFLAGS}
endif

# onvm-specific variables
ifeq ($(ONVM),1)
ifeq ($(RTE_TARGET),)
$(error "Please define RTE_TARGET environment variable")
endif

INC += -I@ONVMLIBPATH@/onvm_nflib
INC += -I@ONVMLIBPATH@/lib
INC += -DENABLE_ONVM
LIBS += @ONVMLIBPATH@/onvm_nflib/$(RTE_TARGET)/libonvm.a
LIBS += @ONVMLIBPATH@/lib/$(RTE_TARGET)/lib/libonvmhelper.a -lm
endif

ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif

ifeq ($(CCP), 1)
# LIBCCP
LIBCCP = $(MTCP_FLD)/src/libccp
LIBS += -L$(LIBCCP) -lccp -lstartccp
INC += -I$(LIBCCP)
endif

server.o: server.c
	$(MSG) "   CC $<"
	$(HIDE) ${CC} -c $< ${CFLAGS} ${INC}

server: server.o ${MTCP_FLD}/lib/libmtcp.a
	$(MSG) "   LD $<"
	$(HIDE) ${CC} $< ${LIBS} ${UTIL_OBJ} -o $@

client.o: client.c include/*.h
		$(CC) -c $(CFLAGS) $< -o $@

client: client.o
		$(LD) $(LDFLAGS) client -o $(TARGET) $(LIBS)

clean:
		rm -f *.o $(TARGET)