# Learning log

This file is written entirely by me, without AI assistance. It records what I learn in each milestone of the project.

## Milestone 1 – Docker environment and minimal TCP listener

### What I built

In this step of the project, i´ve been building de base of the load balancer. Just a single conection to the load balancer from the client, with only one socket.

### What I learned

I learn how to configure docker correctly and how it works at the momment of accepting connections from a localhost. I´ve been searching for how docker use the NAT conecction. I also learn how to implement teoricaly the sockets in the architecture and why.

### What I changed in review

In this review i didn´t change anything

### Open questions

How can i manage multiple conections?

## Milestone 2 – Forward traffic to a single backend

### What I built

In this sesion i build a second socket that conects to the backend1 from the load balancer.

### What I learned

I learned how to manage the flow of information between the client and the backend with the load balancer in the middle, using the poll() method in C++ to make a queue of descriptors and, with this, avoiding a possible deadlock situation

### What I changed in review

I didn´t change anything in this review

### Open questions

The same that in milestone-1. How can i manage multiple connections?
