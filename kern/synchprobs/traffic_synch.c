#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */

#define MAX_THREADS 10  // is it right ?


//static struct semaphore *intersectionSem;
 static struct cv *intersectionCV;
 static Vehicle * volatile intersectionVehicles[MAX_THREADS];


/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

/*
  intersectionSem = sem_create("intersectionSem",1);
  if (intersectionSem == NULL) {
    panic("could not create intersection semaphore");
  }
  return;
*/

  intersectionCV = cv_create("intersectionBusy");
  if (intersectionCV == NULL) {
    panic("Couldn't create intersectionCV")
  }
  for (i=0; i<MAX_THREADS; i++) {
    intersectionVehicles[i] = (Vehicle * volatile) NULL;
  }
  return;

}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
/*
  KASSERT(intersectionSem != NULL);
  sem_destroy(intersectionSem);
*/
  KASSERT(intersectionCV != NULL);
  cv_destory(intersectionCV);
  for (i=0; i<MAX_THREADS; i++) {
    intersectionVehicles[i] = NULL;
  }
  return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  // (void)origin;  /* avoid compiler complaint about unused parameter */
  // (void)destination;  // avoid compiler complaint about unused parameter 
  // KASSERT(intersectionSem != NULL);
  // P(intersectionSem);


// LJC: do we know the thread_num of the calling thread? test this !! 
// if yes, then we save trouble by not using nested for loop
  Vehicle v;
  v.origin = origin;
  v.destination = destination;
  // directly use thread_num from calling thread
  intersectionVehicles[thread_num] = &v;
  for(i=0;i<NumThreads;i++) {


  }
}


bool can_enter_intersection(int thread_num) {
/*
	LJC: this check is crucial, especially check vehicles[i]==NULL. 
	Because when some thread run the check_constraint function, 
	other threads within vehicles may not even be initialized !!
    */
    for (int i=0; i<MAX_THREADS; i++) {
        if ((i==thread_num) || (intersectionVehicles[i] == NULL)) continue;
    /* no conflict if both vehicles have the same origin */
        if (intersectionVehicles[i]->origin == intersectionVehicles[thread_num]->origin) continue;
    /* no conflict if vehicles go in opposite directions */
        if ((intersectionVehicles[i]->origin == intersectionVehicles[thread_num]->destination) &&
        (intersectionVehicles[i]->destination == intersectionVehicles[thread_num]->origin)) continue;
    /* no conflict if one makes a right turn and 
       the other has a different destination */
        if ((right_turn(intersectionVehicles[i]) || right_turn(intersectionVehicles[thread_num])) &&
  (intersectionVehicles[thread_num]->destination != intersectionVehicles[i]->destination)) continue;

        return false;
    }
    return true;
}

/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  KASSERT(intersectionSem != NULL);
  V(intersectionSem);
}

