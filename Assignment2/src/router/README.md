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

This project implements a simple router with ARP caching and packet forwarding functionality. The main files for this implementation are `sr_arpcache.*` and `sr_router.*`. The router manages ARP requests and replies, handles packet forwarding, and generates ICMP error messages when needed.

## Components

### 1. ARP Cache Implementation (`sr_arpcache.*`)

#### `handle_arpreq()`
- Implements control logic based on provided pseudocode.
- If an ARP request times out after 5 attempts, sends an ICMP error packet from the router's interface to the original sender.
- If the ARP request needs to be broadcasted, it sends an ARP request to broadcast address `0xff`.

#### `sr_lookup_route()`
- Helper function to find the routing table entry for a given destination IP address.

#### `sr_arpcache_sweepreqs()`
- Periodically sweeps through ARP requests and handles them based on the pseudocode provided in the header file.

### 2. Router Logic (`sr_router.*`)

#### Control Code
- The main routing control code follows the steps specified in the tutorial.

#### `sr_handle_arprequest()`
- Handles incoming ARP requests addressed to the router.
- Sends a reply to the requesting device with the router's MAC address if the router is the intended recipient.

#### `sr_handle_arpreply()`
- Handles ARP replies by sending queued packets to the replying device.
- Iterates over all packets in the queue meant for the reply’s sender and forwards them.

#### Packet Handling Logic
- If the incoming packet is not an ARP request, verifies its checksum and size.
- If the packet is addressed to the router, calls `sr_destined_for_router()`, where it determines if the packet is an ICMP echo request.
- Depending on whether the packet is an echo request, prepares to send an ICMP type 3 code 3 (destination unreachable) or ICMP type 0 code 0 (echo reply).

#### IP Forwarding and Error Handling
- For packets meant for IP forwarding, prepares to send ICMP type 3 or type 11 error packets using helper functions `icmp_3_error()` and `icmp_11_error()`.
- Checks if an ARP entry exists for the next-hop IP and MAC address:
  - If it exists, forwards the packet using `forward_packet()`.
  - If it does not exist, calls `handle_arpreq()`, which either broadcasts an ARP request to `0xff` or queues the request to retry up to 5 times before timing out.

### Summary

This router handles basic packet forwarding, ARP caching, and ICMP error generation according to standard networking protocols. It processes ARP requests and replies efficiently, maintains an ARP cache, and sends ICMP error packets as necessary.

