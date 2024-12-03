#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <bpf/bpf_helpers.h>

#define THRESHOLD 250 // Max packets per second
#define TIME_WINDOW_NS 1000000000 // 1 second in nanoseconds

struct rate_limit_entry {
    __u64 last_update; // Timestamp of the last update
    __u32 packet_count; // Packet count within the time window
};

// Hash map to track rate limits for each source IP
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32); // Source IP
    __type(value, struct rate_limit_entry);
} rate_limit_map SEC(".maps");

SEC("xdp") int ddos_protection(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // Parse Ethernet header
    struct ethhdr *eth = data;
    // Check if packet is large enough to contain Ethernet header
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;
    
    // Check for IP packets
    if (eth->h_proto != __constant_htons(ETH_P_IP))
        return XDP_PASS;
    
    // Parse IP header
    struct iphdr *iph = (void *)(eth + 1);
    // Check if ethernet frame is large enough to contain IP header
    if ((void *)(iph + 1) > data_end)
        return XDP_PASS;
        
    // Convert source IP from network to host byte order
    __u32 src_ip = __builtin_bswap32(iph->saddr);
    
    // Lookup rate limit entry for this IP
    struct rate_limit_entry *entry = bpf_map_lookup_elem(&rate_limit_map, &src_ip);
    
    // Get current time in nanoseconds
    __u64 current_time = bpf_ktime_get_ns();
    
    if (entry) {
        // Check if we're in the same time window
        if (current_time - entry->last_update < TIME_WINDOW_NS) {
            entry->packet_count++;
            if (entry->packet_count > THRESHOLD) {
                return XDP_DROP; // Drop packet if rate exceeds threshold
            }
        } else {
            // New time window, reset counter
            entry->last_update = current_time;
            entry->packet_count = 1;
        }
    } else {
        // Initialize rate limit entry for new IP
        struct rate_limit_entry new_entry;
        // Zero out padding bytes
        __builtin_memset(&new_entry, 0, sizeof(new_entry));
        new_entry.last_update = current_time;
        new_entry.packet_count = 1;
        if (bpf_map_update_elem(&rate_limit_map, &src_ip, &new_entry, BPF_ANY) != 0) {
            return XDP_ABORTED; // Handle error if update fails
        }
    }
    return XDP_PASS; // Allow packet if under threshold   
}

char _license[] SEC("license") = "GPL";