#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "sr_arpcache.h"
#include "sr_router.h"
#include "sr_if.h"
#include "sr_protocol.h"

/*
  This function gets called every second. For each request sent out, we keep
  checking whether we should resend an request or destroy the arp request.
  See the comments in the header file for an idea of what it should look like.
*/
void sr_arpcache_sweepreqs(struct sr_instance *sr) {
    struct sr_arpreq *req = sr->cache.requests;
    while (req) {
        struct sr_arpreq *temp = req;
        req = req->next;
        handle_arpreq(sr, temp); // handle arpreq destroys temp, so it's good that we saved the next pointer
    }
}

struct sr_rt *sr_lookup_route(struct sr_rt *cur_rt, uint32_t ip) {
    // search through routing table to find the mac address of the destination ip
    while (cur_rt) {
        if (cur_rt->dest.s_addr == ip) {
            return &(cur_rt->rt);
        }
        cur_rt = cur_rt->next;
    }
    return NULL;
}

// LECTURE 9 talks about spanning tree
void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *request) {
    
    if (difftime(time(NULL), request->sent) <= 1.0) {
        return;
    }

    if (request->times_sent >= 5) {
        // send icmp host unreachable
        // Figure out the length of your new icmp packet (hint: it will be the same length as the original packet if the original packet was an ICMP echo request. Otherwise, calculate it based on the size of the headers it will have).
        // Create a new icmp packet (hint: use malloc)

        struct sr_packet *cur_pkt = request->packets;
        while (cur_pkt) { 
            // allocate memory for the icmp packet which consists of icmp header, ip header, and ethernet header
            size_t icmp_packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t);
            uint8_t *icmp_packet = (uint8_t *)malloc(icmp_packet_len);

            // get the original ip header of the packet, because we want to match its IP with its mac address using the routing table, and get other info from it
            sr_ip_hdr_t *cur_ip_hdr = (sr_ip_hdr_t *)(cur_pkt->buf + sizeof(sr_ethernet_hdr_t));
            /*
            ip_hdr->ip_src = return_if->ip; // 192.168.1.1
            ip_hdr->ip_dst = cur_ip_hdr->ip_src; // 192.168.1.10
            */

            // uint8_t icmp_type; (set to the type # you are sending)
            // uint8_t icmp_code; (set to the code # you are sending)
            // uint16_t icmp_sum; (initially set to 0, compute cksum over the icmp header)
            // uint8_t data[ICMP_DATA_SIZE]; (don’t set if it’s an echo reply, otherwise original packet ip header)

            // populate icmp header
            struct sr_icmp_hdr_t *icmp_hdr = (struct sr_icmp_hdr_t *)(icmp_packet + sizeof(sr_ethernet_hdr_t) + sizeof(struct sr_ip_hdr_t));
            icmp_hdr->icmp_type = 3; // 3 is destination unreachable
            icmp_hdr->icmp_code = 1; // 1 is host unreachable
            icmp_hdr->icmp_sum = 0; // checksum is 0`
            memcpy(icmp_hdr->data, cur_ip_hdr, ICMP_DATA_SIZE);

            // populate IP headers
            // uint8_t ip_tos; (just set to 0, as per reference binary) Don’t need to do this since you’re memsetting the IP header with the original IP header anyway.
            // uint16_t ip_len; (set to packet length minus ethernet header size)
            // uint16_t ip_off; (just set to IP_DF, as per the reference binary) UPDATE: Don’t do this for echo replies.
            // uint8_t ip_ttl; (set to something reasonable, e.g. TTL_INIT = 255)
            // uint8_t ip_p; (set to ip_protocol_icmp)
            // uint16_t ip_sum; (initially set to 0, compute cksum over the ip header)
            // uint32_t ip_src, (set to ip of our interface or outgoing interface based on whether the original packet was destined for us or not)
            // unint32_t ip_dst; (set to original packet’s ip_src )

            sr_ip_hdr_t *ip_hdr = (sr_ip_hdr_t *)(icmp_packet + sizeof(sr_ethernet_hdr_t));
            // ip_hdr->ip_tos = 0;
            ip_hdr->ip_len = htons(icmp_packet_len - sizeof(sr_ethernet_hdr_t));
            ip_hdr->ip_off = htons(IP_DF);
            ip_hdr->ip_ttl = TTL_INIT;
            ip_hdr->ip_p = IP_PROTOCOL_ICMP;
            ip_hdr->ip_sum = 0;
            ip_hdr->ip_src = return_iface->ip;
            ip_hdr->ip_dst = cur_ip_hdr->ip_src;


            // Populate Ethernet header
            // search through routing table to find the correct interface
            struct sr_rt *rt = sr_lookup_route(sr->routing_table, cur_ip_hdr->ip_src);
            struct sr_if *return_iface = sr_get_interface(sr, rt->iface); // match 192.168.1.10 with the interface 192.168.1.0/24, for example
            
            
            sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t *)icmp_packet;
            memcpy(ethernet_hdr->ether_shost, return_iface->addr, ETHER_ADDR_LEN); // source is the router's interface's mac address
            memcpy(ethernet_hdr->ether_dhost, ((sr_ethernet_hdr_t *)(cur_pkt->buf))->ether_shost, ETHER_ADDR_LEN); // destination is back to the original source's mac address
            ethernet_hdr->ether_type = htons(ETHERTYPE_IP);

            // recompute checksum
            icmp_hdr->icmp_sum = cksum(icmp_hdr, sizeof(sr_icmp_hdr_t));
            ip_hdr->ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t));

            if (sr_send_packet(sr, icmp_packet, icmp_packet_len, return_iface->name) == -1) {
                fprintf(stderr, "Failed to send packet\n");
            }
            free(icmp_packet);
            cur_pkt = cur_pkt->next; // go take care of the next packet
            

        }
        sr_arpreq_destroy(&sr->cache, request);
    } else {
        // send arp request
        struct sr_if *iface = sr_if_get_interface(sr, request->iface);
        if (!iface) {
            fprintf(stderr, "Interface not found\n");
            return;
        }
        
        /* Arp Request Packet consists of Ethernet Header and Arp Header*/
        size_t arp_packet_len = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arp_hdr);
        uint8_t *arp_packet = (uint8_t *)malloc(arp_packet_len);

        /* Fill in the Ethernet Header */
        struct sr_ethernet_hdr *ethernet_hdr = (struct sr_ethernet_hdr *)arp_packet; 
        memset(ethernet_hdr->ether_dhost, 0xff, ETHER_ADDR_LEN); // broadcast
        ethernet_hdr->ether_shost = iface->ether_addr; // source is the interface's mac address USE MEMCPY??
        ethernet_hdr->ether_type = htons(ETHERTYPE_ARP); // USE MEMSET???

        /* Fill in the Arp Header */
        /*  unsigned short ar_hrd; (set to arp_hrd_ethernet)
            unsigned short ar_pro;  (set to ethertype_ip)
            unsigned char ar_hln; (set to length of a mac address)
            unsigned char ar_pln;  (set to length of an IP address)
            unsigned short ar_op; (set to ar_op_request)
            unsigned char ar_sha[ETHER_ADDR_LEN];  (set to the current arpreq’s first packet’s interface’s addr)
            uint32_t ar_sip; (set to current arpreq’s first packet’s interface’s ip)
            unsigned char ar_tha[ETHER_ADDR_LEN];  (set to 0xff, since broadcasted)
            uint32_t ar_tip; (set to current arpreq’s ip)
        */

        struct sr_arp_hdr *arp_hdr = (struct sr_arp_hdr *)(arp_packet + sizeof(struct sr_ethernet_hdr));
        arp_hdr->ar_hrd = htons(1);
        arp_hdr->ar_pro = htons(ETHERTYPE_IP);
        arp_hdr->ar_hln = ETHER_ADDR_LEN;
        arp_hdr->ar_pln = 4;
        arp_hdr->ar_op = htons(1); // op code 1 is request, op code 2 is reply
        memcpy(arp_hdr->ar_sha, iface->ether_addr, ETHER_ADDR_LEN);
        arp_hdr->ar_sip = iface->ip; // source ip is the interface's ip
        memset(arp_hdr->ar_tha, 0xff, ETHER_ADDR_LEN); // broadcast
        arp_hdr->ar_tip = request->ip; // destination ip is the arpreq's ip
        
        /* Send the packet */
        sr_send_packet(sr, arp_packet, arp_packet_len, iface->name);
        free(arp_packet);

        request->sent = time(NULL);
        request->times_sent++;
        }
    }


/* You should not need to touch the rest of this code. */

/* Checks if an IP->MAC mapping is in the cache. IP is in network byte order.
   You must free the returned structure if it is not NULL. */
struct sr_arpentry *sr_arpcache_lookup(struct sr_arpcache *cache, uint32_t ip) {
    pthread_mutex_lock(&(cache->lock));

    struct sr_arpentry *entry = NULL, *copy = NULL;

    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
        if ((cache->entries[i].valid) && (cache->entries[i].ip == ip)) {
            entry = &(cache->entries[i]);
        }
    }

    /* Must return a copy b/c another thread could jump in and modify
       table after we return. */
    if (entry) {
        copy = (struct sr_arpentry *) malloc(sizeof(struct sr_arpentry));
        memcpy(copy, entry, sizeof(struct sr_arpentry));
    }

    pthread_mutex_unlock(&(cache->lock));

    return copy;
}

/* Adds an ARP request to the ARP request queue. If the request is already on
   the queue, adds the packet to the linked list of packets for this sr_arpreq
   that corresponds to this ARP request. You should free the passed *packet.

   A pointer to the ARP request is returned; it should not be freed. The caller
   can remove the ARP request from the queue by calling sr_arpreq_destroy. */
struct sr_arpreq *sr_arpcache_queuereq(struct sr_arpcache *cache,
                                       uint32_t ip,
                                       uint8_t *packet,           /* borrowed */
                                       unsigned int packet_len,
                                       char *iface)
{
    pthread_mutex_lock(&(cache->lock));

    struct sr_arpreq *req;
    for (req = cache->requests; req != NULL; req = req->next) {
        if (req->ip == ip) {
            break;
        }
    }

    /* If the IP wasn't found, add it */
    if (!req) {
        req = (struct sr_arpreq *) calloc(1, sizeof(struct sr_arpreq));
        req->ip = ip;
        req->next = cache->requests;
        cache->requests = req;
    }

    /* Add the packet to the list of packets for this request */
    if (packet && packet_len && iface) {
        struct sr_packet *new_pkt = (struct sr_packet *)malloc(sizeof(struct sr_packet));

        new_pkt->buf = (uint8_t *)malloc(packet_len);
        memcpy(new_pkt->buf, packet, packet_len);
        new_pkt->len = packet_len;
		new_pkt->iface = (char *)malloc(sr_IFACE_NAMELEN);
        strncpy(new_pkt->iface, iface, sr_IFACE_NAMELEN);
        new_pkt->next = req->packets;
        req->packets = new_pkt;
    }

    pthread_mutex_unlock(&(cache->lock));

    return req;
}

/* This method performs two functions:
   1) Looks up this IP in the request queue. If it is found, returns a pointer
      to the sr_arpreq with this IP. Otherwise, returns NULL.
   2) Inserts this IP to MAC mapping in the cache, and marks it valid. */
struct sr_arpreq *sr_arpcache_insert(struct sr_arpcache *cache,
                                     unsigned char *mac,
                                     uint32_t ip)
{
    pthread_mutex_lock(&(cache->lock));

    struct sr_arpreq *req, *prev = NULL, *next = NULL;
    for (req = cache->requests; req != NULL; req = req->next) {
        if (req->ip == ip) {
            if (prev) {
                next = req->next;
                prev->next = next;
            }
            else {
                next = req->next;
                cache->requests = next;
            }

            break;
        }
        prev = req;
    }

    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
        if (!(cache->entries[i].valid))
            break;
    }

    if (i != SR_ARPCACHE_SZ) {
        memcpy(cache->entries[i].mac, mac, 6);
        cache->entries[i].ip = ip;
        cache->entries[i].added = time(NULL);
        cache->entries[i].valid = 1;
    }

    pthread_mutex_unlock(&(cache->lock));

    return req;
}

/* Frees all memory associated with this arp request entry. If this arp request
   entry is on the arp request queue, it is removed from the queue. */
void sr_arpreq_destroy(struct sr_arpcache *cache, struct sr_arpreq *entry) {
    pthread_mutex_lock(&(cache->lock));

    if (entry) {
        struct sr_arpreq *req, *prev = NULL, *next = NULL;
        for (req = cache->requests; req != NULL; req = req->next) {
            if (req == entry) {
                if (prev) {
                    next = req->next;
                    prev->next = next;
                }
                else {
                    next = req->next;
                    cache->requests = next;
                }

                break;
            }
            prev = req;
        }

        struct sr_packet *pkt, *nxt;

        for (pkt = entry->packets; pkt; pkt = nxt) {
            nxt = pkt->next;
            if (pkt->buf)
                free(pkt->buf);
            if (pkt->iface)
                free(pkt->iface);
            free(pkt);
        }

        free(entry);
    }

    pthread_mutex_unlock(&(cache->lock));
}

/* Prints out the ARP table. */
void sr_arpcache_dump(struct sr_arpcache *cache) {
    fprintf(stderr, "\nMAC            IP         ADDED                      VALID\n");
    fprintf(stderr, "-----------------------------------------------------------\n");

    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
        struct sr_arpentry *cur = &(cache->entries[i]);
        unsigned char *mac = cur->mac;
        fprintf(stderr, "%.1x%.1x%.1x%.1x%.1x%.1x   %.8x   %.24s   %d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ntohl(cur->ip), ctime(&(cur->added)), cur->valid);
    }

    fprintf(stderr, "\n");
}

/* Initialize table + table lock. Returns 0 on success. */
int sr_arpcache_init(struct sr_arpcache *cache) {
    /* Seed RNG to kick out a random entry if all entries full. */
    srand(time(NULL));

    /* Invalidate all entries */
    memset(cache->entries, 0, sizeof(cache->entries));
    cache->requests = NULL;

    /* Acquire mutex lock */
    pthread_mutexattr_init(&(cache->attr));
    pthread_mutexattr_settype(&(cache->attr), PTHREAD_MUTEX_RECURSIVE);
    int success = pthread_mutex_init(&(cache->lock), &(cache->attr));

    return success;
}

/* Destroys table + table lock. Returns 0 on success. */
int sr_arpcache_destroy(struct sr_arpcache *cache) {
    return pthread_mutex_destroy(&(cache->lock)) && pthread_mutexattr_destroy(&(cache->attr));
}

/* Thread which sweeps through the cache and invalidates entries that were added
   more than SR_ARPCACHE_TO seconds ago. */
void *sr_arpcache_timeout(void *sr_ptr) {
    struct sr_instance *sr = sr_ptr;
    struct sr_arpcache *cache = &(sr->cache);

    while (1) {
        sleep(1.0);

        pthread_mutex_lock(&(cache->lock));

        time_t curtime = time(NULL);

        int i;
        for (i = 0; i < SR_ARPCACHE_SZ; i++) {
            if ((cache->entries[i].valid) && (difftime(curtime,cache->entries[i].added) > SR_ARPCACHE_TO)) {
                cache->entries[i].valid = 0;
            }
        }

        sr_arpcache_sweepreqs(sr);

        pthread_mutex_unlock(&(cache->lock));
    }

    return NULL;
}
