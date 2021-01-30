########################################################################
# This is the makefile for NetPipe
# Simply type make with one of the following choices for environments:
#
#      mpi         : will use mpicc to compile
#      mplite      : It will look for the MP_Lite library in $HOME/mplite
#      tcp         : You start the receiver and transmitter manually
#      paragon     : Uses MPI on the Paragon
#      pvm         : Old version doesn't use pvm_spawn
#                    Use 'NPpvm -r' on receiver and 'NPpvm' on transmitter
#      tcgmsg      : Run directly on TCGMSG
#      tcgmsg-mpich: Test TCGMSG layer on top of mpich
#      lapi        : Test the LAPI interface on the IBM SP
#      mx          : Directly measure MX / Open-MX
#      gm          : Directly measure raw GM on Myrinet
#                    Use 'NPgm -r' on receiver and 'NPgm -t -h ...' on trans
#      shmem       : Directly measure SHMEM on Cray and SGI systems
#      gpshmem     : Measure GPSHMEM on any other system using shmem.c
#
#      For more information, see the function printusage() in netpipe.c
#
########################################################################

CC         = cc
CFLAGS     = -O -g
SRC        = ./src

# For MPI, mpicc will set up the proper include and library paths

MPICC       = mpicc

MP_Lite_home   = $(HOME)/MP_Lite

PVM_HOME   = /usr/share/pvm3
PVM_ARCH   = LINUX
#PVM_ARCH   = LINUXALPHA

TCGMSG_HOME = $(HOME)/np/packs/ga
TCGMSG_LIB = $(TCGMSG_HOME)/lib/LINUX/libtcgmsg.a
TCGMSG_INC = $(TCGMSG_HOME)/include

TCGMSG_MPI_HOME= $(HOME)/np/ga
TCGMSG_MPI_LIB = $(TCGMSG_MPI_HOME)/lib/LINUX/libtcgmsg-mpi.a
TCGMSG_ARMCI_LIB = $(TCGMSG_MPI_HOME)/armci-1.0/lib/LINUX/libarmci.a
TCGMSG_MPI_INC = $(TCGMSG_MPI_HOME)/include

GM_HOME = /opt/gm
GM_INC = $(GM_HOME)/include
GM_LIB = -L $(GM_HOME)/lib -lgm
GM_DRI = $(GM_HOME)/drivers/linux/gm

MX_HOME = /opt/mx
MX_INC = $(MX_HOME)/include
MX_LIB = -L$(MX_HOME)/lib -lmyriexpress -lpthread

GPSHMEM_LIB = $(HOME)/np/ga/gpshmem/lib/libgpshmem.a
GPSHMEM_INC = $(HOME)/np/ga/gpshmem/include

ARMCI_LIB   = $(HOME)/armci/lib/LINUX/libarmci.a -lm
ARMCI_INC   = $(HOME)/armci/src

# MTHOME should be defined in the environment
#MTHOME=/usr/mellanox
VAPI_INC    = $(MTHOME)/include
VAPI_LIB    = $(MTHOME)/lib

# Modify UDAPL_INC & UDAPL_LIB to match your uDAPL installation.
# Currently defined for (OpenIB) 64-bit OFED 1.0
UDAPL_INC   = /usr/local/ofed/include
UDAPL_LIB   = /usr/local/ofed/lib64

# If the MPI-2 implementation provides the mpicc compiler, then simply
# set MPI2CC to mpicc, and set MPI2_LIB and MPI2_INC to nothing.
# If mpicc is not included with the implementation, then set MPI2CC to
# an appropriate compiler, and set the paths to the MPI-2 library and include
# directory (library path should be absolute, e.g. MPI2_LIB = 
# $(HOME)/mpi/libmpi.a)

MPI2CC   = mpicc
MPI2_LIB =
MPI2_INC =

all:tcp 

clean:
	rm -f *.o NP* np.out

#
# This section of the Makefile is for compiling the binaries
#


mtcp: $(SRC)/tcp.c $(SRC)/netpipe.c $(SRC)/netpipe.h 
	$(CC) $(CFLAGS) $(SRC)/netpipe.c $(SRC)/tcp.c -DTCP -o NPtcp -I$(SRC)