#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <linux/ip.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

//##################################################################################################################
//                                                Packet header structs                                            #
//##################################################################################################################

//arp header
struct arpheader {
    unsigned short hardware;            // Format of hardware address.
    unsigned short protocol;            // Format of protocol address.
    unsigned char hardware_length;      // Length of hardware address.
    unsigned char protocol_length;      // Length of protocol address.
    unsigned short op;                  // ARP opcode (1 == request | 2 == reply).
    unsigned char src_addr[6];          // Source mac address.
    unsigned char src_ip[4];            // Source IP address.
    unsigned char dst_addr[6];          // Destination mac address.
    unsigned char dst_ip[4];            // Destination IP address.
};

//eth header
struct ethheader {
    unsigned char eth_dst[6];           //ethernet destination
    unsigned char eth_src[6];           //ethernet source
    unsigned short eth_type;            //ethernet type (0x0806 == ARP | 0x0800 == IP)
};

//icmp header
struct icmpheader {
    uint8_t type;                       //message type
    uint8_t code;                       //type sub code
    uint16_t checksum;                  //checksum of icmp
    uint16_t id;                        //random number
    uint16_t seq;                       //seq #
    uint32_t data;                      //data sent in icmp
};

//ip header
struct ipheader {
    uint8_t ihl:4, version:4;           //ihl version
    uint8_t tos;                        //tos
    uint16_t tot_len;                   //total length
    uint16_t id;                        //random number
    uint16_t frag_off;                  //fragmentation offset
    uint8_t ttl;                        //time to live (some default)
    uint8_t protocol;                   //protocol
    uint16_t checksum;                  //checksum for ip
    unsigned char src_ip[4];            //source ip
    unsigned char dst_ip[4];            //source destination

};

//##################################################################################################################
//                                                Global Variables                                                 #
//##################################################################################################################

int openthreads = 0;
int exitProgram = 0;
int numInterfaces = 0;
pthread_t tids[10];
int threadNums[10];
int routerNum = 0;

//routerAddresses has 8 strings of max size 9 bytes
//indexes 0-3 correspond to r1 interface addresses
//indexes 4-7 correspond to r2 interface addresses
//Example: routerAddresses[0] == r1 eth0 ip address | routerAddresses[4] == r2 eth0 ip address
//To calculate correct index based on router number and interface number do...[((routerNum-1)*4) + ethNum]
//r1 has 3 interfaces: eth0 - eth2
//r2 has 4 interfaces: eth0 - eth3
//So if routerNum == 1 and ethNum == 1 the corresponding ipAddress is in index 1
//So if routerNum == 2 and ethNum == 2 the corresponding ipAddress is in index 6
const char routerAddresses[8][9] =
        {"10.0.0.1", "10.1.0.1", "10.1.1.1", "", "10.0.0.2", "10.3.0.1", "10.3.1.1", "10.3.4.1"};

char routerOneRoutingInfo[4][30] = {"", "", "", ""};
char routerTwoRoutingInfo[5][30] = {"", "", "", "", ""};


//##################################################################################################################
//                                              Checksum Calculation                                               #
//##################################################################################################################

//in_chksum from Berkely Software Distribution
uint16_t in_chksum(unsigned char *addr, int len) {
    int nleft = len;
    const uint16_t *w = (const uint16_t *) addr;
    uint32_t sum = 0;
    uint16_t answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    // mop up an odd byte, if necessary
    if (nleft == 1) {
        *(unsigned char *) (&answer) = *(const unsigned char *) w;
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum & 0xffff) + (sum >> 16);
    sum += (sum >> 16);

    answer = ~sum;    // truncate to 16 bits
    return answer;
}

//##################################################################################################################
//                                              IP address to String                                               #
//##################################################################################################################

char * ipAddressToString(char *inputIP)
{
    static char ipString[9];
    unsigned char *p = inputIP;
    sprintf(ipString, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
    return(ipString);


}

//##################################################################################################################
//                                              Append Char to String                                              #
//##################################################################################################################

void appendChar(char* s, char c)
{
    int len = strlen(s);
    s[len] = c;
    s[len+1] = '\0';
}

//##################################################################################################################
//                                        ignal Handler (To exit on Ctrl-C)                                        #
//##################################################################################################################

void exitprog(int sig)
{
    exitProgram = 1;
    printf(" received\n");
    printf("Waiting for all threads to close..\n");
}



//##################################################################################################################
//                                                Interface Threads                                                #
//##################################################################################################################

void *interfaces(void *args)
{
    int ethNum = *((int *)args);

    printf("Created thread for interface eth%d\n", ethNum);

    struct ethheader *ethhdr, *ethhdrsend;
    struct arpheader *arphdr, *arphdrsend;
    struct ipheader *iphdr, *iphdrsend;
    struct icmpheader *icmphdr, *icmphdrsend;
    struct sockaddr_ll *getaddress;
    unsigned char localadr[6];

    int packet_socket;
    //get list of interfaces (actually addresses)
    struct ifaddrs *ifaddr, *tmp;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
    }
    //have the list, loop over the list
    for (tmp = ifaddr; tmp != NULL; tmp = tmp->ifa_next) {
        //Check if this is a packet address, there will be one per
        //interface.  There are IPv4 and IPv6 as well, but we don't care
        //about those for the purpose of enumerating interfaces. We can
        //use the AF_INET addresses in this list for example to get a list
        //of our own IP addresses
        if (tmp->ifa_addr->sa_family == AF_PACKET) {
            //printf("Interface: %s\n", tmp->ifa_name);

            //create a packet socket on interface r?-eth1
            char ethName[5];
            sprintf(ethName, "eth%d", ethNum);
            if (!strncmp(&(tmp->ifa_name[3]), ethName, 4)) {
                printf("From eth%d thread: Creating Socket on interface %s\n",ethNum, tmp->ifa_name);

                //get local mac address
                getaddress = tmp->ifa_addr;
                memcpy(localadr, getaddress->sll_addr, 6);

                //create a packet socket
                //AF_PACKET makes it a packet socket
                //SOCK_RAW makes it so we get the entire packet
                //could also use SOCK_DGRAM to cut off link layer header
                //ETH_P_ALL indicates we want all (upper layer) protocols
                //we could specify just a specific one
                packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
                if (packet_socket < 0) {
                    perror("socket");
                }
                //Bind the socket to the address, so we only get packets
                //recieved on this specific interface. For packet sockets, the
                //address structure is a struct sockaddr_ll (see the man page
                //for "packet"), but of course bind takes a struct sockaddr.
                //Here, we can use the sockaddr we got from getifaddrs (which
                //we could convert to sockaddr_ll if we needed to)
                if (bind(packet_socket, tmp->ifa_addr, sizeof(struct sockaddr_ll)) == -1) {
                    perror("bind");
                }
            }
        }
    }
    //free the interface list when we don't need it anymore
    //May need to comment out if we are keeping pointers to address list
    //freeifaddrs(ifaddr);

    //loop and recieve packets. We are only looking at one interface,
    //for the project you will probably want to look at more (to do so,
    //a good way is to have one socket per interface and use select to
    //see which ones have data)


    printf("From eth%d thread: Ready to receive now\n", ethNum);
    while (exitProgram == 0) {
        char buf[1500], sendbuf[1500];
        struct sockaddr_ll recvaddr;
        int recvaddrlen = sizeof(struct sockaddr_ll);

        int result;
        fd_set readset;
        struct timeval tv;

        FD_ZERO(&readset);
        FD_SET(packet_socket, &readset);

        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        result = select(packet_socket+1, &readset, NULL, NULL, &tv);

        if(result > 0)
        {
            //we can use recv, since the addresses are in the packet, but we
            //use recvfrom because it gives us an easy way to determine if
            //this packet is incoming or outgoing (when using ETH_P_ALL, we
            //see packets in both directions. Only outgoing can be seen when
            //using a packet socket with some specific protocol)
            int n = recvfrom(packet_socket, buf, 1500, 0, (struct sockaddr *) &recvaddr, &recvaddrlen);
            //ignore outgoing packets (we can't disable some from being sent
            //by the OS automatically, for example ICMP port unreachable
            //messages, so we will just ignore them here)
            if (recvaddr.sll_pkttype == PACKET_OUTGOING) {
                continue;
            }
            //start processing all others
            printf("\n");
            printf("From eth%d thread: Got a %d byte packet\n", ethNum, n);

            iphdr = (struct ipheader *) (buf + sizeof(struct ethheader));
            ethhdr = (struct ethheader *) buf;
            arphdr = (struct arpheader *) (buf + sizeof(struct ethheader));

            //if eth_type is of type ARP and router IP address matches correct interface then send ARP reply
            if (ntohs(ethhdr->eth_type) == 0x0806 &&
                    !strncmp(ipAddressToString(arphdr->dst_ip), routerAddresses[((routerNum-1)*4) + ethNum], 9))
            {

                printf("From eth%d thread: Got arp request\n", ethNum);

                printf("From eth%d thread: Building arp header\n", ethNum);
                //fill arp header
                arphdrsend = (struct arpheader *) (sendbuf + sizeof(struct ethheader));
                arphdrsend->hardware = htons(1);
                arphdrsend->protocol = htons(ETH_P_IP);
                arphdrsend->hardware_length = 6;
                arphdrsend->protocol_length = 4;
                arphdrsend->op = htons(2);
                memcpy(arphdrsend->src_addr, localadr, 6);
                memcpy(arphdrsend->src_ip, arphdr->dst_ip, 4);
                memcpy(arphdrsend->dst_addr, arphdr->src_addr, 6);
                memcpy(arphdrsend->dst_ip, arphdr->src_ip, 4);

                printf("From eth%d thread: Building ethernet header\n", ethNum);
                //fill ethernet header
                ethhdrsend = (struct ethheader *) sendbuf;
                memcpy(ethhdrsend->eth_dst, ethhdr->eth_src, 6);
                memcpy(ethhdrsend->eth_src, ethhdr->eth_dst, 6);
                ethhdrsend->eth_type = htons(0x0806);

                printf("From eth%d thread: Attempting to send arp reply\n", ethNum);
                //send arp reply
                send(packet_socket, sendbuf, 42, 0);

            }

                //if eth_type is of type IP and router IP address matches correct interface then must be ICMP packet
            else if (ntohs(ethhdr->eth_type) == 0x0800 &&
                    !strncmp(ipAddressToString(iphdr->dst_ip), routerAddresses[((routerNum-1)*4) + ethNum], 9))
            {
                icmphdr = (struct icmpheader *) (buf + sizeof(struct ethheader) + sizeof(struct ipheader));
                printf("From eth%d thread: Received ICMP ECHO\n", ethNum);

                //ICMP echo request
                if (icmphdr->type == 8) {
                    printf("From eth%d thread: Received ICMP ECHO request\n", ethNum);


                    //copy received packet to send back
                    memcpy(sendbuf, buf, 1500);

                    printf("From eth%d thread: Building ICMP header\n", ethNum);
                    //fill ICMP header
                    icmphdrsend = ((struct icmpheader *) (sendbuf + sizeof(struct ethheader) +
                            sizeof(struct ipheader)));
                    icmphdrsend->type = 0;
                    icmphdrsend->checksum = 0;
                    icmphdrsend->checksum = in_chksum((char *) icmphdrsend,
                                                      (1500 - sizeof(struct ethheader) - sizeof(struct ipheader)));

                    printf("From eth%d thread: Building IP header\n", ethNum);
                    //fill IP header
                    iphdrsend = (struct ipheader *) (sendbuf + sizeof(struct ethheader));
                    memcpy(iphdrsend->src_ip, iphdr->dst_ip, 4);
                    memcpy(iphdrsend->dst_ip, iphdr->src_ip, 4);

                    printf("From eth%d thread: Building ethernet header\n", ethNum);
                    //fill ethernet header
                    ethhdrsend = (struct ethheader *) sendbuf;
                    memcpy(ethhdrsend->eth_dst, ethhdr->eth_src, 6);
                    memcpy(ethhdrsend->eth_src, ethhdr->eth_dst, 6);

                    printf("From eth%d thread: Attempting to send ICMP response\n", ethNum);
                    //send ICMP respsonse packet
                    send(packet_socket, sendbuf, 98, 0);
                }


            }
        }
        else
        {
            //printf("From eth%d thread: Didn't receive anything..\n", ethNum);
        }

    }

    printf("Exiting eth%d thread\n", ethNum);
}



//##################################################################################################################
//                                                   Main Thread                                                   #
//##################################################################################################################

int main()
{
    signal(SIGINT, exitprog);

    //Find out how many interface threads to create
    struct ifaddrs *ifaddr, *tmp;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return 1;
    }
    //have the list, loop over the list
    for (tmp = ifaddr; tmp != NULL; tmp = tmp->ifa_next)
    {
        if (tmp->ifa_addr->sa_family == AF_PACKET)
        {
            numInterfaces++;
        }
    }

    //printf("There are %d total interfaces\n", numInterfaces);

    //Figure out if this is r1 or r2. Used for receive packet logic
    if(numInterfaces == 4)
    {
        routerNum = 1;
    }
    else if(numInterfaces == 5)
    {
        routerNum = 2;
    }

    //Read in correct routing table info
    if(routerNum == 1)
    {
        FILE* file = fopen("r1-table.txt", "r");
        char line[30] = "";
        int i = 0;
        while(!feof(file))
        {
            fgets(line, sizeof(line), file);
            if(line[strlen(line) - 1] == '\n')
            {
                line[strlen(line) - 1] = '\0';
            }
            strcpy(routerOneRoutingInfo[i], line);
            i++;
        }
        fclose(file);
    }

    //print routing table info to check valid info
    int i;
    int numLines = sizeof(routerOneRoutingInfo)/sizeof(*routerOneRoutingInfo);
    printf("Num entries in routing table1 = %d\n", numLines);
    for(i = 0; i < numLines; i++)
    {
        printf("%s\n", routerOneRoutingInfo[i]);
        i++;
    }

    printf("Creating threads for each interface..\n");

    //int i;
    int status;

    //create interface threads. Create numInterfaces-1 threads because we don't need a thread for lo interface
    for(i = 0; i < numInterfaces-1; i++)
    {
        threadNums[i] = i;
        if((status = pthread_create(&tids[i], NULL, interfaces, &threadNums[i])) != 0)
        {
            fprintf(stderr, "thread create error %d: %s", status, strerror(status));
        }
        openthreads++;
    }

    //waits for threads to exit
    for(i = 0; i < numInterfaces-1; i++)
    {
        pthread_join(tids[i], NULL);
    }

    printf("All threads closed!\n");
    printf("Shutting down. Goodbye!\n");

    return 0;
}
