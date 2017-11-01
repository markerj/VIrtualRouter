# 457Project3

## How to run (Part 1):
1.) start mininet on eos
2.) Login to mininet
3.) Either clone this directry where you want or switch to directory where you previously cloned.
4.) Pull to make sure it's up to date
5.) Use ifconfig to obtain mininet IP address
6.) ssh into this IP in eos terminal by typing "ssh -Y mininet@ipaddressfromstep5"
7.) Switch to github directory
8.) Run "sudo python prj3-net.py"
9.) Open r1 and h1 terminal using "xterm r1" and "xterm h1"
10.) Compile route2.c by typing "gcc -o route2 route2.c" (in either terminal) (ignore compile error for now)
11.) Inside the r1 terminal run the program by typing "./route2"
12.) You can now ping router1 by typing "ping 10.1.0.1" inside h1 terminal
13.) DONE
