#include "include/ip.h"
#include "include/icmp.h"
#include "include/packet.h"
#include "include/arpcache.h"
#include "include/rtable.h"
#include "include/arp.h"

#include "include/log.h"

#include <stdlib.h>

// initialize ip header 
void ip_init_hdr(struct iphdr *ip, u32 saddr, u32 daddr, u16 len, u8 proto)
{
	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = htons(len);
	ip->id = rand();
	ip->frag_off = htons(IP_DF);
	ip->ttl = DEFAULT_TTL;
	ip->protocol = proto;
	ip->saddr = htonl(saddr);
	ip->daddr = htonl(daddr);
	ip->checksum = ip_checksum(ip);
}

// lookup in the routing table, to find the entry with the same and longest prefix.
// the input address is in host byte order
rt_entry_t *longest_prefix_match(u32 dst)
{
	/*fprintf(stderr, "TODO: longest prefix match for the packet.\n");*/
	rt_entry_t *pos, *maxpos = NULL;
	u32 maxlen = 0;
	list_for_each_entry(pos,  &rtable, list){
		u32 pos_ip = pos->dest & pos->mask;
		u32 ip = dst & pos->mask;
		if ( pos_ip == ip && pos->mask > maxlen ){
			/*printf (" %d, %d\n", ip, pos_ip);*/
			maxlen = pos->mask;
			maxpos = pos;
		}
	}
	return maxpos;
	/*return NULL;*/
}

// forward the IP packet from the interface specified by longest_prefix_match, 
// when forwarding the packet, you should check the TTL, update the checksum,
// determine the next hop to forward the packet, then send the packet by 
// iface_send_packet_by_arp
void ip_forward_packet(u32 ip_dst, char *packet, int len)
{
	/*fprintf(stderr, "TODO: forward ip packet.\n");*/
	struct iphdr *iphr = packet_to_ip_hdr(packet);
	--iphr->ttl;
	if (iphr->ttl <= 0){
		/*icmp->type = 11;*/
		/*icmp->code = 0;*/
		icmp_send_packet(packet, len, 11, 0);
		return;
	}
	/*iphr->saddr = iphr->daddr;*/
	rt_entry_t *dst = longest_prefix_match(ip_dst);
	/*iphr->daddr = dst->dest;*/
	iphr->checksum = ip_checksum(iphr);
	if (dst){
		/*printf(" Find ip \n");*/
		u32 next_hop = dst->gw;
		if (!next_hop){
			next_hop = ip_dst;
		}
		iface_send_packet_by_arp(dst->iface, next_hop, packet, len);
	}
	else {
		icmp_send_packet(packet, len, 3, 0);
	}
}

// handle ip packet
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	/*fprintf(stderr, "TODO: handle ip packet: echo the ping packet, and forward other IP packets.\n");*/
	struct ether_header *eh = (struct ether_header *)packet;
	struct iphdr *ip = packet_to_ip_hdr(packet);
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	if(ip->protocol == 1){
		struct icmphdr * icmp = (struct icmphdr *)((char *)ip + IP_HDR_SIZE(ip));
		if(icmp->type == 8 && iface->ip == ntohl(ip->daddr)){
			icmp_send_packet(packet, len, 8, 0);
		}
		else
		  ip_forward_packet(ntohl(ip->daddr), packet, len);
	}
	else{
		ip_forward_packet(ntohl(ip->daddr), packet, len);
	}
}

// send IP packet
//
// Different from ip_forward_packet, ip_send_packet sends packet generated by
// router itself. This function is used to send ICMP packets.
void ip_send_packet(char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 dst = ntohl(ip->daddr);
	rt_entry_t *entry = longest_prefix_match(dst);
	if (!entry) {
		log(ERROR, "Could not find forwarding rule for IP (dst:"IP_FMT") packet.", 
					HOST_IP_FMT_STR(dst));
		free(packet);
		return ;
	}

	u32 next_hop = entry->gw;
	if (!next_hop)
	  next_hop = dst;

	struct ether_header *eh = (struct ether_header *)packet;
	eh->ether_type = ntohs(ETH_P_IP);
	memcpy(eh->ether_shost, entry->iface->mac, ETH_ALEN);

	iface_send_packet_by_arp(entry->iface, next_hop, packet, len);
}
