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




#define MAX_THREADS 10  // keep this number same as traffic.c 

typedef struct Vehicles
{
  Direction origin;
  Direction destination;
  struct lock* lk; 
} Vehicle;

//static struct semaphore *intersectionSem;
static struct cv* intersectionCV;
static Vehicle* volatile intersectionVehicles[MAX_THREADS];
static struct lock* enter_lock;
static struct lock* exit_lock;

 /*
the intersectionVehicles list here is functionality similar to the vechiles list
in traffic.c file. But intersectionVehicles here don't directly related with the
actual thread. It just serve as an book keeper to see if there is any conflict

therefore in the 'intersection_after_exit' we can just scan the vehicle and remove
any vehicle that has the origin and destination we want. Although this vehicle may
not actually created by the right thread, but it doesn't affect our program. 

 */
 //static struct lock* volatile locks[MAX_THREADS];
// static volatile int vehicleCount;

/* functions that defined and used internally */
static bool right_turn(Vehicle *v);
static void can_enter_intersection(Vehicle* v);
void intersection_before_entry(Direction origin, Direction destination);
void intersection_after_exit(Direction origin, Direction destination);


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

  intersectionCV = cv_create("intersectionBusy");
  // vehicleCount = 0;
  //locks = kmalloc(sizeof(struct lock *)* MAX_THREADS);
  enter_lock = lock_create("enter_lock");
  exit_lock = lock_create("exit_lock");
  if (intersectionCV == NULL) {
    panic("Couldn't create intersectionCV");
  }
  for (int i=0; i<MAX_THREADS; i++) {
    intersectionVehicles[i] = (Vehicle * volatile) NULL;
    //locks[i] = lock_create("");
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
  cv_destroy(intersectionCV);

  for (int i=0; i<MAX_THREADS; i++) {
    intersectionVehicles[i] = NULL;
    //lock_destroy(locks[i]);
    //locks[i] = NULL;
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

void can_enter_intersection(Vehicle* v) {
  /*
	LJC: this check is crucial, especially check vehicles[i]==NULL. 
	Because when some thread run the check_constraint function, 
	other threads within vehicles may not even be initialized !!
  */
  int safe = 0;
  while (safe==0) {
    for (int i=0; i<MAX_THREADS; i++) {
        //lock_acquire(locks[vehicleCount-1]); // ERROR to be remember!! 
/*
the above lock_acquire will cause deadlock!! each time the for loop runs, it 
will try to acquire the lock. after a certain number of loop, all current 
threads will be waiting for the lock and therefore leads to deadlock. s
*/
        if ((intersectionVehicles[i] == NULL)) {
//kprintf("c1");
          if (i==MAX_THREADS-1) safe=1;
          continue;
        }
    /* no conflict if both vehicles have the same origin */
        if (intersectionVehicles[i]->origin == v->origin) {
//kprintf("c2");
          if (i==MAX_THREADS-1) safe=1;
          continue;
        }
    /* no conflict if vehicles go in opposite directions */
        if ((intersectionVehicles[i]->origin == v->destination) &&
        (intersectionVehicles[i]->destination == v->origin)) {
//kprintf("c3");
          if (i==MAX_THREADS-1) safe=1;
          continue;
        }
    /* no conflict if one makes a right turn and 
       the other has a different destination */
        if ((right_turn(intersectionVehicles[i]) || right_turn(v)) &&
  (v->destination != intersectionVehicles[i]->destination)) {
//kprintf("c4");
          if (i==MAX_THREADS-1) safe=1;
          continue;
        }         
// kprintf("have conflict: put vehicle %d into cv\n", vehicleCount);
        //lock_acquire(locks[vehicleCount]);
        //lock_acquire(v->lk);
// here: should also remove this vehicle from the intersectionVehicles list !!
// because we are using intersectionVehicles to make judgement !
// otherwise this will cause deadlock
        //cv_wait(intersectionCV, locks[vehicleCount]);
        cv_wait(intersectionCV, enter_lock);
        //lock_release(locks[vehicleCount]);
        //lock_release(v->lk);
        break; // this break the for loop, and go back to while loop
        
    }
  }
  return;
}


void
intersection_before_entry(Direction origin, Direction destination) 
{
  // this vehicle need to exist after the intersection_before_entry function exit
  Vehicle* v = kmalloc(sizeof(Vehicle));
  v->origin = origin;
  v->destination = destination;
  v->lk = lock_create("");
  KASSERT(4 > v->origin);
  KASSERT(4 > v->destination);
  KASSERT(v->origin != v->destination);

  lock_acquire(enter_lock);
  can_enter_intersection(v);

//kprintf("finish_can_enter_intersection check, now enter intersection");

  // first find an available position in intersectionVehicles
  int index = 0;
  for (int i=0; i<MAX_THREADS; i++) {
    if (intersectionVehicles[i] == NULL) {
      index = i;
      break;
    }
  }
  intersectionVehicles[index] = v;

//kprintf("intersection has those vehicles: \n");
//for (int j=0; j<MAX_THREADS; j++) {
//    if (intersectionVehicles[j] != NULL) {
//        print_direction(intersectionVehicles[j]->origin);
//        kprintf("->");
//        print_direction(intersectionVehicles[j]->destination);
//        kprintf("\n");
//    }
//}
//kprintf("finish printing all vehicles\n");
  lock_release(enter_lock);

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
  lock_acquire(exit_lock);
  int free_success = 0; // flag to make sure we do find the right vehicle to free
  // this is wrong !! should not just decrease the count and remove the last vehicle
  // because the vehicle get free may not be the most recent one !! 
//kprintf("I am freeing a vehicle!\n");

  for (int i=0; i<MAX_THREADS; i++) {
    if (intersectionVehicles[i] != NULL) {
        if ((intersectionVehicles[i]->origin == origin) && 
         (intersectionVehicles[i]->destination == destination)) {
         //lock_acquire(intersectionVehicles[i]->lk);
         //lock_release(intersectionVehicles[i]->lk);
         intersectionVehicles[i] = NULL; // this can be potential probelmatic 
        // should really make sure this set to NULL tgt with cv_broadcast
	     free_success = 1;
//kprintf("at least one such free is success ! \n"); 
//kprintf("the free_success value is: %d\n", free_success);
        }
    }
  }
  cv_broadcast(intersectionCV, exit_lock);
//kprintf("the (again) free_success value is: %d\n", free_success);
  //KASSERT(free_success == 1); // Q: I still don't understand why this is not right?
/*
About cv: thread A may added itself into cv, and wait for another thread B to 
wake itself up! So thread A will not wake itself
*/
  lock_release(exit_lock);

  return;

}


