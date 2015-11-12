/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>

#include <limits.h> // L: added
#include <vm.h> // L: added
#include <vfs.h> //L: added
#include <copyinout.h> //L: added
/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
//int
//runprogram(char *progname)
int runprogram(char *progname, char **kernel_args, int argc) 
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	//KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();



	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}




	// added for A2b: 

	DEBUG(DB_SYSCALL_E, "runprogram: parpare to copy arguments into user space\n ");
	// newargs, in case args isn't defined
	char *empty_args[1];
	empty_args[0] = NULL;
	if (kernel_args == NULL) {
		kernel_args = empty_args;
	}

  int args_total_len = 0;

  for (int i = 0; i < argc; i++) {
    int arglen = strlen(kernel_args[i]);
    args_total_len += arglen + 1; // +1 for \0
  } 
  if (args_total_len > ARG_MAX) {
    return E2BIG;
  }

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

  DEBUG(DB_SYSCALL_E, "runprogram: start to copy arguments into user space\n ");

  for (int j = 0; j < argc; j++) {
    char * arg = kernel_args[j]; // L: original is args[i]
    userptr_t dest = (userptr_t)((char *)user_argv_val + argv_val_offset);
    result = copyoutstr(arg, dest, strlen(arg) + 1, &len);
    if (result) {
      DEBUG(DB_SYSCALL_E, "runprogram:  args copy to user space fail .1\n ");
      return result;
    }
    argv[j] = (char *)dest;
    argv_val_offset += len;
  }

  argv[argc] = NULL;
  result = copyout(argv, user_argv, (argc + 1) * sizeof(char *));
  if (result) {
    DEBUG(DB_SYSCALL_E, "runprogram:  args copy to user space fail .2\n ");
    return result;
  }



	// end of added for A2b

	/* Warp to user mode. */
	//enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
	//		  stackptr, entrypoint);
	enter_new_process(argc, user_argv, stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

