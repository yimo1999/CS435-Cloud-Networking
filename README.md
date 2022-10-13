# CS435-Cloud-Networking

# Programming Assignment 1: HTTP Client
The purpose of this programming assignment is to learn basic network programming and protocols, and to exercise your background in C/C++ programming.  You'll also become familiar with the environment to be used in the class and the procedure for handing in machine problems. 

You will write, compile and run a simple network program, and implement an HTTP client program that can load web pages and objects. For this assignment, you will work on either the provided Docker container or the provided virtual machine. The instructions for setting up and running Docker containers are provided in Week 1 Programming Assignment Preparation part. The instructions for installing the virtual machine can be found in the same location.



# Programming Assignment 2: Unicast Routing
In this assignment, you will implement traditional shortest-path routing, with the link state (LS) or distance/path vector (DV/PV) protocols. 

The test setup will be the same environment as in MP1, you can use either Docker container or a VM as the environment. Your nodes will use different virtual network adapters, with iptables restricting communication among them. We are providing you with the same script that our testing environment uses to establish these restrictions.

This MP introduces the unfortunate distinction between “network order” and “host order”, AKA endianness. The logical way to interpret 32 bits as an integer is to call the left-most bit the highest, and the right-most the lowest. This is the standard that programs sending raw binary integers over the internet are expected to follow. Due to historical quirks, however, x86 processors store integers reversed at the byte level. Look up htons(), htonl(), ntohs(), ntohl() for information on how to deal with it.
