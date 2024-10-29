# README for Assignment 2: Router

Name:

JHED:

---

**DESCRIBE YOUR CODE AND DESIGN DECISIONS HERE**

This will be worth 10% of the assignment grade.

Some guiding questions:
- What files did you modify (and why)?
- What helper method did you write (and why)?
- What logic did you implement in each file/method?
- What problems or challenges did you encounter?

# Router Implementation

The first thing I did was implement the ARP cache, so that my router would be able to find other hosts' MAC addresses. This was in `sr_arpcache.*`.

## ARP Cache Implementation (`sr_arpcache.*`)

### `handle_arpreq()`
- I followed the pseudocode for my control logic.
- ICMP error packets go from the router's interface to the original sender.
- ARP Requests go from the router's interface to broadcast `0xff`.

### Helper Function: `sr_lookup_route()`
- I wrote `sr_lookup_route()` to find routing table entry.

### `sr_arpcache_sweepreqs()`
- Followed the pseudocode in the header file.

## Router Logic (`sr_router.*`)

### Control Code
- First, I wrote the control code, as specified by the tutorial.

### `sr_handle_arprequest`
- This part is meant to say "I'm the router, I have that IP and here's my MAC address," so it sends back to the requesting device the router's information.

### `sr_handle_arpreply`
- This part is meant to say "Here are all the packets meant for you," so it loops through all the packets of the request and sends them off to the one that replied.

### Packet Handling
- If the packet type is not an ARP request, then check its checksum and size.
- If the packet is meant for the router, then call `sr_destined_for_router`.
  - Determine if it's an echo or not, and store that result in variable `echo`. The length of the ICMP packet will differ based on the result.
  - I followed the tutorial to know how and when to send an ICMP type 3 code 3, and type 0 code 0.
- Otherwise, it's for IP forwarding.

### IP Forwarding and Error Handling
- I prepare an error packet to send either IC
