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

#include <limits.h> // L: added
#include <test.h> // L: added
#include <vm.h> // L: added
#include <vfs.h> //L: added
#include <kern/fcntl.h>


#define EXECV_MAX_ARG_SIZE PATH_MAX

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
//int align_memory(int count, size_t typesize); // declare in test.h

int align_memory(int count, size_t typesize) {
  size_t addrsize = sizeof(userptr_t);
  int bytes = (typesize * count);
  bytes += bytes % addrsize == 0 ? 0 : addrsize - (bytes % addrsize);
  return bytes;
}


//int sys_execv(char *program/*, char **args*/, int32_t *retval) {
int sys_execv(const_userptr_t program, const_userptr_t args[], int32_t *retval) {

/*
 Step I: process args, copy into kernel buffer
*/
  DEBUG(DB_SYSCALL_E, "sys_execv: start step 1\n ");
  int argc = 0;
  size_t program_len = strlen((char *) program) + 1;
  size_t args_total_len = 0;

  // get the number of arguments:
  while (args[argc] != NULL) {
    args_total_len += strlen(((char **)args)[argc]) + 1;
    argc ++;

  }

  if (program_len + args_total_len > ARG_MAX || argc > ARG_MAX/EXECV_MAX_ARG_SIZE) {
    DEBUG(DB_SYSCALL_E, "sys_execv: Too many execv arguments \n");
    *retval = E2BIG;
    return 1;
  }

  int copy_res;  // L: merge copy_res with result
  int cum_copy_len = 0; 

  char kernel_program[program_len];
  char tmp_args[args_total_len]; 
  char *kernel_args[argc];

  // copy program name (prepare kernel_program)
  copy_res = copyinstr(program, kernel_program, program_len, NULL);
  if (copy_res) {
    DEBUG(DB_SYSCALL_E, "within sys_execv: copy program name fail\n");
    *retval = copy_res;
    return 1;
  }

  // copy args into kernel buffer (prepare kargs):
  for (int i=0; i<argc; i++) {
    size_t len;
    size_t arglen = strlen(((char **)args)[i]) + 1;
    copy_res = copyinstr(args[i], (char *)(tmp_args + cum_copy_len), arglen, &len);
    if (copy_res) {
      DEBUG(DB_SYSCALL_E, "within sys_execv: copy args to kernel fail\n");
      *retval = copy_res;
      return 1;
    }
    kernel_args[i] = tmp_args + cum_copy_len;
    cum_copy_len += len;
  }


/*
 Step II: Open the executable, create a new address space and load the elf into it
*/
  DEBUG(DB_SYSCALL_E, "sys_execv: start step 2 \n ");
  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;

  /* Open the file. */
  result = vfs_open(kernel_program, O_RDONLY, 0, &v);
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


/*
 Step 3: Copy the arguments from kernel buffer into user stack
*/

  DEBUG(DB_SYSCALL_E, "sys_execv: start step 3 \n ");

// Don't confused with the definition in step 1:
  // size_t program_len = strlen((char *) program) + 1;
  // size_t args_total_len = 0;

  // compute the size needed for keeping user program's argv 
  // including offset and argv_value
  int offset_mem = align_memory(argc + 1, sizeof(char **));
  int argv_mem = align_memory(args_total_len, sizeof(char));

  // Total size of memory required for all arguments
  int total_mem = offset_mem + argv_mem;

  // move stack pointer to make the space for arguments
  stackptr -= total_mem;

  char *argv[argc + 1]; 
  userptr_t user_argv = (userptr_t)stackptr;
  userptr_t user_argv_val = (userptr_t)(stackptr + offset_mem);

  size_t len;
  int argv_val_offset = 0;

  for (int j = 0; j < argc; j++) {
    char * arg = kernel_args[j]; // L: original is args[i]
    userptr_t dest = (userptr_t)((char *)user_argv_val + argv_val_offset);
    result = copyoutstr(arg, dest, strlen(arg) + 1, &len);
    if (result) {
      DEBUG(DB_SYSCALL_E, "sys_execv: step3: args copy to user space fail .1\n ");
      *retval = result;
      return 1;
    }
    argv[j] = (char *)dest;
    argv_val_offset += len;
  }

  argv[argc] = NULL;
  result = copyout(argv, user_argv, (argc + 1) * sizeof(char *));
  if (result) {
    DEBUG(DB_SYSCALL_E, "sys_execv: step3: args copy to user space fail .2\n ");
    *retval = result;
    return 1;
  }



/*
 Step 4: Return user mode using enter_new_process
*/
  DEBUG(DB_SYSCALL_E, "sys_execv: start step 4 \n ");

  enter_new_process(argc, user_argv, stackptr, entrypoint);

  
  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  *retval = EINVAL;
  return 1;
}
