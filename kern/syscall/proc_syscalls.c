#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include <mips/trapframe.h>
#include <spl.h>
#include <array.h>
//#include <limits.h> // L: added
#include <test.h> // L: added
#include <vm.h> // L: added
#include <vfs.h> //L: added
#include <kern/fcntl.h>


// L: #define EXECV_MAX_ARG_SIZE PATH_MAX

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */


//static struct lock *sys_exit_lock;

void sys__exit(int exitcode) {

  int ss = splhigh();

  DEBUG(DB_SYSCALL_E, "sys_exit: start to run sys_exit for proc (%d)\n", curproc->pid);
  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

  //DEBUG(DB_SYSCALL_E,"Syscall: _exit(%d)\n",exitcode);
  //if (sys_exit_lock == NULL) {
  //  sys_exit_lock = lock_create("sys_exit_lock");
  //}
  //lock_acquire(sys_exit_lock);

  KASSERT(curproc->p_addrspace != NULL);

  //remove attached child process, and allow them to exit
  //for (unsigned int i=0; i <array_num(&p->p_children); i++) {
   // struct proc *child = array_get(&p->p_children, i);
   // lock_release(child->p_exit_lock);
    //array_remove(&p->p_children, i);
  //}
  //array_cleanup(&p->p_children);
  
  // remove children from backward: 
  // because of unsigned int, so we have to use i>0 as judge criteria:
  for (unsigned int i=array_num(&p->p_children); i>0; i--) {
    struct proc *child = array_get(&p->p_children, i-1);
    lock_release(child->p_exit_lock);
    array_remove(&p->p_children,i-1);
  }
  DEBUG(DB_SYSCALL_E, "sys_exit: # of children remains after remove loop is (%d)\n", array_num(&p->p_children) );
  KASSERT(array_num(&p->p_children) == 0);

  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  p->p_exited = true;
  p->p_exit_code = _MKWAIT_EXIT(exitcode);
  cv_broadcast(p->p_hold_cv, p->p_hold_lock); //L: when is this p_wait_lk acquired ?? <- see sys_waitpid

  /*
    make sure the p_exit_lock is free (not hold by parent proc)
  */
  lock_acquire(p->p_exit_lock);
  lock_release(p->p_exit_lock);
  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */

  //lock_release(sys_exit_lock);
  proc_destroy(p);

  splx(ss);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  *retval = curproc->pid;
  //DEBUG(DB_SYSCALL_E,"At sys_getpid: the pid is (%d)\n", *retval);
  //*retval = 0;
  return(0);
}



/* stub handler for waitpid() system call                */
int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  DEBUG(DB_SYSCALL_E,"Start sys_waitpid: wait for thread with pid: (%d)\n", pid);
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  // search pid on proc_list
  struct proc* wait_proc = NULL;
  for (unsigned int i=0; i<array_num(proc_list); i++) {
    struct proc* tmp_proc = array_get(proc_list, i);
    if (pid == tmp_proc->pid) {
       wait_proc = tmp_proc;
       break;
    }
  }

  if (options != 0) {
    return(EINVAL); // The options argument requested invalid or unsupported options.
  }
  if (wait_proc == NULL) {
    DEBUG(DB_SYSCALL_E,"sys_waitpid Fail: The pid argument named a nonexistent process.: (%d)\n", pid);
    return ESRCH; // The pid argument named a nonexistent process.
  }
  if (wait_proc == curproc) {
    return ECHILD; //The pid argument named a process that the current process was not interested in or that has not yet exited.
  }
  
  lock_acquire(wait_proc->p_hold_lock);
  while (!wait_proc->p_exited) {
    cv_wait(wait_proc->p_hold_cv, wait_proc->p_hold_lock);
  }
  lock_release(wait_proc->p_hold_lock);

  exitstatus = wait_proc->p_exit_code;
  //exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  /*
    L: the exit_code will take effect from here: 
    we the the p_exit_code form the proc that we wait
    copy it into the memory location points by status. 
    status is a pointer pass into our function from syscall
  */
  if (result) {
    return(result);
  }

  *retval = pid; // return the pid for parent process (see syscall.c)
  return(0);
}


/* sys_fork system call */
int
sys_fork(struct trapframe *tf, 
          pid_t *retval) 
{
  DEBUG(DB_SYSCALL_E,"\nStart sys_fork: called by proc with pid: (%d)\n", curproc->pid);
  //int ss = splhigh();
  struct proc *cur_proc = curproc;

  struct proc *new_proc = proc_create_runprogram(cur_proc->p_name);
  if (new_proc == NULL) {
    DEBUG(DB_SYSCALL_E, "sys_fork Fail: proc_create_runprogram fail\n");
    return ENPROC;
  }
  DEBUG(DB_SYSCALL_E,"At sys_fork: child proc's pid is (%d)\n\n", new_proc->pid);

  //copy address space:
  as_copy(curproc_getas(), &(new_proc->p_addrspace));
  if (new_proc->p_addrspace == NULL) {
    DEBUG(DB_SYSCALL_E, "sys_fork Fail: fail to copy address\n");
    proc_destroy(new_proc);
    return ENOMEM;
  }

  // copy trapframe: 
  struct trapframe *new_tf = kmalloc(sizeof(struct trapframe));
  if (new_tf == NULL) {
    proc_destroy(new_proc);
    return ENOMEM;
  }
  memcpy(new_tf, tf, sizeof(struct trapframe));
  //DEBUG(DB_SYSCALL_E, "copy trapframe success\n");

  int result = thread_fork(curthread->t_name, new_proc, &enter_forked_process, new_tf, 0);
  if (result) {
    DEBUG(DB_SYSCALL_E, "sys_fork Fail: thread_fork fail\n");
    proc_destroy(new_proc);
    kfree(new_tf);
    return result;
  }

  array_add(&cur_proc->p_children, new_proc, NULL);

  lock_acquire(new_proc->p_exit_lock);

  *retval = new_proc->pid;
  //splx(ss);

  return 0;
}

// A2b:
int sys_execv(char *program/*, char **args*/, int32_t *retval) {

//  TODO: consider to merge sys_execv with runprogram to avoid duplication
  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;

  /* Open the file. */
  result = vfs_open(program, O_RDONLY, 0, &v);
  if (result) {
    //return result;
    *retval = result;
    return 1;
  }

  /* Create a new address space. */
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    *retval = ENOMEM;
    return 1;
  }

  // if there exists an old as, deactivate it and delete it
  if (curproc_getas() != NULL) {
    as_deactivate();
  }

  // destroy the old address space and activate the new create one
  struct addrspace * oldas = curproc_setas(as); 
  if (oldas != NULL) {
    as_destroy(oldas);
  }
  as_activate();



  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    *retval = result;
    return 1;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    *retval = result;
    return 1;
  }

  /* Warp to user mode. */
  enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
        stackptr, entrypoint);
  
  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  *retval = EINVAL;
  return 1;
}
