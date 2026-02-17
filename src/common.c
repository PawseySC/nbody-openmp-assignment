
/** @file common.c
 *  @brief common functions for nbody code, including timing and memory usage reporting, and core binding reporting.
 */

#include "common.h"

/// \defgroup Utilities 
//@{
/** Initialize a start time 
 * @returns The current wall time as a struct timeval.
 */
struct timeval init_time(){
    struct timeval curtime;
    gettimeofday(&curtime, NULL);
    return curtime;
}

/** Get the elapsed time relative to start, return current wall time
 * @param start The starting time to compare against.
 * @returns The current wall time as a struct timeval.
 */
struct timeval get_elapsed_time(struct timeval start){
    struct timeval curtime, delta;
    gettimeofday(&curtime, NULL);
    delta.tv_sec = curtime.tv_sec - start.tv_sec;
    delta.tv_usec = curtime.tv_usec - start.tv_usec;
    double deltas = delta.tv_sec+delta.tv_usec/1e6;
    printf("Elapsed time %f s\n", deltas);
    return curtime;
}

/** Get the memory used at a specified point in the code. 
 * This function reads the memory usage information from the /proc/self/status file on Linux systems and reports it. 
 * It will not work on Apple (macOS) systems, where it will simply report -1 as memory usage.
 * @param where A string describing the context for the memory usage report (e.g., function name).
 * 
 */
void report_memory_usage(char where[1000]) 
{
    // apple does not provide /proc/self/status file so 
    // just ignore mem usage 
    struct memusage
    {
        size_t mem[4];
        //vm_current, vm_peak, rss_current, rss_peak;
    } usage;

    usage.mem[0] = usage.mem[1] = usage.mem[2] = usage.mem[3] = -1; // default to -1 for Apple systems

    const char *stat_file = "/proc/self/status";
    FILE *fp;
    char line[10000] = {0};
    char searchstr[4][100] = {"VmSize:", "VmPeak:", "VmRSS:", "VmHWM:"};
    char tmpstr[100];
    int i;
    #ifndef __APPLE__
    fp = fopen (stat_file, "r");
    if (! fp) {
        printf("========================================================= \n");
        printf("Couldn't open %s for memory usage reading\n",stat_file);
        printf("========================================================= \n");
        return;
    }
    while (fgets(line, 10000, fp))
    {
	for (i=0;i<4;i++) 
        {
	    if (strstr(line,searchstr[i]))
            {
                sscanf(&line[7],"%zu", &usage.mem[i]);
            }
        }
    }
    fclose(fp);
    #else
    printf("Mem reporting still in dev for Apple systems. Results will all be -1\n");
    #endif
    printf("========================================================= \n");
    printf("Memory Usage @ %s \n (VM, Peak VM, RSS, Peak RSS) = (%zu, %zu, %zu, %zu) kilobytes \n", 
        where, usage.mem[0], usage.mem[1], usage.mem[2], usage.mem[3]);
    printf("========================================================= \n");
    printf(" \n");
}

#ifdef __APPLE__

static inline void
CPU_ZERO(my_cpu_set_t *cs) { cs->count = 0; }

static inline void
CPU_SET(int num, my_cpu_set_t *cs) { cs->count |= (1 << num); }

static inline int
CPU_ISSET(int num, my_cpu_set_t *cs) { return (cs->count & (1 << num)); }

int sched_getaffinity(pid_t pid, size_t cpu_size, my_cpu_set_t *cpu_set)
{
  int32_t core_count = 0;
  size_t  len = sizeof(core_count);
  int ret = mysysctlbyname(SYSCTL_CORE_COUNT, &core_count, &len, 0, 0);
  int i;
  if (ret) {
    printf("error while get core count %d\n", ret);
    return -1;
  }
  cpu_set->count = 0;
  for (i = 0; i < core_count; i++) {
    cpu_set->count |= (1 << i);
  }
  return 0;
}

//#ifdef __APPLE__
int mysysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp,
	     size_t newlen)
{
	int name2oid_oid[2];
	int real_oid[CTL_MAXNAME+2];
	int error;
	size_t oidlen;

	name2oid_oid[0] = 0;	/* This is magic & undocumented! */
	name2oid_oid[1] = 3;

	oidlen = sizeof(real_oid);
	error = sysctl(name2oid_oid, 2, real_oid, &oidlen, (void *)name,
		       strlen(name));
	if (error < 0) 
		return error;
	oidlen /= sizeof (int);
	error = sysctl(real_oid, oidlen, oldp, oldlenp, newp, newlen);
	return (error);
}

#endif 

#ifdef __APPLE__
void cpuset_to_cstr(my_cpu_set_t *mask, char *str)
#else
void cpuset_to_cstr(cpu_set_t *mask, char *str)
#endif
{
  char *ptr = str;
  int i, j, entry_made = 0;
  int32_t core_count;
#ifdef __APPLE__
  size_t len = sizeof(core_count);
  mysysctlbyname(SYSCTL_CORE_COUNT, &core_count, &len, 0, 0);
#else 
  core_count = get_nprocs();
#endif
  for (i = 0; i < core_count; i++) {
    if (CPU_ISSET(i, mask)) {
      int run = 0;
      entry_made = 1;
      for (j = i + 1; j < core_count; j++) {
        if (CPU_ISSET(j, mask)) run++;
        else break;
      }
      if (!run)
        sprintf(ptr, "%d ", i);
      else if (run == 1) {
        sprintf(ptr, "%d,%d ", i, i + 1);
        i++;
      } else {
        sprintf(ptr, "%d-%d ", i, i + run);
        i += run;
      }
      while (*ptr != 0) ptr++;
    }
  }
  ptr -= entry_made;
  ptr = NULL;
}

/** @brief Reports the core binding 
 * This function checks the CPU affinity of the current process and reports which cores it is bound to. 
 * It also reports the MPI rank and OpenMP thread number if applicable.
 */
void report_core_binding()
{
    int rank, size;
#ifdef _MPI
    //find out how big the SPMD world is
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    //and this processes' rank is
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
#else 
    rank=0, size=1;
#endif
    char binding_report[10*1024+256] = {"Core binding report:\n "};
#ifdef __APPLE__
    my_cpu_set_t coremask;
#else
    cpu_set_t coremask;
#endif
    char clbuf[7 * 1024], hnbuf[64];
    memset(clbuf, 0, sizeof(clbuf));
    memset(hnbuf, 0, sizeof(hnbuf));
    (void)gethostname(hnbuf, sizeof(hnbuf));
#if defined(_OPENMP)&&defined(USEOPENMP)
    #pragma omp parallel shared (binding_report) private(coremask, clbuf) 
#endif
    {
        char result[9 * 1024 + 256],tmpstr[8 * 1024 + 256];
        (void)sched_getaffinity(0, sizeof(coremask), &coremask);
        cpuset_to_cstr(&coremask, clbuf);
        sprintf(tmpstr, "On node %s : ",hnbuf);
        strcat(result,tmpstr);
        sprintf(tmpstr,"MPI Rank %d : ", rank);
        strcat(result,tmpstr);
        int thread = 0;
#if defined(_OPENMP)&&defined(USEOPENMP)
        thread = omp_get_thread_num();
#endif
        sprintf(tmpstr," Thread %d : ",thread);
        strcat(result,tmpstr);
        sprintf(tmpstr, " Core affinity = %s\n", clbuf);
        strcat(result,tmpstr);
#if defined(_OPENMP)&&defined(USEOPENMP)
        #pragma omp critical 
#endif 
        {
            strcat(binding_report,result);
        }
    }
    printf("========================================================= \n");
    printf("%s",binding_report);
    printf("========================================================= \n");
    printf(" \n");
#ifdef _MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif
}
//@}

/**
 * @brief Calculates the Euclidean distance between two 3D vectors.
 * 
 * This function computes the magnitude of the vector resulting from
 * the difference between pos1 and pos2.
 *
 * @param pos1 The first input vector.
 * @param pos2 The second input vector.
 * @return Euclidean distance.
 */
double get_dist(const double pos1[3], const double pos2[3]) {
    double rad_squared = 0.0;
    int i;
    for (i = 0; i < 3; i++) {
        double diff = pos1[i] - pos2[i];
        rad_squared += diff * diff;
   }
    return sqrt(rad_squared);
}

/**
 * @brief Generates tangential velocity to a mass at a given position
 * 
 * This function creates a velocity vector `v` that is tangent to the position
 * vector `xin`. The generated `v` satisfies two conditions:
 * 1. It is perpendicular to `xin` (x · v = 0).
 * 2. Its magnitude is determined by physical parameters.
 *
 * @param opt   A structure containing simulation options (e.g., gravitational constant).
 * @param mass  The mass of the particle.
 * @param xin   The position vector.
 * @param v     The output velocity vector (must be pre-allocated).
 * @return void This function populates the `v` array with the result.
 */
void get_tagential_velocity(const struct Options* opt, double mass, const double xin[3], double *v) {
    // --- 1. Initial Calculations ---
    double rad = 0.0;
    int i,j,k;
    for (i = 0; i < 3; i++) rad += xin[i] * xin[i];
    rad = sqrt(rad);
    
    // Normalize xin to get unit vector 'x'
    double x[3];
    for (i = 0; i < 3; i++) x[i] = xin[i] / rad;
    
    // Calculate the magnitude of the velocity
    // Note: We assume opt->grav_unit and opt->vlunittolunit are defined
    double vel = sqrt((opt->grav_unit) / (opt->vlunittolunit) * mass / rad);

    // Determine which component to set to zero ---
    int izero[3] = {0, 0, 0}; // Array to store indices with value 0 in 'x'
    // Count how many components are zero
    int num_zeros = 0;
    for (i = 0; i < 3; i++) {
        if (fabs(x[i]) == 0) {
            izero[num_zeros++] = i;
        }
    }
    
    // Logic to choose which component to set to zero
    // This implements the "permutation" logic from the Fortran code
    if (num_zeros == 0) {
        double y, z;
        k = floor(((double)rand()/((double)RAND_MAX))*3.0);
        i = (k+1)%3;
        j = (i+1)%3;
        v[k] = 0;
        double fac = -x[i]/x[j];
        v[i] = 1.0 / sqrt((-fac*x[k])*(-fac*x[k]) + x[k]*x[k] + (x[i]*fac-x[j])*(x[i]*fac-x[j]));
        v[j] = fac*v[i];
    } 
    else if (num_zeros > 0) {
        // If there are zero components, set the corresponding velocity component to 1.0
        v[0] = 0;
        v[1] = 0;
        v[2] = 0;
        v[izero[0]] = 1.0;
    }
    for (i = 0; i < 3; i++) v[i] = v[i] * vel;
}

/** @brief Wrap positions for period
 * This function wraps the position vector `pos1` according to the periodic boundary conditions defined in `opt`.
 * If the period is greater than zero, it calculates a factor to add to `pos1` to ensure it stays within the defined periodic box.
 * @param opt A pointer to the global options structure containing the period information.
 * @param pos1 The position vector to be wrapped (modified in place).
 */
void period_wrap(double period, double pos1[]) {
    if (period > 0) {
        // Calculate the factor to add
        double fac = -floor(pos1[0] / period) * period;
        
        // Wrap each component
        pos1[0] += fac;
        pos1[1] += fac;
        pos1[2] += fac;
    }
}

/** @brief Wrap positions for period - delta version
 * This function wraps the position vector `pos1` according to the periodic boundary conditions defined in `opt`.
 * If the period is greater than zero, it calculates a factor to add to `pos1` to ensure it stays within the defined periodic box.
 * @param opt A pointer to the global options structure containing the period information.
 * @param pos1 The position vector to be wrapped (modified in place).
 */
void period_wrap_delta(double period, double pos1[]) {
    if (period > 0) {
        // Wrap each component individually
        double half_period = period / 2.0;

        if (pos1[0] > half_period) {
            pos1[0] -= period;
        } else if (pos1[0] < -half_period) {
            pos1[0] += period;
        }

        if (pos1[1] > half_period) {
            pos1[1] -= period;
        } else if (pos1[1] < -half_period) {
            pos1[1] += period;
        }

        if (pos1[2] > half_period) {
            pos1[2] -= period;
        } else if (pos1[2] < -half_period) {
            pos1[2] += period;
        }
    }
}

/**
 * @brief Finds the global minima and maxima of particle positions across all dimensions.
 * @param nparts The number of particles.
 * @param parts Array of Particle data.
 * @return A 1D array where limits[j] is the minimum value for dimension j,
 *         and limits[j+3] is the maximum value for dimension j.
 */
double *get_particle_limits(int nparts, struct Particle *parts) {
    // Allocate memory for the limits (3 dimensions, 2 values each: min and max)
    double *limits = (double *)malloc(6 * sizeof(double));
    if (limits == NULL) {
        fprintf(stderr, "Failed to allocate memory for limits\n");
        return NULL;
    }

    // Initialize limits to zero (or a very negative number for min, and a very positive
    // number for max) to ensure the first particle's values will update them.
    // For this example, we'll use a large negative and positive number.
    const double LARGE_NEGATIVE = -1e30;
    const double LARGE_POSITIVE = 1e30;
    
    for (int j = 0; j < 3; j++) {
        limits[j] = LARGE_NEGATIVE; // Initialize min to a very small number
        limits[j+3] = LARGE_POSITIVE; // Initialize max to a very large number
    }

    // Loop through each particle
    for (int i = 0; i < nparts; i++) {
        // Check each of the three dimensions
        for (int j = 0; j < 3; j++) {
            double particle_position = parts[i].position[j];

            // Update the minimum value for this dimension
            if (particle_position < limits[j]) {
                limits[j] = particle_position;
            }
            if (particle_position > limits[j+3]) {
                limits[j+3] = particle_position;
            }
        }
    }
    return limits;
}

/// \defgroup Visualization
//@{
/**
 * @brief Constructs a mesh representing particle density.
 * @param opt The options structure containing configuration parameters.
 * @param parts Array of Particle data.
 * @param limits A 3x2 array containing the global minima and maxima for each dimension.
 * @return A 2D mesh (allocated as 1D array) representing the mesh density.
 */
double *get_mesh(struct Options *opt, struct Particle *parts, double limits[6]) {
    // Get the dimensions from the input parameters
    int vis_res = opt->vis_res;
    
    // Allocate memory for the mesh
    double *mesh = (double *)malloc(vis_res * vis_res * sizeof(double));
    if (mesh == NULL) {
        fprintf(stderr, "Failed to allocate memory for mesh\n");
        return NULL;
    }

    // Initialize the mesh to zero
    for (int i = 0; i < vis_res * vis_res; i++) {
        mesh[i] = 0.0;
    }

    // Calculate delta for each dimension
    double delta[3] = {0.0, 0.0, 0.0};
    for (int i = 0; i < 3; i++) {
        delta[i] = (limits[i+3] - limits[i]) / (double)opt->vis_res;
    }

    // Iterate over each particle and populate the mesh
    for (int i = 0; i < opt->nparts; i++) {
        
        // Calculate ix and iy using floor() function
        int ix, iy;
        double x_pos = parts[i].position[0];
        double y_pos = parts[i].position[1];
        double mass = parts[i].mass;

        // For C, the Fortran indices start at 1, so we add 1 to convert to 1-based indexing
        ix = (int)floor((x_pos - limits[0]) / delta[0]) + 1;
        iy = (int)floor((y_pos - limits[1]) / delta[1]) + 1;

        // Add the particle's mass to the corresponding mesh cell
        if (ix >= 1 && ix <= vis_res && iy >= 1 && iy <= vis_res) {
            mesh[(ix - 1) * vis_res + (iy - 1)] += mass;
        }
    }
    return mesh;
}

/**
 * @brief Normalizes a mesh by rescaling its values to lie within [0, 1].
 * @param opt A pointer to the Options structure containing configuration parameters.
 * @param mesh A 1D array of doubles representing the mesh data (flattened).
 * @return An array containing the minimum and maximum values of the original mesh before normalization.
 */
double * normalize_mesh(struct Options *opt, double *mesh) {
    // --- Step 1: Find the min and max values ---
    double * mesh_lim = (double *)malloc(2 * sizeof(double));
    if (mesh_lim == NULL) {
        fprintf(stderr, "Failed to allocate memory for mesh limits\n");
        return NULL;
    }
    mesh_lim[0] = mesh[0];
    mesh_lim[1] = mesh[0];

    int total_cells = opt->vis_res * opt->vis_res;
    for (int j = 1; j < total_cells; j++) {
        double value = mesh[j];
        if (value < mesh_lim[0]) {
            mesh_lim[0] = value;
        }
        if (value > mesh_lim[1]) {
            mesh_lim[1] = value;
        }
    }

    // --- Step 2: Normalize the mesh values ---
    double norm = 1.0 / (mesh_lim[1] - mesh_lim[0]);
    for (int j = 0; j < total_cells; j++) {
        mesh[j] = (mesh[j] - mesh_lim[0]) * norm;
    }
    return mesh_lim;
}

/**
 * @brief Produce a ascii visualization of normalised mesh density.
 * @param opt A pointer to the Options structure containing configuration parameters.
 * @param parts Array of Particle data.
 * @param step The current simulation step number.
 */
void visualise_ascii_mesh(struct Options *opt, struct Particle *parts, int step) {
    // We use a 1D array 'mesh_values' to simulate Fortran's 2D array
    double *mesh_values = NULL;
    double *limits = NULL; 

    // --- Step 1: Construct the mesh and its limits ---
    if (opt->period > 0.0) {
        limits = (double *)malloc(6 * sizeof(double)); 
        for (int i = 0; i < 3; i++) { // x, y, z axes
            limits[i] = 0.0;
            limits[i + 3] = opt->period;
        }
    } 
    else {
        limits = get_particle_limits(opt->nparts, parts);
    }
    double *mesh = get_mesh(opt, parts, limits);
    double *mesh_lim = normalize_mesh(opt, mesh);
    // --- Step 5: Output the results ---

    printf("Collisional NBody\n");
    printf("Step %d\n", step);
    
    // Print limits (Fortran uses 1-based indexing, C uses 0)
    printf("Mesh limits:\n");
    for (int i = 0; i < 3; i++) {
        printf("Axis %d: [%f, %f]\n", i+1, limits[i], limits[i+3]);
    }
    printf("Mesh value limits: (%f, %f)\n", mesh_lim[0], mesh_lim[1]);
    printf("Normalised mesh\n");
    printf("-----------------------\n");

    // Print the normalized values
    for (int i = 0; i < opt->vis_res; i++) {
        for (int j = 0; j < opt->vis_res; j++) {
            // Use the values from 'mesh' array (already normalized)
            printf("%5.2f ", mesh[i * opt->vis_res + j]);
        }
        printf("\n");
    }
    printf("-----------------------\n");

    // --- Step 6: Deallocate memory ---
    free(mesh);
    free(mesh_lim);
    free(limits);
}

/**
 * @brief Generates an ASCII visualization of particles at each simulation step.
 * @param opt   Pointer to the Options structure containing configuration.
 * @param parts Array of particle data.
 * @param step  The current simulation step number.
 */
void visualise_ascii(struct Options *opt, struct Particle *parts, int step) {
    // Open or append to the output file
    FILE *file;
    if (step == 0) {
        // First step: open the file for writing
        file = fopen(opt->asciifile, "w");
        if (!file) {
            fprintf(stderr, "Error opening output file %s for writing\n", opt->asciifile);
            return;
        }
    } else {
        // Subsequent steps: append to the file
        file = fopen(opt->asciifile, "a");
        if (!file) {
            fprintf(stderr, "Error opening output file %s for appending\n", opt->asciifile);
            return;
        }
    }

    // Write data to the file
    for (int i = 0; i < opt->nparts; i++) {
        // In Fortran, the array is 1-based, so we start writing from index 1
        if (i == 0) {
            fprintf(file, "%d %f ", step, opt->time);
        } else {
            // For subsequent particles in the same step, just append them
            fprintf(file, "%s", "\n");
            fprintf(file, "%d %f ", step, opt->time);
        }
        
        fprintf(file, 
            "%ld %f %f %f %f %f %f %f %f %e %e %e %ld\n", 
            parts[i].ID,
            parts[i].mass, parts[i].radius,
            parts[i].position[0], parts[i].position[1], parts[i].position[2], 
            parts[i].velocity[0], parts[i].velocity[1], parts[i].velocity[2],
            parts[i].accel[0], parts[i].accel[1], parts[i].accel[2],
            parts[i].PID
        );
    }

    // Close the file
    fclose(file);
}

/**
 * @brief Generates no visualization. 
 * @param step  The current simulation step number.
 */
void visualise_none(int step) {
    printf("Collisional NBody, Step %d\n", step);
}

/**
 * @brief Generates visualization. 
 * @param opt   Pointer to the Options structure containing configuration.
 * @param parts Array of particle data.
 * @param step  The current simulation step number.
 */
void visualise(struct Options *opt, struct Particle *parts, int step) {
    if (opt->ivisualisetype == VisualiseType_VISUAL_ASCII) {
        visualise_ascii(opt, parts, step);
    } else if (opt->ivisualisetype == VisualiseType_VISUAL_ASCII_MESH) {
        visualise_ascii_mesh(opt, parts, step);
    } else {
        visualise_none(step);
    }
}
//@}


/// \defgroup InitialConditions
//@{
/**
 * @brief Generates random initial conditions (IC) for particles.
 * @param opt Pointer to the Options structure containing configuration.
 * @return parts pointer to store the array of particle data.
 */
struct Particle * generate_rand_IC(struct Options *opt) 
{
    // --- 1. Print information about memory usage ---
    printf("Particle class requires %zu bytes\n", sizeof(struct Particle));
    printf("Total number of particles %d\n", opt->nparts);
    printf("Total memory required is %.2f GB\n",
           (double)(opt->nparts * sizeof(struct Particle)) / 1024.0 / 1024.0 / 1024.0);
    printf("Generating random particle positions and velocities\n");

    // --- 2. Allocate memory for particles ---
    struct Particle *parts = (struct Particle *)malloc(opt->nparts * sizeof(struct Particle));
    if (!parts) {
        fprintf(stderr, "Memory allocation failed in generate_rand_IC\n");
        exit(EXIT_FAILURE);
    }

    // --- 4. Generate random positions and velocities ---
    srand(opt->seed);
    double x[3];
    struct timeval time1 = init_time(); 
    opt->massave = 0.0; // Reset the total mass
    for (int i = 0; i < opt->nparts; i++) {
        // Generate random x, y, z
        x[0] = (double)rand() / (double)RAND_MAX;
        x[1] = (double)rand() / (double)RAND_MAX;
        x[2] = (double)rand() / (double)RAND_MAX;

        // Assign to the particle's position
        parts[i].position[0] = x[0] * opt->initial_size;
        parts[i].position[1] = x[1] * opt->initial_size;
        parts[i].position[2] = x[2] * opt->initial_size;
    }
    // --- 7. Set mass, ID, and radius for all particles ---
    for (int i = 0; i < opt->nparts; i++) {
        // Generate random mass and radius
        double mval = (double)rand() / (double)RAND_MAX;
        parts[i].mass = mval * opt->munit;
        parts[i].ID = i + 1; // Particles are 1-based in the code
        parts[i].PID = i + 1;
        // Calculate radius
        parts[i].radius = opt->initial_size * opt->radiusfac * mval;
        // Accumulate total mass
        opt->massave += parts[i].mass;
    }

    opt->massave /= opt->nparts;
    printf("Done generating positions \n");
    get_elapsed_time(time1); 
    return parts;
}

/**
 * @brief Generates orbital initial conditions (IC) for particles based on center of mass.
 * @param opt  Pointer to the Options structure containing configuration.
 * @return parts pointer to store the array of particle data.
 */
struct Particle * generate_orbit_IC(struct Options *opt) 
{
    // generate random positions
    struct Particle *parts = generate_rand_IC(opt);
    printf("Generating orbital velocities based on center of mass\n");
    struct timeval time1 = init_time();
    // Calculate center of mass (cmx) and total mass (mtot) ---
    double mtot = 0.0;
    double cmx[3] = {0.0, 0.0, 0.0}; // Center of mass coordinates
    for (int i = 0; i < opt->nparts; i++) {
        mtot += parts[i].mass;
        cmx[0] += parts[i].position[0] * parts[i].mass;
        cmx[1] += parts[i].position[1] * parts[i].mass;
        cmx[2] += parts[i].position[2] * parts[i].mass;
    }
    if (mtot > 0.0) {
        cmx[0] /= mtot;
        cmx[1] /= mtot;
        cmx[2] /= mtot;
    } else {
        printf("Warning: Total mass is zero; all particles have zero mass.\n");
    }
    // Calculate velocity for each particle ---
    double x[3], v[3];
    for (int i = 0; i < opt->nparts; i++) {
        x[0] = cmx[0] - parts[i].position[0];
        x[1] = cmx[1] - parts[i].position[1];
        x[2] = cmx[2] - parts[i].position[2];
        get_tagential_velocity(opt, mtot - parts[i].mass, x, v);
        parts[i].velocity[0] = v[0];
        parts[i].velocity[1] = v[1];
        parts[i].velocity[2] = v[2];
    }
    get_elapsed_time(time1); // Assume this function is defined elsewhere
    return parts;
}

/**
 * @brief Generates initial conditions (IC) for particles based on options.
 * @param opt Pointer to the Options structure containing configuration.
 * @return parts pointer to store the array of particle data.
 */
struct Particle * generate_IC(struct Options *opt) 
{
    if (opt->iictype == ICType_IC_RAND) {
        return generate_rand_IC(opt);
    } else if (opt->iictype == ICType_IC_ORBIT) {
        return generate_orbit_IC(opt);
    } else {
        fprintf(stderr, "Invalid IC type in generate_IC\n");
        exit(EXIT_FAILURE);
    }
}

//@}

/// \defgroup Usage
//@{
/**
 * @brief Displays the program usage and help information.
 * This function should typically be called when the command line arguments
 * are not provided correctly.
 */
void usage() {
    printf("Usage: \n");
    printf(" -n <number of particles> -t <nsteps> \n");
    printf("-B [<Boundary type>\n");
    printf("-I <IC type> \n");
    printf("-T <Time Step Criterion> \n");
    printf("-V <Visualisation type> -r <Vis res>\n");
    printf("---------------------------------------------------\n");
    printf("Example: \n");
    printf("./nbody -n 1000 -t 1000 -B 1 -I 1 -T 1 -V 1 -r 50\n");
    printf("Note that the above example will generate 1000 particles, run for 1000 steps, with periodic boundaries, orbiting initial conditions, adaptive time-stepping, and a visualisation of the projected density mesh as ascii with a resolution of 50x50.\n");
    printf("---------------------------------------------------\n");
    printf("Boundary type: 0 non-periodic (default), 1 periodic\n");
    printf("IC type: 0 random (default), 1 orbiting (useful for few-body systems)\n");
    printf("Time Step Criterion: 0 static, 1 adaptive (default)\n");
    printf("Visualisation type: 0 none (default), 1 projected density mesh as ascii\n");
    printf("Vis res: valid for vis type 1, mesh resolution\n");
    printf("---------------------------------------------------\n");
}

/**
 * @brief The main run_state function that sets up and runs the simulation.
 */
void run_state(struct Options *opt) {

    struct Particle p; 
    uint64_t nbytes = sizeof(p) * opt->nparts;
    double memfootprint_gb = (double)nbytes / (1024.0 * 1024.0 * 1024.0);

    printf("====================================================\n");
    printf(" NBody Running with following parameters\n");
    printf("====================================================\n");
    printf("Requesting number of particles %lu\n", (unsigned long)opt->nparts);
    printf("which requires %.2f GB\n", memfootprint_gb);
    printf("Number of steps %lu\n", (unsigned long)opt->nsteps);
    printf("Physical parameters:\n");
    printf("munit in solar masses: %.10e\n", opt->munit);
    printf("lunit in parsecs: %.10e\n", opt->lunit);
    printf("vunit in km/s: %.10e\n", opt->vunit);
    printf("tunit in seconds: %.10e\n", opt->tunit);
    printf("time step: %.10e\n", opt->time_step);
    printf("grav in solar masses pc km^2/s^2: %.10e\n", opt->grav_unit);
    printf("collisional force unit in grav units: %.10e\n", opt->collision_unit);
    printf("period in parsecs: %.10e\n", opt->period);
    printf("Collisoinal radius factor: %.10e\n", opt->radiusfac);
    printf("Boundary rule type %d\n", opt->iboundarytype);
    printf("IC type %d\n", opt->iictype);
    printf("Time-step criterion %d\n", opt->itimestepcrit);
    printf("Visualization type %d\n", opt->ivisualisetype);

#ifdef USEPAIRS
    printf("Running using pre-determined pair set \n");
#else 
    printf("Running double for loop for pairs \n");
#endif
    printf("====================================================\n");

    // --- 4. MPI Section ---
#ifdef _MPI
    int mpi_size;
    int ierror;

    // Check if MPI is initialized
    if (MPI_Init(NULL) != MPI_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize MPI.\n");
        exit(EXIT_FAILURE);
    }

    // Get the number of processes
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &mpi_size));
    printf("====================================================\n");
    printf("Running with MPI\n");
    printf("Comm World contains %d processes \n", mpi_size);
    printf("====================================================\n");
#endif

    // --- 5. OpenMP Section ---
#ifdef _OPENMP
    int omp_size;
    omp_size = omp_get_max_threads();
    printf("====================================================\n");
    printf("Running with OpenMP\n");
    printf("Number of threads: %d \n", omp_size);
    printf("====================================================\n");

#endif
    report_core_binding();
}

/**
 * @brief Get user input to configure simulation 
 */
void getinput(int argc, char *argv[], struct Options *opt) {
    opt->nparts = 0;
    opt->nsteps = 0;
    opt->iboundarytype = BoundaryType_BOUNDARY_NONE;
    opt->iictype = ICType_IC_RAND;
    opt->itimestepcrit = TimeStepCrit_Adaptive;
    opt->ivisualisetype = VisualiseType_VISUAL_ASCII;
    opt->initial_size = 1.0;
    opt->period = 0.0;
    char c;
    while ((c = getopt (argc, argv, "n:t:B:I:V:r:")) != -1) {
        switch (c) {
            case 'n':
                opt->nparts = atoi(optarg);
                break;
            case 't':
                opt->nsteps = atoi(optarg);
                break;
            case 'B':
                opt->iboundarytype = atoi(optarg);
                break;
            case 'I':
                opt->iictype = atoi(optarg);
                break;
            case 'V':
                opt->ivisualisetype = atoi(optarg);
                break;
            case '?':
                printf("Unknown option\n");
                usage();
            }
    }
    char outfilename[2000] = "nbody-data.txt";
    char asciifilename[2000] = "nbody-asciivis.txt";
    strcpy(opt->outfile, outfilename);
    strcpy(opt->asciifile, asciifilename); 
    if (opt->nparts <= 0) {
        printf("Invalid number of particles. Must be > 0\n");
        usage();
        exit(EXIT_FAILURE);
    }
    if (opt->nsteps <= 0) {
        printf("Invalid number of steps. Must be > 0\n");
        usage();
        exit(EXIT_FAILURE);
    }
    // and radius is 1/10th of the initial size of the box 
    opt->radiusfac = 0.1/pow((double)opt->nparts, 1.0/3.0);
    opt->munit = 1.0;
    opt->vunit = 1.0; 
    opt->lunit = 1.0; 
    // conversion from km to pc 
    opt->vlunittolunit = 3.24078e-14;
    // grav in km/s^2 pc^2 / solar masses
    opt->grav_unit = 4.30241002e-3 * opt->vlunittolunit;
    // make time unit related to graviational unit in seconds
    opt->tunit = 1.0/sqrt((opt->grav_unit *opt->vlunittolunit * opt->munit/pow(opt->initial_size, 3.0))) ;
    // time step should be some fraction of the dynamical time of a close encounter the system which is related 
    // to tunit by radiusfac **1.5
    opt->time = 0;
    opt->time_step_fac = 0.1;
    // opt->time_step = opt->tunit * pow(opt->radiusfac, 1.5) * opt->time_step_fac;
    opt->time_step = opt->tunit * opt->time_step_fac;
    // make collisional (repulsive) force 10 times stronger than gravity 
    // and is in units of gravitational forces 
    opt->collision_unit = 4.0;
    opt->seed = 4224;
    srand(opt->seed); // Sets the seed for random number generation
    run_state(opt);
}

//@}
