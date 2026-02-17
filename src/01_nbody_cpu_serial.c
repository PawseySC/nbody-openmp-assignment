#include <stdio.h>
#include <math.h> // For sqrt(), pow(), etc.
#include <stdio.h>
#include "common.h" 


/** Calculate the acceleration for each particle
 * @param opt A pointer to the global options structure.
 * @param parts A pointer to an array of particle structures (will be modified).
 */
void accel_update(struct Options *opt, struct Particle *parts) {
    double rad, rad2, invrad, accel, maxaccel;
    double minrad = DBL_MAX; 
    double massave, newstep;
    double delta[3];
    int i, j, k;
    struct timeval time1;

    // Get start time
    time1 = init_time();

    // Initialize particle accelerations to zero
    for (i = 0; i < opt->nparts; i++) {
        for (k = 0; k < 3; k++) {
            parts[i].accel[k] = 0.0;
        }
    }

    // Calculate gravitational forces
    for (i = 0; i < opt->nparts; i++) {
        for (j = 0; j < opt->nparts; j++) {
            if (i != j) {
                // Get the position difference
                for (k = 0; k < 3; k++) {
                    delta[k] = parts[j].position[k] - parts[i].position[k];
                }
                period_wrap_delta(opt->period, delta);

                // Calculate squared distance and radius
                rad2 = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
                rad = sqrt(rad2);

                // Update minrad for smoothing
                if (rad < minrad) {
                    minrad = rad;
                }

                // Apply force smoothing inside radius
                invrad = 1.0 / (rad2 + 0.05 * parts[i].radius * parts[i].radius);
                // Add acceleration from particle j to particle i
                for (k = 0; k < 3; k++) {
                    parts[i].accel[k] += invrad * delta[k] / rad * parts[j].mass * opt->grav_unit;
                }
            }
        }
    }

    // Calculate collisional forces
    for (i = 0; i < opt->nparts; i++) {
        for (j = 0; j < opt->nparts; j++) {
            if (i != j) {
                // Get the position difference
                for (k = 0; k < 3; k++) {
                    delta[k] = parts[i].position[k] - parts[j].position[k];
                }
                period_wrap_delta(opt->period, delta); // Assuming this function is in nbody_common

                rad2 = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
                rad = sqrt(rad2);

                // Check if particles are within collision radius
                if (rad < parts[i].radius) {
                    invrad = 1.0 / (parts[i].radius * parts[i].radius);
                    // Add collisional acceleration from particle j to particle i
                    for (k = 0; k < 3; k++) {
                        parts[i].accel[k] += invrad * delta[k] / rad * parts[j].mass * opt->grav_unit * opt->collision_unit;
                    }
                }
            }
        }
    }

    // Adaptive time step calculation based on closest particle pair
    if (opt->itimestepcrit == TimeStepCrit_Adaptive) {
        // get average mass between particle pairs with min distance
        // massave = 0.5*(parts(iminrad)%mass + parts(jminrad)%mass)        
        newstep = opt->time_step_fac * sqrt(2.0 * minrad * minrad * minrad / (opt->grav_unit * opt->vlunittolunit * opt->massave));
        if (newstep < opt->tunit) {
            opt->time_step = newstep;
        } else {
            opt->time_step = opt->tunit;
        }
    }

    printf("Finished calculating acceleration\n");
    get_elapsed_time(time1);
}


/**
 * Updates the velocities of all particles based on their accelerations.
 * @param opt A pointer to the global options structure.
 * @param parts A pointer to an array of particle structures (will be modified).
 */
void velocity_update(const struct Options *opt, struct Particle *parts) {
    // --- Initialize variables ---
    struct timeval time1 = init_time(); // Assume this function is declared in nbody_common.h
    
    double dkin_min = HUGE_VAL; // C's macro for "huge" value
    double dkin_max = 0.0;
    double dkin_ave = 0.0;

    // --- Main loop over all particles ---
    for (int i = 0; i < opt->nparts; i++) 
    { 
        // Update velocities by determining velocity change 
        double delta[3];
        for (int k = 0; k < 3; k++) delta[k] = parts[i].accel[k] * opt->time_step;

        // Update the particle's velocity 
        // and get the fractional change in kinetic energy 
        double dkin_num = 0, vel_sq = 0;
        for (int k = 0; k < 3; k++) 
        {
            dkin_num += delta[k] * delta[k];
            vel_sq += parts[i].velocity[k] * parts[i].velocity[k];
            parts[i].velocity[k] += delta[k];
        }
        double current_dkin = (vel_sq > 1e-12) ? dkin_num / vel_sq : 0.0;

        // 5. Update min, max, and average dkin
        dkin_min = (current_dkin < dkin_min) ? current_dkin : dkin_min;
        dkin_max = (current_dkin > dkin_max) ? current_dkin : dkin_max;
        dkin_ave += current_dkin;
    }

    // 6. Calculate the average dkin
    dkin_ave /= opt->nparts;

    // --- Output (replace with your logging mechanism if needed) ---
    printf("Finished calculating velocity\n");
    printf("Particles specific kinetic energy changed by [min, ave, max]: %e %e %e\n", 
           dkin_min, dkin_ave, dkin_max);

    get_elapsed_time(time1);
}

/**
 * Updates the positions of all particles based on their velocities.
 * @param opt A pointer to the global options structure.
 * @param parts A pointer to an array of particle structures (will be modified).
 */
void position_update(const struct Options *opt, struct Particle *parts) {
    // --- Initialize variables ---
    struct timeval time1 = init_time(); // Assume this function is declared in nbody_common.h
    
    double *rads = malloc(opt->nparts * sizeof(double)); 
    double rad_average = 0.0;
    double rmin = HUGE_VAL; 
    double rmax = 0.0;

    // --- Main loop over all particles ---
    for (int i = 0; i < opt->nparts; i++) { 
        
        // Calculate delta position (parts(i)%velocity * opt%time_step * opt%vlunittolunit)
        double delta[3];
        for (int k = 0; k < 3; k++) {
            delta[k] = parts[i].velocity[k] * opt->time_step * opt->vlunittolunit;
            parts[i].position[k] += delta[k];
        }
        period_wrap(opt->period, parts[i].position); 
        // get fractional change in position normalized by approximate average density 
        double norm_delta_squared = 
            (delta[0]*delta[0]) + 
            (delta[1]*delta[1]) +
            (delta[2]*delta[2]);
        double n_pow_inv_third = 1.0 / pow((double)opt->nparts, 1.0 / 3.0);
        double denominator = opt->initial_size / n_pow_inv_third;
        rads[i] = sqrt(norm_delta_squared) / denominator;
        rad_average += rads[i];
    }
    rad_average /= opt->nparts;

    // 7. Find min and max of rads
    for (int i = 0; i < opt->nparts; i++) {
        rmin = (rads[i] < rmin) ? rads[i] : rmin;
        rmax = (rads[i] > rmax) ? rads[i] : rmax;
    }

    // --- Output (replace with your logging mechanism if needed) ---
    printf("Finished calculating position\n");
    printf("Particles moved by [min, ave, max]: %e %e %e\n", 
           rmin, rad_average, rmax);

    // --- Elapsed time ---
    get_elapsed_time(time1);
    free(rads); 
}

/**
 * @brief Writes output data for the current simulation step to a file.
 * @param opt Pointer to the Options structure containing configuration.
 * @param parts Pointer to the array of particle data.
 * @param step The current simulation step number.
 */
void nbody_output(const struct Options *opt, const struct Particle *parts, int step) {
    FILE *fp;
    if (step == 0) {
            fp = fopen(opt->outfile, "w");
    } else {
            fp = fopen(opt->outfile, "a");
    }
    if (!fp) {
            printf("Error opening output file %s\n", opt->outfile);
            return;
    }
    for (int i = 0; i < opt->nparts; i++) {
        fprintf(fp, 
            "%d %f %ld %f %f %f %f %f %f %f %f %e %e %e %ld\n", 
            step, opt->time,
            parts[i].ID,
            parts[i].mass, parts[i].radius,
            parts[i].position[0], parts[i].position[1], parts[i].position[2], 
            parts[i].velocity[0], parts[i].velocity[1], parts[i].velocity[2],
            parts[i].accel[0], parts[i].accel[1], parts[i].accel[2],
            parts[i].PID
        );
    }
    fclose(fp);
}


/**
 * Main function: The entry point of the NBody simulation program.
 */
int main(int argc, char *argv[]) {

    // Create an instance of Options
    struct Options opt;

    // Call the function to get input (similar to Fortran's getinput)
    getinput(argc, argv, &opt);
    // generate initial conditions for the particles
    struct Particle *parts = generate_IC(&opt);

    // Variables for timing
    struct timeval time1, time2;

    // --- Step 3: The Main Simulation Loop ---

    printf("Running simulation for %d steps...\n", opt.nsteps);
    time1 = init_time();
    
    // Loop over each step in the simulation
    for (int step = 0; step < opt.nsteps; step++) 
    {
        time2 = init_time();
        visualise(&opt, parts, step);
        nbody_output(&opt, parts, step);
        accel_update(&opt, parts);
        velocity_update(&opt, parts);
        position_update(&opt, parts);
        get_elapsed_time(time2);
        opt.time += opt.time_step;
        printf("Finished step %d moving %e in time to %e\n", step, opt.time_step, opt.time);
    }
    get_elapsed_time(time1);

    free(parts);

    return 0;
}

