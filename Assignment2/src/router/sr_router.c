/**********************************************************************
 * file:  sr_router.c
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* fill in code here */
  uint16_t packet_type = ethertype(packet);

  if(packet_type == ethertype_arp) {
      sr_arp_hdr_t *arp_pkt = (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
      uint16_t opcode = ntohs(arp_pkt->ar_op)
      if (opcode == arp_op_request) {// it's a request
        handle_arp_request(sr, arp_pkt, len, interface, packet);
      } else if (opcode == arp_op_reply) {
        sr_handle_arpreply(sr, arp_pkt, len, interface);
      } else {
        fprintf(stderr, "Received invalid ARP packet opcode\n");
      } else if (packet_type == ethertype_ip) {
        // grab it's IP header
        sr_ip_hdr_t *req_ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
        printf("IP Packet of length %d was received.\n", len);
        print_hdr_ip((uint8_t *) req_ip_hdr); 
        
        //verify checksum
        uint16_t ip_checksum = req_ip_hdr->ip_sum;
        req_ip_hdr->ip_sum = 0;
        req_ip_hdr->ip_sum = cksum(req_ip_hdr, sizeof(sr_ip_hdr_t));
        
        if (req_ip_hdr->ip_sum != ip_checksum) {
          fprintf(stderr, "Received an IP packet with invalid checksum\n");
          return;
        }
  
        // Check if IP packet meets min length
        if (len - sizeof(sr_ethernet_hdr_t) < sizeof(sr_ip_hdr_t)) {
          fprintf(stderr, "Received an IP packet that's too small!!!\n");
          return;
        }


        //check if ip packet meets minimum length
        //determine if ip packet is destined for router
        struct sr_if *router_if = get_interface_from_ip(sr, ip_hdr->ip_dst);
        if (router_if) {
          // sr_ip_hdr_t *req_ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t)); comment out since it was already done earlier above
          sr_icmp_hdr_t *req_icmp_hdr = (sr_icmp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
          struct sr_if *outgoing_iface= get_interface_from_ip(sr, req_ip_hdr->ip_dst);
          if (outgoing_iface) {
            if (req_ip_hdr->ip_p != ip_protocol_icmp) {
              unsigned int error_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t);
              handle_icmp_error_reply(sr, error_len, interface, outgoing_iface, req_ip_hdr);

            }
            if (req_icmp_hdr->icmp_type == 8) {
              handle_icmp_echo_reply(sr, len, interface, outgoing_iface,  req_ip_hdr, req_icmp_hdr)
            } 
          } else {
            // Debugging Print Statements
            fprintf(stdout, "Handling IP (non-interface destined) packet:\n");
            print_hdrs(packet, len); 
            fprintf(stdout, "\n");
                 
            sr_handle_ip_forwarding(sr, packet, len, interface);
          }

      }
  }

} /* end sr_handlepacket */


/* Add any additional helper methods here & don't forget to also declare
them in sr_router.h.

If you use any of these methods in sr_arpcache.c, you must also forward declare
them in sr_arpcache.h to avoid circular dependencies. Since sr_router
already imports sr_arpcache.h, sr_arpcache cannot import sr_router.h -KM */

void sr_handle_arprequest(struct sr_instance* sr, sr_arp_hdr_t *arp_pkt, unsigned int len,
        char* interface, uint8_t * packet) {

  // answers the question "Who has IP of router, send me your mac address" with "I'm the router, I have that IP, here is my mac address"
          

  // check if target IP address is the router's IP address:
  struct sr_if *matching_iface = get_interface_from_ip(sr, arp_pkt->ar_tip)
  
  if(matching_iface == NULL) { // router can have multiple interfaces for different networks, so search through them
    return;
  }

  uint8_t *arp_reply = (uint8_t *) malloc(len) // same length as original arp packet


  // Ethernet Headers
  struct sr_ethernet_hdr *ethernet_hdr = (struct sr_ethernet_hdr *)arp_reply; 
  memcpy(ethernet_hdr->ether_dhost, ((sr_ethernet_hdr_t *) packet)->ether_shost, ETHER_ADDR_LEN); // broadcast
  memcpy(ethernet_hdr->ether_shost = matching_iface->addr, ETHER_ADDR_LEN); // mac address that matches the interface for router
  ethernet_hdr->ether_type = htons(ETHERTYPE_ARP); 

  // ARP headers
  /*unsigned short ar_hrd; (set to arp_hrd_ethernet)
    unsigned short ar_pro;  (set to ethertype_ip)
    unsigned char ar_hln; (set to length of a mac address)
    unsigned char ar_pln;  (set to length of an IP address)
    unsigned short ar_op; (set to ar_op_reply)
    unsigned char ar_sha[ETHER_ADDR_LEN];  (set to receiving interface’s addr)
    uint32_t ar_sip; (set to receiving interface’s ip)
    unsigned char ar_tha[ETHER_ADDR_LEN]; (set to original packet’s ether_shost)
    uint32_t ar_tip; (set to original packet’s sip)
  */
  struct sr_arp_hdr *arp_hdr = (struct sr_arp_hdr *)(arp_reply + sizeof(struct sr_ethernet_hdr));
  arp_hdr->ar_hrd = htons(arp_hrd_ethernet);
  arp_hdr->ar_pro = htons(ethertype_ip);
  arp_hdr->ar_hln = ETHER_ADDR_LEN;
  arp_hdr->ar_pln = sizeof(uint32_t);
  arp_hdr->ar_op = htons(arp_op_reply);
  memcpy(arp_hdr->ar_sha, matching_iface->addr, ETHER_ADDR_LEN);
  arp_hdr->ar_sip = matching_iface->ip;
  memcpy(arp_hdr->ar_tha, arp_pkt->ar_sha, ETHER_ADDR_LEN);
  arp_hdr->ar_tip = arp_pkt->ar_sip;


  if (sr_send_packet(sr, arp_reply, len, interface) == -1) {
    fprintf(stderr, "Failed to send ARP reply\n");
  }
  free(arp_reply);  
}

/*
  It checks if the IP address in the received ARP reply is already present in the request queue. If found, it removes this request from the queue (to avoid redundant requests) and returns it.
  It then updates the ARP cache with the new IP-to-MAC mapping, marking it as valid.
*/
void sr_handle_arpreply(struct sr_instance* sr, sr_arp_hdr_t *arp_pkt, unsigned int len, 
        char* interface) {
  // responds to answer of "Hey router, I have that IP, and this is my MAC address" with "Okay, here are all the packets that were meant for you"        
  // In arpcache.c, I actually implement the part where the router asks, "Hey I'm the router, and I'm looking for mac address that corresponds to <ARP request's IP>"

  // check if the ARP reply is for us
  struct sr_if *matching_iface = get_interface_from_ip(sr, arp_pkt->ar_tip)
  if(matching_iface == NULL) { // the ARP reply is not for us
    return;
  }

  // Check if IP address is already in the request queue
  struct sr_arpreq *request = sr_arpcache_insert(&sr->cache, arp_pkt->ar_sha, arp_pkt->ar_sip);
  if (request) {
    // If found, send off all packets on the request's pending packets list, then remove the request from the queue

    struct sr_packet *cur_pkt = request->packets;
    while (cur_pkt) {
      // send the packet

      // arp_pkt contains the destination mac address, since it's the arp reply
      sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t *)cur_pkt->buf;
      memcpy(ethernet_hdr->ether_dhost, arp_pkt->ar_sha, ETHER_ADDR_LEN); // destination is the mac address from the arp reply
      memcpy(ethernet_hdr->ether_shost, matching_iface->addr, ETHER_ADDR_LEN); // source is the router's interface's mac address

      if(sr_send_packet(sr, cur_pkt->buf, cur_pkt->len, cur_pkt->iface) == -1) {
        fprintf(stderr, "ARP Reply: Failed to send queued packet\n");
      }
      cur_pkt = cur_pkt->next;
    }
    sr_arpreq_destroy(&sr->cache, request);
  }

  // Insert the MAC address & corresponing IP into the ARP cache



}

void handle_icmp_echo_reply(struct sr_instance *sr, uint8_t *packet, unsigned int reply_len, char *incoming_iface_name, 
                            struct sr_if *outgoing_iface, sr_ip_hdr_t *req_ip_hdr, sr_icmp_hdr_t *req_icmp_hdr) {


    // Create reply packet
    uint8_t *icmp_reply = create_icmp_reply_packet(sr, packet, reply_len, incoming_iface_name, outgoing_iface, req_ip_hdr);
    sr_ip_hdr_t *ip_hdr = (sr_ip_hdr_t *)(icmp_reply + sizeof(sr_ethernet_hdr_t)); // need to grab IP to make small adjustments

    // Fill ICMP echo reply fields
    sr_icmp_hdr_t *icmp_hdr = (sr_icmp_hdr_t *)(icmp_reply + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    size_t icmp_hdr_len = reply_len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t);
    memcpy(icmp_hdr, req_icmp_hdr, icmp_hdr_len); // copy ICMP header, since it's an echo
    icmp_hdr->icmp_type = 0; // Echo reply
    icmp_hdr->icmp_code = 0;
    icmp_hdr->icmp_sum = 0;

    icmp_hdr->icmp_sum = cksum(icmp_hdr, sizeof(sr_icmp_hdr_t));
    ip_hdr->ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t)); 

    if (sr_send_packet(sr, icmp_reply, reply_len, incoming_iface_name) == -1) {
        fprintf(stderr, "Failed to send ICMP Echo Reply\n");
    }
    free(icmp_reply);
}

void handle_icmp_error_reply(struct sr_instance *sr, uint8_t *packet, unsigned int error_len, char *incoming_iface_name,
                              struct sr_if *outgoing_iface, sr_ip_hdr_t *req_ip_hdr) {
    // Define the length for ICMP error (type 3 code 3) packet
    uint8_t *icmp_error_reply = create_icmp_reply_packet(sr, packet, error_len, incoming_iface_name, outgoing_iface, req_ip_hdr);
    sr_ip_hdr_t *ip_hdr = (sr_ip_hdr_t *)(icmp_reply + sizeof(sr_ethernet_hdr_t)); // need to grab IP to make small adjustments


    ip_hdr->ip_off = htons(IP_DF);

    // Fill ICMP error fields
    sr_icmp_t3_hdr_t *icmp_hdr = (sr_icmp_hdr_t *)(icmp_error_reply + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    icmp_hdr->icmp_type = 3; // Destination Unreachable
    icmp_hdr->icmp_code = 3; // Port unreachable
    icmp_hdr->icmp_sum = 0;
    memcpy(icmp_hdr->data, (uint8_t *) req_ip_hdr, ICMP_DATA_SIZE);

    icmp_hdr->icmp_sum = cksum(icmp_hdr, sizeof(sr_icmp_t3_hdr_t));
    ip_hdr->ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t));

    if (sr_send_packet(sr, icmp_error_reply, error_len, incoming_iface_name) == -1) {
        fprintf(stderr, "Failed to send ICMP Error Reply\n");
    }
    free(icmp_error_reply);
}

uint8_t *create_icmp_reply_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *incoming_iface_name, 
                                  struct sr_if *outgoing_iface, sr_ip_hdr_t *req_ip_hdr) {
    // Allocate memory for the reply packet
    uint8_t *icmp_reply = (uint8_t *)malloc(len);

    // Fill Ethernet header
    sr_ethernet_hdr_t *req_ether_hdr = (sr_ethernet_hdr_t *)packet;
    sr_ethernet_hdr_t *ether_hdr = (sr_ethernet_hdr_t *)icmp_reply;
    memcpy(ether_hdr->ether_dhost, req_ether_hdr->ether_shost, ETHER_ADDR_LEN);
    struct sr_if *incoming_iface = sr_get_interface(sr, incoming_iface_name);
    memcpy(ether_hdr->ether_shost, incoming_iface->addr, ETHER_ADDR_LEN);
    ether_hdr->ether_type = htons(ethertype_ip);

    // Fill IP header
    sr_ip_hdr_t *ip_hdr = (sr_ip_hdr_t *)(icmp_reply + sizeof(sr_ethernet_hdr_t));
    ip_hdr->ip_hl = req_ip_hdr->ip_hl;
    ip_hdr->ip_v = req_ip_hdr->ip_v;
    // ip_hdr->ip_tos = req_ip_hdr->ip_tos;
    ip_hdr->ip_len = htons(len - sizeof(sr_ethernet_hdr_t)); // huh how does this make sense
    ip_hdr->ip_id = req_ip_hdr->ip_id;
    ip_hdr->ip_ttl = INIT_TTL;
    ip_hdr->ip_p = ip_protocol_icmp;
    ip_hdr->ip_src = outgoing_iface->ip;
    ip_hdr->ip_dst = req_ip_hdr->ip_src;
    ip_hdr->ip_sum = 0;
    ip_hdr->ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t)); // huh

   

    return icmp_reply;
}


void sr_handle_ip_forwarding(struct sr_instance* sr, uint8_t *forward_pkt, unsigned int len, char* interface) {
  // handle IP forwarding logic here
  // decrement TTL by 1
  // grab IP header first
  sr_ip_hdr_t *forward_ip_hdr = (sr_ip_hdr_t *)(forward_pkt + sizeof(sr_ethernet_hdr_t));
  forward_ip_hdr->ip_ttl--;
  forward_ip_hdr->ip_sum = 0;
  forward_ip_hdr->ip_sum = cksum(forward_ip_hdr, sizeof(sr_ip_hdr_t));

  // prepare error messages ICMP type 11 and 3, all error messages will be sent directly back to sender, so send them out the same incoming interface
  size_t error_pkt_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t);
  uint8_t *error_pkt = (uint8_t *) malloc(error_pkt_len);
  // create packet
  struct sr_if *incoming_if = sr_get_interface(sr, interface);

  uint8_t *err_pkt = create_ip_forwarding_error_packet()

  // check TTL  expiration
  if (forward_ip_hdr->ip_ttl < 1) {
    printf("TTL expired. Sending ICMP Time Exceeded.\n");
    // SEND ICMP TIME EXCEEDED PACKET
  } else {
    printf("TTL is %d. Packet can be forwarded.\n", ip_hdr->ip_ttl);
  }

  // Find match for destination IP in routing table...
  // next_hop_mac_address = (matching function result)

  struct sr_rt *rt = sr_lookup_route(sr->routing_table, cur_ip_hdr->ip_src);
  if (rt == NULL) {
    //send ICMP destination net unreachable
  }
  struct sr_if *outgoing_if = sr_get_interface(sr, rt->interface); // rt tells you you need to send 192.168.1.10 to interface eth0, where it's 192.168.1.0

  struct sr_arpentry *entry = sr_arpcache_lookup(&sr->cache, forward_ip_hdr->ip_dst); // cache tells you 192.168.1.1 has mac address AAA...
  if (entry) {
    // send to next hop, just redo the layer 2 header of forward_ip_pkt, keep all else
    forward_packet(sr, len, outgoing_if, forward_ip_pkt, entry);
  } else {
    // No ARP entry was found, prepare packet to be placed into queue
    memset(((sr_ethernet_hdr_t *) forward_ip_pkt)->ether_dhost, 0, ETHER_ADDR_LEN);

    // Place packet into the cache's queue, send ARP request
    struct sr_arpreq *arp_req = sr_arpcache_queuereq(&sr->cache, forward_ip_pkt->ip_dst, forward_ip_pkt, len, outgoing_if->name);
    handle_arpreq(sr, arp_req);
  } 
}


create_ip_forwarding_error_packet(uint8_t * error_pkt, uint8_t * forward_ip_pkt, sr_if *incoming_if) {
  // make ethernet headers
  sr_ethernet_hdr_t *eth_hdr = (sr_ethernet_hdr_t *) error_pkt;
  memcpy(eth_hdr->ether_dhost, ((sr_ethernet_hdr_t *) forward_ip_pkt)->ether_shost, ETHER_ADDR_LEN);
  memcpy(eth_hdr->ether_shost, incoming_if->addr, ETHER_ADDR_LEN);
  eth_hdr->ether_type = htons(ethertype_ip);

  // make IP headers
  
}


void forward_packet(struct sr_instance* sr, unsigned int len, struct sr_if outgoing_if, uint8_t *forward_ip_pkt, struct sr_arpentry entry) {
    memcpy(((sr_ethernet_hdr_t *)forward_ip_pkt)->ether_dhost, entry->mac, ETHER_ADDR_LEN);
    memcpy(((sr_ethernet_hdr_t *)forward_ip_pkt)->ether_shost, outgoing_if->addr, ETHER_ADDR_LEN);

    if (sr_send_packet(sr, forward_ip_pkt, len, outgoing_if->name) == -1){
      fprintf(stderr, "Failed to send Forwarding IP Packet\n");
    }
    free(entry);
}