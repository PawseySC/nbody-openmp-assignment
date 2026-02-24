# here the make accepts the compiler, gpu, calctype
# will add a make print to indicate what types of compilations
# are enabled.
COMPILERTYPE ?= GCC
GPUTYPE ?= NVIDIA
OPTLEVEL ?= 2
VISUALTYPE ?= STANDARD
PROFILER ?= OFF

# optmisation flags
OPTFLAGS = -O$(OPTLEVEL)
# profiling flags if desired 
ifeq ($(PROFILER), ON)
	OPTFLAGS += -pg -g
endif

# formatting characters for info output
NULL := 
TAB := $(NULL)  $(NULL)

# lets define a bunch of compilers
GCC = gcc
GCCCXX = g++
GCCFORT = gfortran
CLANG = clang
CLANGCXX = clang++
CLANGFORT = flang
AOMP = aompcc
AOMPCXX = aompcc
CRAYCC = cc 
CRAYCXX = CC
CRAYFORT = ftn 

# C flags. Currently only really need to enforce c11 standard
GCCCFLAGS = -std=c11

# fortran flags 
GCCFFLAGS = -cpp -ffixed-line-length-none -dM

# mpi
MPIFORT = mpif90

# openmp 
OMP_FLAGS = -fopenmp -DUSEOPENMP
OMPTARGET_FLAGS = -fopenmp -DUSEOPENMPTARGET

#now set the default compilers
CC = $(GCC)
CXX = $(GCCCXX)
FORT = $(GCCFORT)
FFLAGS = $(GCCFFLAGS)
CFLAGS = $(GCCCFLAGS)

# change if required
ifeq ($(COMPILERTYPE), CRAY-GNU)
	CC = $(CRAYCC)
	CXX = $(CRAYCXX)
	FORT = $(CRAYFORT)
endif
ifeq ($(COMPILERTYPE), CLANG)
	CC = $(CLANG)
	CXX = $(CLANGCXX)
	FORT = $(CLANGFORT)        
endif
ifeq ($(COMPILERTYPE), CRAY)
	CC = $(CRAYCC)
	CXX = $(CRAYCXX)
	CFLAGS =
	FORT = $(CRAYFORT)
	FFLAGS = -eZ -ffree
endif

# compiler used for openmp
OMPCC = $(CC)
OMPCXX = $(CXX)
OMPFORT = $(FORT)

# compilers used for openacc
OACCCC = $(CC)
OACCCXX = $(CXX)

VISUALFLAGS =
# here this flag may need to be updated with PNG location
PNGDIR ?= /opt/homebrew/
PNGLIB ?= -lpng
ifeq ($(VISUALTYPE), PNG)
	VISUALFLAGS += $(PNGLIB) -I$(PNGDIR)/include/ -L$(PNGDIR)/lib/ -DUSEPNG
endif

COMMONFLAGS = $(OPTFLAGS) $(VISUALFLAGS) -g

.PHONY : allinfo configinfo buildinfo makecommands clean
.PHONY : cpu_serial 
.PHONY : cpu_openmp cpu_openmp_loop cpu_openmp_task
.PHONY : cpu_mpi
.PHONY : gpu_openmp

all : buildinfo cpu_serial 

allinfo : configinfo makecommands buildinfo 

# have it spit out information about current configuration
configinfo : 
	$(info )
	$(info ========================================)
	$(info Compiler options:)
	$(info ========================================)
	$(info Allowed COMPILERTYPE: GCC, CLANG, CRAY, CRAY-GNU)
	$(info Allowed GPUTYPE: NVIDIA, AMD)
	$(info )
	$(info Other options:)
	$(info ----------------------------------------)
	$(info One can also specify optmisation level with OPTLEVEL=)
	$(info one can use 0, 1, 2, 3, or fast)
	$(info )
	$(info One can also turn on profiling flags using PROFILER=ON)
	$(info )
	$(info One can also specify using PNG visualisation of GOL with VISUALTYPE=PNG)
	$(info but does require setting the PNGDIR to point to the base directory)
	$(info where include/png.h and lib/libpng are located.)
	$(info if the png library has a non-standard name like /usr/lib/libpng16.so.16)
	$(info set PNGLIB= to the non-standard name)
	$(info ========================================)
	$(info )


# list current build info given the commands make was passed
buildinfo :
	$(info )
	$(info ========================================)
	$(info Current compilers to be used:)
	$(info ========================================)
	$(info Compiling with $(CC) $(CXX) $(FORT) for CPU focused codes)
	$(info Compiling with $(MPICC) $(MPICXX) $(MPIFORT) for MPI-CPU focused codes)
	$(info Compiling with $(GPUCC) $(GPUCXX) for GPU focused codes)
	$(info Compiling with $(OMPCC) $(OMPCXX) $(OMPFORT) for OpenMP directive GPU focused codes)
	$(info ========================================)
	$(info )

# list current build info given the commands make was passed
makecommands :
	$(info )
	$(info ========================================)
	$(info Make commands:)
	$(info ========================================)
	$(info Make is configured so that the following can be compiled if provided this argument)
	$(info )
	$(info cpu_serial : compiles both C and Fortran verisons of the standard serial code. )
	$(info $(TAB)Sources : 01_nbody_cpu_serial.c or 01_nbody_cpu_serial.f90)
	$(info cpu_serial_expanded : compiles C verison of serial code setup to allow for easy expansion of rules. )
	$(info $(TAB)Sources : 01_nbody_cpu_serial_expanded.c)
	$(info )
	$(info cpu_openmp : compiles both C and Fortran verisons cpu parallelised openmp codes.)
	$(info Will try compiling both loop and task based parallelism )
	$(info $(TAB)Sources : 02_nbody_cpu_openmp_loop.c or 02_nbody_cpu_openmp_loop.f90)
	$(info $(TAB)          02_nbody_cpu_openmp_task.c or 02_nbody_cpu_openmp_task.f90)
	$(info cpu_openmp_loop : compiles both C and Fortran verisons cpu parallelised openmp codes.)
	$(info Will try compiling both loop and task based parallelism )
	$(info $(TAB)Sources : 02_nbody_cpu_openmp_loop.c or 02_nbody_cpu_openmp_loop.f90)
	$(info )
	$(info gpu_openmp : compiles Fortran verisons gpu parallelised openmp codes.)
	$(info ----------------------------------------)
	$(info NOTE:: You can append to the make argument _cc or  to only compile the c or fortran versions.)
	$(info For example)
	$(info cpu_serial_cc : compiles ONLY C version of serial code. )
	$(info ========================================)
	$(info )


clean :
	rm -f obj/*
	rm -f bin/*
	rm -f src/nbody_common.mod 
	rm -f nbody_common.mod

# just make an easier make name to remember
cpu_serial : bin/01_nbody_cpu_serial bin/01_nbody_cpu_serial_c
cpu_openmp : cpu_openmp_loop cpu_openmp_task cpu_openmp_loop_c cpu_openmp_task_c
cpu_openmp_loop : bin/02_nbody_cpu_openmp_loop
cpu_openmp_task : bin/02_nbody_cpu_openmp_task
cpu_openmp_loop_c : bin/02_nbody_cpu_openmp_loop_c
cpu_openmp_task_c : bin/02_nbody_cpu_openmp_task_c
# gpu related 
gpu_openmp : bin/02_nbody_gpu_openmp

obj/common.o : src/common.f90 
	$(FORT) $(COMMONFLAGS) $(FFLAGS) $(OMP_FLAGS) -c src/common.f90 -o obj/common.o

obj/common_c.o : src/common.h src/common.c 
	$(CC) $(COMMONFLAGS) $(CFLAGS) -c src/common.c -o obj/common_c.o

bin/01_nbody_cpu_serial : src/01_nbody_cpu_serial.f90 obj/common.o
	$(FORT) $(COMMONFLAGS) $(FFLAGS) -c src/01_nbody_cpu_serial.f90 -o obj/01_nbody_cpu_serial.o
	$(FORT) $(COMMONFLAGS) $(FFLAGS) $(OMP_FLAGS) -o bin/01_nbody_cpu_serial obj/01_nbody_cpu_serial.o obj/common.o

bin/01_nbody_cpu_serial_c : src/common.h src/01_nbody_cpu_serial.c obj/common_c.o
	$(CC) $(COMMONFLAGS) $(CFLAGS) -c src/01_nbody_cpu_serial.c -o obj/01_nbody_cpu_serial_c.o
	$(CC) $(COMMONFLAGS) $(CFLAGS) -o bin/01_nbody_cpu_serial_c obj/01_nbody_cpu_serial_c.o obj/common_c.o


obj/common_omp.o : src/common.f90 
	$(FORT) $(COMMONFLAGS) $(FFLAGS) $(OMP_FLAGS) -c src/common.f90 -o obj/common_omp.o

obj/common_c_omp.o : src/common.h src/common.c 
	$(CC) $(COMMONFLAGS) $(CFLAGS) $(OMP_FLAGS) -c src/common.c -o obj/common_c_omp.o

# fortran openmp codes
bin/02_nbody_cpu_openmp_loop : src/02_nbody_cpu_openmp_loop.f90 obj/common_omp.o
	$(OMPFORT) $(OMP_FLAGS) $(COMMONFLAGS) $(FFLAGS) -c src/02_nbody_cpu_openmp_loop.f90 -o obj/02_nbody_cpu_openmp_loop.o
	$(OMPFORT) $(OMP_FLAGS) $(COMMONFLAGS) $(FFLAGS) -o bin/02_nbody_cpu_openmp_loop obj/02_nbody_cpu_openmp_loop.o obj/common.o

bin/02_nbody_cpu_openmp_task : src/02_nbody_cpu_openmp_task.f90 obj/common_omp.o
	$(OMPFORT) $(OMP_FLAGS) $(COMMONFLAGS) $(FFLAGS) -c src/02_nbody_cpu_openmp_task.f90 -o obj/02_nbody_cpu_openmp_task.o
	$(OMPFORT) $(OMP_FLAGS) $(COMMONFLAGS) $(FFLAGS) -o bin/02_nbody_cpu_openmp_task obj/02_nbody_cpu_openmp_task.o obj/common.o

# c version of openmp codes
bin/02_nbody_cpu_openmp_loop_c : src/common.h src/02_nbody_cpu_openmp_loop.c obj/common_c_omp.o
	$(CC) $(COMMONFLAGS) $(CFLAGS) $(OMP_FLAGS) -c src/02_nbody_cpu_openmp_loop.c -o obj/02_nbody_cpu_openmp_loop_c.o
	$(CC) $(COMMONFLAGS) $(CFLAGS) $(OMP_FLAGS) -o bin/02_nbody_cpu_openmp_loop_c obj/02_nbody_cpu_openmp_loop_c.o obj/common_c.o

bin/02_nbody_cpu_openmp_task_c : src/common.h src/02_nbody_cpu_openmp_task.c obj/common_c_omp.o
	$(CC) $(COMMONFLAGS) $(CFLAGS) $(OMP_FLAGS) -c src/02_nbody_cpu_openmp_task.c -o obj/02_nbody_cpu_openmp_task_c.o
	$(CC) $(COMMONFLAGS) $(CFLAGS) $(OMP_FLAGS) -o bin/02_nbody_cpu_openmp_task_c obj/02_nbody_cpu_openmp_task_c.o obj/common_c.o

# gpu related
bin/02_nbody_gpu_openmp : src/02_nbody_gpu_openmp.f90 obj/common.o
	$(OMPFORT) $(OMP_FLAGS) $(COMMONFLAGS) $(FFLAGS) -c src/02_nbody_gpu_openmp.f90 -o obj/02_nbody_gpu_openmp.o
	$(OMPFORT) $(OMP_FLAGS) $(COMMONFLAGS) $(FFLAGS) -o bin/02_nbody_gpu_openmp obj/02_nbody_gpu_openmp.o obj/common.o
