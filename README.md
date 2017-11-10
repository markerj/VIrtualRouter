# 457Project3

## How to run (Part 1):
1. start mininet on eos
2. Login to mininet
3. Either clone this directory where you want or switch to directory where you previously cloned.
4. Pull to make sure it's up to date
5. Use ifconfig to obtain mininet IP address
6. ssh into this IP in eos terminal by typing "ssh -Y mininet@ipaddressfromstep5"
7. Switch to github directory
8. Run "sudo python prj3-net.py"
9. Open r1 and h1 terminal using "xterm r1" and "xterm h1"
10. Compile Part1.c by typing "gcc -o Part1 Part1.c" (in either terminal) (ignore compile error for now)
11. Inside the r1 terminal run the program by typing "./Part1"
12. You can now ping router1 by typing "ping 10.1.0.1" inside h1 terminal
13. DONE

## How to run (Part 2):
1. start mininet on eos
2. Login to mininet
3. Either clone this directory where you want or switch to directory where you previously cloned.
4. Pull to make sure it's up to date
5. Use ifconfig to obtain mininet IP address
6. ssh into this IP in eos terminal by typing "ssh -Y mininet@ipaddressfromstep5"
7. Switch to github directory
8. Run "sudo python prj3-net.py"
9. Compile Part2.c by typing "gcc -o Part2 Part2.c -lpthread" (must use -lpthread flag now that the program implements threading) (ignore compile error for now)

You can now ping each router from each of it's respective hosts (r1 - h1,h2 | r2 - h3,h4,h5) simultaneously. This is because mulithreading is implemented which creates a new thread for each socket for each interface when the program is run on one or both of the routers. Refer to network diagram on project page for IP addresses to ping each router from their respective hosts.

You can also now ping any host from any other host and the 'local' router (one directly connected to host sending ping) will print the mac address of the next hop. This next hop will either be the destination (in the case it's a neighbor host) or it will be the non local router address (in the case the packet needs to be forwarded across machines (from r1 to r2 or vice versa))

Actually forwarding the packet to the "next hop" address will be implemented in Part 3.

## How to run (Part 3):
Steps 1-9 remain the same from Part 2. Working on adding forwarding features
