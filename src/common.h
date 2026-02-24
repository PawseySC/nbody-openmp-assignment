
/** @file common.h
 *  @brief declares global variables and prototypes.
 */

#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <float.h>
#include <math.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#ifndef __APPLE__
#include <sys/sysinfo.h>
#endif
#include <unistd.h>
#ifdef USEOPENMP
#include <omp.h>
#endif

/**
 * @brief Enumeration for visualization types.
 */
typedef enum {
    VisualiseType_VISUAL_NONE = 0,
    VisualiseType_VISUAL_ASCII,
    VisualiseType_VISUAL_ASCII_MESH
} VisualiseType;


/**
 * @brief Enumeration for initial condition (IC) types.
 */
typedef enum {
    ICType_IC_RAND = 0,
    ICType_IC_ORBIT,
    ICType_IC_FILE
} ICType;


/**
 * @brief Enumeration for boundary condition types.
 */
typedef enum {
    BoundaryType_BOUNDARY_NONE = 0,
    BoundaryType_BOUNDARY_PERIODIC
} BoundaryType;


/**
 * @brief Enumeration for time-step criteria.
 */
typedef enum {
    TimeStepCrit_Static = 0,
    TimeStepCrit_Adaptive
} TimeStepCrit;


/**
 * @brief Represents the options and configuration settings for an N-body simulation.
 */
struct Options {
    /**
     * @name Simulation Parameters
     * @{
     */
    int nparts;                ///< Number of particles in the simulation.
    int nsteps;                ///< Total number of steps in the simulation.

    ICType iictype;               /**< Type of initial condition input (e.g., file, random). */
    VisualiseType ivisualisetype;        /**< Type of visualization to be used. */
    BoundaryType iboundarytype;         /**< Type of boundary conditions (e.g., periodic). */
    TimeStepCrit itimestepcrit;         /**< Criterion for adjusting the time step. */

    double initial_size;      /**< Initial size scale of the simulation box. */
    double period;            /**< Gravitational period for time-step adjustment. */
    double radiusfac;         /**< Factor to determine particle radii from mass. */
    
    double time;              /**< Current simulation time. */
    double time_step;         /**< Current time step size. */
    double time_step_fac;     /**< Factor to adjust the time step. */

    double munit;             /**< Mass unit for the simulation (in solar masses). */
    double massave;           /**< Average particle mass (for radius calculation). */
    double vunit;             /**< Velocity unit for the simulation (in km/s). */
    double tunit;             /**< Time unit for the simulation. */
    double lunit;             /**< Length unit for the simulation. */
    /**
     * @brief VLUNITTOLUNIT is vunit / lunit, representing velocity over length.
     *        This is useful for checking units consistency in calculations.
     */
    double vlunittolunit;    

    double grav_unit;         /**< Gravitational constant (with correct units). */
    double collision_unit;    /**< Unit related to collision detection. */

    /**
     * @name Visualization Settings
     * @{
     */
    int vis_res;              /**< Resolution for visualization. */
    /**
     * @brief Filenames for output.
     *        These are character strings that hold the names
     *        of files where simulation data will be written.
     *        In C, we use standard C strings (null-terminated).
     */
    char outfile[2001];       /**< Filename for main output. */
    char asciifile[2001];     /**< Filename for ASCII dump output. */
    /**
     * @}
     */
    int seed;                 /**< Random seed for reproducibility. */
    
};

/**
 * @brief Represents a single particle in the N-body simulation.
 */
struct Particle {
    /**
     * @name Identification and Properties
     * @{
     */
    long long ID;             /**< A unique 64-bit identifier for the particle. */
    double mass;              /**< The mass of the particle. */
    double radius;            /**< The radius of the particle (for collision detection). */
    /**
     * @name Position, Velocity, and Acceleration
     * @{
     */
    double position[3];       /**< The 3D position of the particle. */
    double velocity[3];       /**< The 3D velocity of the particle. */
    double accel[3];          /**< The 3D acceleration of the particle (updated at each step). */
    /**
     * @}
     */
    
    long long PID;            /**< A parallel ID, likely for threading/pool management. */
};


/// \defgroup Profiling
//@{
/// get some basic timing info
struct timeval init_time();
/// get the elapsed time relative to start, return current wall time
struct timeval get_elapsed_time(struct timeval start);
/// get memory footprint
void report_mem_usage(char where[1000]);


#ifdef __APPLE__
typedef struct cpu_set {
  unsigned int count;
} my_cpu_set_t;
int sched_getaffinity(pid_t pid, size_t cpu_size, my_cpu_set_t *cpu_set);
int mysysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp, size_t newlen);
#define SYSCTL_CORE_COUNT   "machdep.cpu.core_count"
#endif 

/// get core binding
void report_core_binding();
//@}


/** @brief Wrap positions for period
 * @param period A pointer to the global options structure containing the period information.
 * @param pos1 The position vector to be wrapped (modified in place).
 */
void period_wrap(double period, double pos1[]);
/** @brief Wrap positions for period - delta version
 * @param opt A pointer to the global options structure containing the period information.
 * @param pos1 The position vector to be wrapped (modified in place).
 */
void period_wrap_delta(double period, double pos1[]);

/**
 * @brief Calculate accelerations of all particles based on their positions and masses.
 * @param opt A pointer to the global options structure.
 * @param parts A pointer to an array of particle structures (will be modified).
 */
void accel_update(struct Options *opt, struct Particle *parts);
/**
 * @brief Updates the positions of all particles based on their velocities.
 * @param opt A pointer to the global options structure.
 * @param parts A pointer to an array of particle structures (will be modified).
 */
void position_update(const struct Options *opt, struct Particle *parts);
/**
 * @brief Updates the velocities of all particles based on their accelerations.
 * @param opt A pointer to the global options structure.
 * @param parts A pointer to an array of particle structures (will be modified).
 */
void velocity_update(const struct Options *opt, struct Particle *parts);

/// \defgroup Visualisation
//@{
/**
 * @brief Finds the global minima and maxima of particle positions across all dimensions.
 * @param parts An array of particles.
 * @param nparts The number of particles.
 * @return A 1D array where limits[j] is the minimum value for dimension j,
 *         and limits[j+3] is the maximum value for dimension j.
 */
double *get_particle_limits(int nparts, struct Particle *parts);

/**
 * Constructs a 2D mesh representing particle density.
 * @param opt The options structure containing configuration parameters.
 * @param parts An array of particles.
 * @param limits A 3x2 array containing the global minima and maxima for each dimension.
 * @return A 2D mesh (allocated as 1D array) representing the mesh density.
 */
double *get_mesh(struct Options *opt, struct Particle *parts, double limits[6]);

/**
 * @brief Produce a ascii visualization of normalised mesh density.
 * @param opt A pointer to the Options structure containing configuration parameters.
 * @param step The current simulation step number.
 * @param parts Array of Particle data.
 */
void visualise_ascii_mesh(struct Options *opt, struct Particle *parts, int step);

/**
 * @brief Generates an ASCII visualization of particles at each simulation step.
 * @param opt   Pointer to the Options structure containing configuration.
 * @param parts Array of particle data.
 * @param step  The current simulation step number.
 */
void visualise_ascii(struct Options *opt, struct Particle *parts, int step);
/**
 * @brief Generates no visualization. 
 * @param step  The current simulation step number.
 */
void visualise_none(int step);
/**
 * @brief Generates visualization. 
 * @param opt   Pointer to the Options structure containing configuration.
 * @param parts Array of particle data.
 * @param step  The current simulation step number.
 */
void visualise(struct Options *opt, struct Particle *parts, int step);
//@}


/// \defgroup InitialConditions
//@{
/**
 * @brief Generates random initial conditions (IC) for particles.
 * @param opt Pointer to the Options structure containing configuration.
 * @return parts pointer to store the array of particle data.
 */
struct Particle * generate_rand_IC(struct Options *opt);

/**
 * @brief Generates orbital initial conditions (IC) for particles based on center of mass.
 * @param opt  Pointer to the Options structure containing configuration.
 * @return parts pointer to store the array of particle data.
 */
struct Particle * generate_orbit_IC(struct Options *opt);

/**
 * @brief Generates initial conditions (IC) for particles based on options.
 * @param opt Pointer to the Options structure containing configuration.
 * @return parts pointer to store the array of particle data.
 */
struct Particle * generate_IC(struct Options *opt);
//@}

/// \defgroup Usage
//@{
/**
 * @brief Displays the program usage and help information.
 * This function should typically be called when the command line arguments
 * are not provided correctly.
 */
void usage() ;
/**
 * @brief The main run_state function that sets up and runs the simulation.
 * @param opt  Pointer to the Options structure containing configuration.
 */
void run_state(struct Options *opt) ;
/**
 * @brief Get user input to configure simulation 
 * @param argc number of arguments
 * @param argv array of argument strings
 * @param opt  Pointer to the Options structure containing configuration.
 */
void getinput(int argc, char *argv[], struct Options *opt);
//@}

#endif