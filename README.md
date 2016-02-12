# OS/161
OS/161, a simplified POSIX style Operating System, for CS350 by Jinchao Lin, University of Waterloo

## Setup

* [OS/161 Installation Guide for the student.cs computing environment](https://www.student.cs.uwaterloo.ca/~cs350/common/Install161.html)
* [OS/161 Installation Guide for other machines](https://www.student.cs.uwaterloo.ca/~cs350/common/Install161NonCS.html)
* [Working with OS/161](https://www.student.cs.uwaterloo.ca/~cs350/common/WorkingWith161.html)
* [Debugging OS/161 with GDB](https://www.student.cs.uwaterloo.ca/~cs350/common/gdb.html)
* [OS/161 and tools FAQ](https://www.student.cs.uwaterloo.ca/~cs350/common/os161-faq.html)


## Summary of my contribution in this OS/161 implementation:
For general information about this operating course, please visit course website:
https://www.student.cs.uwaterloo.ca/~cs350/F15/

For information about the implementation I made on this operating system, please:
[OS/161 Assignments](https://www.student.cs.uwaterloo.ca/~cs350/F15/assignments/)

In short, what I did in this project can be divided into three parts: 
(1)  Implemented Kernel Synchronization Primitives, including lock and conditional variable. Used them to solve a simulated traffice interesection problem. 

(2) Implemented several OS/161 process-related system calls, including fork, getpid, waitpid, exit and execv

(3) Developed a virtual memory system that can manage TLB, provide read only protection for text segmentation, re-use of physical memory and support discrete physical memory allocation
