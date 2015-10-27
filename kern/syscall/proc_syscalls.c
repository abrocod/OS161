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

// L: #define EXECV_MAX_ARG_SIZE PATH_MAX

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);

  //remove attached child process, and allow them to exit
  for (unsigned int i=0; i <array_num(&p->p_children); i++) {
    struct proc *child = array_get(&p->p_children, i);
    lock_release(child->p_exit_lock);
    array_remove(&p->p_children, i);
  }
  //L: KASSERT(array_num(&p->p_children) == 0);

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

  //lock_acquire(p->p_exit_lock);
  //lock_release(p->p_exit_lock);
  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  *retval = curproc->pid;
  DEBUG(DB_SYSCALL,"At sys_getpid: the pid is (%d)\n", *retval);
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
    }
  }

  if (wait_proc == NULL) {
    return ESRCH; // don't find the right pid
  }

  if (options != 0) {
    return(EINVAL);
  }
  
  lock_acquire(wait_proc->p_hold_lock);
  while (!wait_proc->p_exited) {
    cv_wait(wait_proc->p_hold_cv, wait_proc->p_hold_lock);
  }
  lock_release(wait_proc->p_hold_lock);

  exitstatus = wait_proc->p_exit_code;
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));

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
  struct proc *new_proc = proc_create_runprogram(curproc->p_name);
  if (new_proc == NULL) {
    DEBUG(DB_SYSCALL, "sys_fork: proc_create_runprogram fail");
    return ENPROC;
  }

  //copy address space:
  as_copy(curproc_getas(), &(new_proc->p_addrspace));

  // copy trapframe: 
  struct trapframe *new_tf = kmalloc(sizeof(struct trapframe));
  memcpy(new_tf, tf, sizeof(struct trapframe));
  DEBUG(DB_SYSCALL, "copy trapframe success");

  int result = thread_fork(curthread->t_name, new_proc, &enter_forked_process, new_tf, 0);
  if (result) {
    DEBUG(DB_SYSCALL, "sys_fork: thread_fork fail");
    proc_destroy(new_proc);
    return result;
  }

  array_add(&curproc->p_children, new_proc, NULL);

  lock_acquire(new_proc->p_exit_lock);

  *retval = new_proc->pid;

  return 0;
}


