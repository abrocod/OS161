#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <thread.h> // added
#include <current.h> // added

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

typedef struct Vehicles
{
  Direction origin;
  Direction destination;
} Vehicle;

//static struct semaphore *intersectionSem;
 static struct cv* intersectionCV;
 static Vehicle* volatile intersectionVehicles[MAX_THREADS];
 static struct lock* volatile locks[MAX_THREADS];
 static volatile int vehicleCount;

/* functions that defined and used internally */
static bool right_turn(Vehicle *v);
static void can_enter_intersection(int vehicleCount);

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
  vehicleCount = 0;
  //locks = kmalloc(sizeof(struct lock *)* MAX_THREADS);

  if (intersectionCV == NULL) {
    panic("Couldn't create intersectionCV");
  }
  for (int i=0; i<MAX_THREADS; i++) {
    intersectionVehicles[i] = (Vehicle * volatile) NULL;
    locks[i] = lock_create("");
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

  for (int i=0; i<MAX_THREADS; i++) {
    intersectionVehicles[i] = NULL;
    locks[i] = NULL;
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


bool
right_turn(Vehicle *v) {
  KASSERT(v != NULL);
  if (((v->origin == west) && (v->destination == south)) ||
      ((v->origin == south) && (v->destination == east)) ||
      ((v->origin == east) && (v->destination == north)) ||
      ((v->origin == north) && (v->destination == west))) {
    return true;
  } else {
    return false;
  }
}

void can_enter_intersection(int vehicleCount) {
/*
	LJC: this check is crucial, especially check vehicles[i]==NULL. 
	Because when some thread run the check_constraint function, 
	other threads within vehicles may not even be initialized !!
    */
    for (int i=0; i<MAX_THREADS; i++) {
        lock_acquire(locks[vehicleCount]);
        if ((i==vehicleCount) || (intersectionVehicles[i] == NULL)) {
          continue;
        }
    /* no conflict if both vehicles have the same origin */
        if (intersectionVehicles[i]->origin == intersectionVehicles[vehicleCount]->origin) {
          continue;
        }
    /* no conflict if vehicles go in opposite directions */
        if ((intersectionVehicles[i]->origin == intersectionVehicles[vehicleCount]->destination) &&
        (intersectionVehicles[i]->destination == intersectionVehicles[vehicleCount]->origin)) {
          continue;
        }
    /* no conflict if one makes a right turn and 
       the other has a different destination */
        if ((right_turn(intersectionVehicles[i]) || right_turn(intersectionVehicles[vehicleCount])) &&
  (intersectionVehicles[vehicleCount]->destination != intersectionVehicles[i]->destination)) {
          continue;
        } else {
          cv_wait(intersectionCV, locks[vehicleCount]);
          lock_release(locks[vehicleCount]);
          return;
        }
    }
    return;
}


void
intersection_before_entry(Direction origin, Direction destination) 
{
  Vehicle v;
  v.origin = origin;
  v.destination = destination;
  intersectionVehicles[vehicleCount] = &v;
  vehicleCount += 1;

  can_enter_intersection(vehicleCount);

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
  intersectionVehicles[vehicleCount] = NULL;

  (void) origin;
  (void) destination;

  lock_acquire(locks[vehicleCount]);
  cv_broadcast(intersectionCV, locks[vehicleCount]);
  lock_release(locks[vehicleCount]);

  vehicleCount -= 1;

  return;

}

