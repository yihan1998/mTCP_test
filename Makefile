TARGETS = server client
CC		= g++
DPDK	= 1
PS		= 0
NETMAP	= 0
ONVM	= 0
CCP		= 0
CFLAGS	= -std=c++11 -g -O3 -fkeep-inline-functions #-fgnu89-inline

# Add arch-specific optimization
ifeq ($(shell uname -m),x86_64)
LIBS += -m64
endif

# mtcp library and header 
MTCP_FLD    =../mtcp/mtcp
MTCP_INC    =-I${MTCP_FLD}/include -I${MTCP_FLD}/src/include
MTCP_LIB    =-L${MTCP_FLD}/lib
MTCP_TARGET = ${MTCP_LIB}/libmtcp.a

UTIL_FLD = ../mtcp/util
UTIL_INC = -I${UTIL_FLD}/include
UTIL_OBJ = ${UTIL_FLD}/http_parsing.o ${UTIL_FLD}/tdate_parse.o ${UTIL_FLD}/netlib.o

#HiKV library and header
HIKV_INC	= -I./Hikv/ntstore -I./Hikv/mem -I./Hikv/lib -I./Hikv/obj -I./Hikv/tbb -I./Hikv/pmdk/include 
HIKV_LIB	= -L/usr/local/lib/ -L ./third-party/jemalloc-4.2.1/lib -L ./third-party/tbb 
HIKV_SRC	= ./Hikv/obj/threadpool.cc ./Hikv/obj/btree.cc ./Hikv/mem/pm_alloc.cc ./Hikv/lib/city.cc ./Hikv/lib/pflush.c ./Hikv/ntstore/ntstore.c
HIKV_OBJ	= threadpool.o btree.o pm_alloc.o city.o pflush.o ntstore.o 

# util library and header
INC = -I./include/ ${UTIL_INC} $(HIKV_INC) ${MTCP_INC} -I${UTIL_FLD}/include 
LIBS = $(HIKV_LIB) ${MTCP_LIB} -lpthread -ljemalloc -ltbb -lpmem

# psio-specific variables
ifeq ($(PS),1)
PS_DIR = ../mtcp/io_engine/
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
DPDK_INC = $${RTE_SDK}/$${RTE_TARGET}/include
INC += -I{DPDK_INC}
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

CLI_INC = -Iinclude 
CLI_LIB = -L/usr/local/lib/ -L ./third-party/jemalloc-4.2.1/lib -L ./third-party/tbb -lpthread -ljemalloc -ltbb -lpmem

server.o: server.cc $(HIKV_SRC)
	${CC} -c $^ ${CFLAGS} ${INC}

server: 
	${CC} $(HIKV_OBJ) server.o ${MTCP_FLD}/lib/libmtcp.a $< ${LIBS} ${UTIL_OBJ} -o $@

server_all:
	make server.o && make server

client:
	$(CC) -std=c++11 client.cc $(CLI_INC) -o $@ $(CLI_LIB)

clean:
		rm -f *.o $(TARGET)