program NBody
!---------------------------------------------------------------------
!
!  This program runs a Collisional N-body simulation
!
!  Uses:  
!
!---------------------------------------------------------------------
        use nbody_common 
        implicit none
        interface 
        ! prototypes
        subroutine accel_update(opt, parts)
                use nbody_common 
                implicit none
                type(Options), intent(inout) :: opt
                type(Particle), dimension(:), intent(inout) :: parts
        end subroutine
        subroutine velocity_update(opt, parts)
                use nbody_common 
                implicit none
                type(Options), intent(in) :: opt
                type(Particle), dimension(:), intent(inout) :: parts
        end subroutine
        subroutine position_update(opt, parts)
                use nbody_common 
                implicit none
                type(Options), intent(in) :: opt
                type(Particle), dimension(:), intent(inout) :: parts
        end subroutine
        ! Nbody output protoype
        subroutine nbody_output(opt, step, parts)
                use nbody_common 
                implicit none 
                type(Options), intent(in) :: opt
                integer, intent(in) :: step
                type(Particle), dimension(:), intent(in) :: parts
        end subroutine 
        end interface 
        type(Options) :: opt
        integer :: current_step
        type(Particle), dimension(:), target, allocatable :: parts
        real*8 :: time1, time2

        call getinput(opt)
        call generate_IC(opt, parts)
        time1 = init_time()
        current_step = 0
        print *, "Running simulation for ", opt%nsteps
        do while (current_step .ne. opt%nsteps)
                time2 = init_time()
                call visualise(opt, current_step, parts)
                call nbody_output(opt, current_step, parts)
                call accel_update(opt, parts)
                call velocity_update(opt, parts)
                call position_update(opt, parts)
                opt%time = opt%time + opt%time_step
                current_step = current_step + 1
                print *, "Finished step ", current_step, " moving ", opt%time_step, " in time to ", opt%time   
                call get_elapsed_time(time2)
                time2 = init_time()
        end do 
        write(*,*) "Finished NBody simulation"
        call get_elapsed_time(time1);
        deallocate(parts)
end program

! calculate the accelerations on particles 
subroutine accel_update(opt, parts)
        use nbody_common 
        implicit none
        type(Options), intent(inout) :: opt
        type(Particle), dimension(:), intent(inout) :: parts
        real(8) :: rad, rad2, invrad, accel, maxaccel, minrad, massave, newstep
        real(8), dimension(3) :: delta
        integer :: i, j, k, imaxaccel, iminrad, jminrad
        real(8) :: time1

        time1 = init_time()

        do i = 1, opt%nparts
                do k = 1,3
                        parts(i)%accel(k) = 0
                end do
        end do
        ! calculate gravitational force
        minrad = HUGE(minrad) 
        do i = 1, opt%nparts
                do j = 1, opt%nparts
                        if (j .ne. i) then 
                                delta = parts(j)%position - parts(i)%position
                                call period_wrap_delta(opt, delta)
                                rad2 = delta(1)**2.0 + delta(2)**2.0 + delta(3)**2.0
                                rad = sqrt(rad2)
                                minrad = min(minrad,rad)
                                ! there is some force smoothing inside radius
                                invrad = 1.0/(rad2 + 0.05*parts(i)%radius**2.0)
                                parts(i)%accel = parts(i)%accel + invrad * delta / rad *  parts(j)%mass * opt%grav_unit
                        end if 
                end do 
        end do 
        ! calculate collisional force, other particles must be in the window
        do i = 1, opt%nparts
                do j = 1, opt%nparts
                        if (j .ne. i) then 
                                delta = parts(i)%position - parts(j)%position
                                call period_wrap_delta(opt, delta)
                                rad2 = delta(1)**2.0 + delta(2)**2.0 + delta(3)**2.0
                                rad = sqrt(rad2)
                                if (rad .lt. parts(i)%radius) then 
                                        invrad = 1.0/(parts(i)%radius)**2.0
                                        parts(i)%accel = parts(i)%accel + invrad * delta &
                                        / rad * parts(j)%mass * opt%grav_unit * opt%collision_unit
                                end if 
                        end if 
                end do 
        end do 
        ! if updating step base it on the acceleration between the closest pair of particles 
        if (opt%itimestepcrit .eq. TimeStepCrit_Adaptive) then
                ! get average mass between particle pairs with min distance
                ! massave = 0.5*(parts(iminrad)%mass + parts(jminrad)%mass)
                newstep = opt%time_step_fac*sqrt(2.0*minrad**3.0/(opt%grav_unit * opt%vlunittolunit * opt%massave))
                if (newstep .lt. opt%tunit) then 
                        opt%time_step = newstep
                else 
                        opt%time_step = opt%tunit
                end if 
        end if
        print *, "Finished calculating acceleration"
        call get_elapsed_time(time1) 
end subroutine

! update velocities 
subroutine velocity_update(opt, parts)
        use nbody_common 
        implicit none
        type(Options), intent(in) :: opt
        type(Particle), dimension(:), intent(inout) :: parts
        real(8), dimension(3) :: delta 
        real(8) :: dkin, dkin_min, dkin_max, dkin_ave
        integer :: i
        real(8) :: time1
        time1 = init_time()
        dkin_max = 0
        dkin_min = HUGE(dkin_min)
        dkin_ave = 0
        do i = 1, opt%nparts
                delta = parts(i)%accel * opt%time_step
                dkin = (delta(1)**2.0+delta(2)**2.0+delta(3)**2.0)
                parts(i)%velocity = parts(i)%velocity + delta 
                dkin = dkin/(parts(i)%velocity(1)**2.0+parts(i)%velocity(2)**2.0+parts(i)%velocity(3)**2.0)
                dkin_min = min(dkin_min,dkin)
                dkin_max = max(dkin_max,dkin)
                dkin_ave = dkin_ave + dkin
        end do
        dkin_ave = dkin_ave / opt%nparts 
        print *, "Finished calculating velocity"
        print *, "Particles specic kinetic energy changed by [min,ave,max]", dkin_min, dkin_ave, dkin_max
        call get_elapsed_time(time1) 
end subroutine
! update positions 
subroutine position_update(opt, parts)
        use nbody_common 
        implicit none
        type(Options), intent(in) :: opt
        type(Particle), dimension(:), intent(inout) :: parts
        real(8), dimension(3) :: delta 
        real(8), dimension(opt%nparts) :: rads
        real(8) :: rad_average, rmin, rmax
        integer :: i
        real(8) :: time1 
        time1 = init_time()
        rad_average = 0
        !print *, "Looking at position update"
        do i = 1, opt%nparts
                delta = parts(i)%velocity * opt%time_step * opt%vlunittolunit
                rads(i) = sqrt(delta(1)**2.0+delta(2)**2.0+delta(3)**2.0)/(opt%initial_size/opt%nparts**(1.0/3.0))
                rad_average = rad_average + rads(i)
                parts(i)%position = parts(i)%position + parts(i)%velocity * opt%time_step * opt%vlunittolunit
                call period_wrap(opt, parts(i)%position)
        end do
        rad_average = rad_average/opt%nparts
        rmin = minval(rads)
        rmax = maxval(rads)
        print *, "Finished calculating position"
        print *, "Particles moved by [min,ave,max]", rmin, rad_average, rmax
        call get_elapsed_time(time1) 
end subroutine

! write out the nbody information 
subroutine nbody_output(opt, step, parts)
        use nbody_common 
        implicit none 
        type(Options), intent(in) :: opt
        integer, intent(in) :: step
        type(Particle), dimension(:), intent(in) :: parts
        integer :: i

        if (step .eq. 0) then 
                open(10, file=opt%outfile, access="sequential")
        else 
                open(10, file=opt%outfile, access="append")
        end if
        do i = 1, opt%nparts
                write(10,*) step, opt%time, parts(i)
        end do 
        close(10)
end subroutine 
