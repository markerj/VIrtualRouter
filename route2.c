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
    unsigned short eth_type;            //ethernet type (0x806 == ARP | 0x800 == IP)
};

//icmp header
struct icmpheader {
    uint8_t type;                       //message type
    uint8_t code;                       //type sub code
    uint16_t checksum;             	//checksum of icmp
    uint16_t id;                        //random number
    uint16_t seq;                       //seq #
    uint32_t data;                      //data sent in icmp
};

//ip header
struct ipheader {
    unsigned char src_ip[4];            //source ip
    unsigned char dst_ip[4];            //source destination
    uint8_t ttl;                        //time to live (some default)
    uint16_t ip_checksum;               //checksum for ip
    uint16_t id;                        //random number
};


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


int main() {
    struct ethheader *ethhdr, *ethhdrsend;
    struct arpheader *arphdr, *arphdrsend;
    struct ipheader *iphdr, *iphdrsend;
    struct icmpheader *icmphdr, *icmphdrsend;

    int packet_socket;
    //get list of interfaces (actually addresses)
    struct ifaddrs *ifaddr, *tmp;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return 1;
    }
    //have the list, loop over the list
    for (tmp = ifaddr; tmp != NULL; tmp = tmp->ifa_next) {
        //Check if this is a packet address, there will be one per
        //interface.  There are IPv4 and IPv6 as well, but we don't care
        //about those for the purpose of enumerating interfaces. We can
        //use the AF_INET addresses in this list for example to get a list
        //of our own IP addresses
        if (tmp->ifa_addr->sa_family == AF_PACKET) {
            printf("Interface: %s\n", tmp->ifa_name);
            //create a packet socket on interface r?-eth1
            if (!strncmp(&(tmp->ifa_name[3]), "eth1", 4)) {
                printf("Creating Socket on interface %s\n", tmp->ifa_name);
                //create a packet socket
                //AF_PACKET makes it a packet socket
                //SOCK_RAW makes it so we get the entire packet
                //could also use SOCK_DGRAM to cut off link layer header
                //ETH_P_ALL indicates we want all (upper layer) protocols
                //we could specify just a specific one
                packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
                if (packet_socket < 0) {
                    perror("socket");
                    return 2;
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

    printf("Ready to recieve now\n");
    while (1) {
        char buf[1500], sendbuf[1500];
        struct sockaddr_ll recvaddr;
        int recvaddrlen = sizeof(struct sockaddr_ll);
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
        printf("Got a %d byte packet\n", n);

        iphdr = (struct ipheader *) (buf + sizeof(struct ethheader));
        ethhdr = (struct ethheader *) buf;
        arphdr = (struct arpheader *) (buf + sizeof(struct ethheader));

        //if eth_type is of type ARP then send ARP reply
        if (ntohs(ethhdr->eth_type) == 0x806) {
        
            //fill arp header
            arphdrsend = (struct arpheader *) (sendbuf + sizeof(struct ethheader));
            arphdrsend->hardware = htons(1);
            arphdrsend->protocol = htons(ETH_P_IP);
            arphdrsend->hardware_length = 6;
            arphdrsend->protocol_length = 4;
            arphdrsend->op = htons(2);
            memcpy(arphdrsend->src_addr, arphdr->dst_addr, 6);
            memcpy(arphdrsend->src_ip, arphdr->dst_ip, 4);
            memcpy(arphdrsend->dst_addr, arphdr->src_addr, 6);
            memcpy(arphdrsend->dst_ip, arphdr->src_ip, 4);


            //fill ethernet header
            ethhdrsend = (struct ethheader *) sendbuf;
            memcpy(ethhdrsend->eth_dst, ethhdr->eth_src, 6);
            memcpy(ethhdrsend->eth_src, ethhdr->eth_dst, 6);
            ethhdrsend->eth_type = htons(0x806);

            //send arp reply
            sendto(packet_socket, sendbuf, 1500, 0, (struct sockaddr *) &recvaddr, &recvaddrlen);

        }

        //if eth_type is of type IP then must be ICMP packet
        else if (ntohs(ethhdr->eth_type) == 0x800))
        {
            icmphdr = (struct icmphdr *) (buf + sizeof(struct ethheader) + sizeof(struct ipheader));

            //ICMP echo request
            if (icmphdr->type == 8) {
                printf("Received ICMP ECHO request from %s (code: %u  id: %u  seq: %u)",
                       inet_ntoa(*(struct in_addr *) &iphdr->src_ip),
                       ntohs(icmphdr->code), ntohs(icmphdr->id), ntohs(icmphdr->seq));
            }

            //copy received packet to send back
            memcpy(sendbuf, buf, 1500);

            //fill ICMP header
            icmphdrsend = ((struct icmpheader *) (sendbuf + sizeof(struct ethheader) + sizeof(struct ipheader)));
            icmphdrsend->type = 0;
            icmphdrsend->checksum = 0;
            icmphdrsend->checksum = in_chksum((char *) icmphdrsend,
                                              (1500 - sizeof(struct ethheader) - sizeof(struct ipheader)));

            //fill IP header
            iphdrsend = (struct ipheader *) (sendbuf + sizeof(struct ethheader));
            memcpy(iphdrsend->src_ip, iphdr->src_ip, 4);
            memcpy(iphdrsend->dst_ip, iphdr->dst_ip, 4);

            //fill ethernet header
            ethhdrsend = (struct ethheader *) sendbuf;
            memcpy(ethhdrsend->eth_dst, ethhdr->eth_dst, 6);
            memcpy(ethhdrsend->eth_src, ethhdr->eth_src, 6);

            //send ICMP respsonse packet
            sendto(packet_socket, sendbuf, 1500, 0, (struct sockaddr *) &recvaddr, &recvaddrlen);

        }

    }
    //exit
    return 0;
}
