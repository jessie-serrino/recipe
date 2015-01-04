
/*=============================
  
    
    UDP Messenger 
      (based upon a simple flooding strategy)
    Author: Jessie Serrino
  
  =============================*/ 

#include "help.h" 

#define TRUE  1
#define FALSE 0

// Defined by me 

#define MAXHISTORY 100

typedef struct 
{
    int port;
    char * dotted;
    struct sockaddr_in saddr;
} registry;


// THESE VARIABLES MUST BE GLOBAL in order to be visible in shutdown handler

/* local messenger data */ 
int messenger_sockfd;			/* descriptor for socket endpoint */ 
struct sockaddr_in messenger_sockaddr;	/* address/port pair */ 
struct in_addr messenger_addr; 		/* network-format address */ 
char messenger_dotted[INET_ADDRSTRLEN];	/* human-readable address */ 
int messenger_port; 			/* local port */ 

/* list of targets to which to propogate information */ 
int ntargets = 0; 
struct sockaddr_in targets[MAXTARGETS]; // message targets for flooding

registry targets_[MAXTARGETS];
int ntargets_ = 0;


char * history[MAXHISTORY];
int nmessages = 0;
int replace_message = 0;

int child_start=0;

int message_found(char * mes)
{
    int found = -1;
    int i;
    for(i = 0; i < nmessages; i++)
        if(strcmp(history[i], mes)) found = i;
    return found;
}

void store_message(char * mes)
{
   if(message_found(mes) == -1) // If the message is not found.
   {
        history[replace_message] = mes;
        replace_message++;
        if(nmessages >= MAXHISTORY)
        {
            if(replace_message >= MAXHISTORY)
                replace_message = 0;
        }
        else
            nmessages++;
   }
}

char * port_dotted(char * dotted, int port)
{
    char port_str[11];
    sprintf(port_str, "%d", port);
    char * megastring = calloc(strlen(dotted)+strlen(port_str), sizeof(char));
    strcpy(megastring, dotted);
    strcat(megastring, port_str);
    return megastring;
}

// Deal with timestamp eventually.
char * message_record(char* dotted, int port, char * message)
{
    char * port_dot = port_dotted(dotted, port);
    time_t timer;
    time(&timer);
    char * mes_time = port_dotted(message,timer); 
    int size = strlen(mes_time) + strlen(port_dot);

   // int size = strlen(dotted) + strlen(message) + strlen(port_str);
    char * megastring = calloc(size, sizeof(char));
    strcpy(megastring, port_dot);
    strcat(megastring, mes_time);
    return megastring;
}

/*
 *
 * Here is where a function to obscure "dotted" and "port" into
 * just one thing.
 */

int registry_found(char * dotted, int port)
{
    int i;
    int found = -1;
    while(i < ntargets_ && found == -1)
    {
        
            if( targets_[i].port == port && strcmp( targets_[i].dotted, dotted) == 0)
                found = i;
        i++;
    }
    
    return found;
}

int is_parent(int loc)
{
    if(loc < child_start)
        return TRUE;
    else
        return FALSE;
}
int delete_parent(int loc)
{
    int replacement = loc;
    if(is_parent(loc) == TRUE);
    {
            // If it's not the last parent
           if(child_start - 1 > 0)
           {
                // If it is not the farthest right parent
                if(loc < child_start - 1)
                {
                    replacement = child_start - 1;

                    targets_[loc].saddr = targets_[replacement].saddr;
                    targets_[loc].dotted = targets_[replacement].dotted;
                    targets_[loc].port = targets_[replacement].port;
                }
           }
    }
    child_start --;
    return replacement;
}
void delete_registry(int loc)
{
    if(loc < child_start)
        loc = delete_parent(loc);
    if(ntargets_ > 1)
    {
        int rep = ntargets_ - 1;
    
        targets_[loc].saddr = targets_[rep].saddr;
        targets_[loc].dotted = targets_[rep].dotted;
        targets_[loc].port = targets_[rep].port;
    }
    ntargets_--;
}

void replace_registry(int loc, registry * new_r)
{
    if(loc > ntargets_ || 
            registry_found(targets_[loc].dotted, targets_[loc].port) < 0)
    {
        fprintf(stderr, "DOES NOT APPEAR TO EXIST.");
    }
    else
    {
        if(new_r == NULL || registry_found(new_r->dotted, new_r->port) > 0)
        {
            delete_registry(loc);
        }
        else {
            if(loc < child_start)
            {
                loc = delete_parent(loc);
            }
            targets_[loc].saddr = new_r->saddr;
            targets_[loc].dotted = new_r->dotted;
            targets_[loc].port = new_r -> port;
	    send_register(messenger_sockfd, (struct sockaddr *) &(targets_[loc].saddr));
        }
    }
}

// an interrupt-handler for control-C (to enable orderly shutdown) 
void handler(int signal) { 
    int i; 
    fprintf(stderr,"messenger: received control-C\n"); 

    struct sockaddr * toaddr;
    struct sockaddr * redaddr;

    int nparents = child_start;

    // If there are not any parents  
    if(nparents <= 0)
    {
        if(ntargets_ > 1)
        {
            //Designate the first target as the parent
            redaddr = (struct sockaddr *) &(targets_[0].saddr);
            send_redirect(messenger_sockfd, redaddr, redaddr);
            for(i=1; i < ntargets_; i++)
            {   
                toaddr =  (struct sockaddr *) &(targets_[i].saddr);
                send_redirect(messenger_sockfd, toaddr, redaddr);
            }
        }
        else if(ntargets == 1)
        {
            redaddr = NULL;
            toaddr =  (struct sockaddr *) &(targets_[0].saddr);
             
            send_redirect(messenger_sockfd, toaddr, redaddr);
        }
    }
    else{
            for(i=0; i <ntargets_; i++)
            {
                redaddr = (struct sockaddr *) &(targets_[i%nparents].saddr);
                toaddr = (struct sockaddr *) &(targets_[i].saddr);
                send_redirect(messenger_sockfd, toaddr, redaddr);
            }
    }
    exit(1); 
} 

int main(int argc, char **argv)
{
    int i; 
    char *target_dotted; 		/* human-readable address */ 
    int target_port; 			/* port in local order */
    
/* intercept control-C and do an orderly shutdown */ 
    signal(SIGINT, handler); 

/* read arguments */ 
    if (argc%2 == 1 && argc/2 < 11) { // must have an odd number of arguments
	for (i=1; i<argc; i+=2) { 
	    /* target server data */ 
	    struct in_addr target_addr; 	/* address in network order */ 
	    target_dotted = argv[i]; 
	    target_port = atoi(argv[i+1]); 
	    if (!messenger_arguments_valid(target_dotted, target_port)) { 
		    messenger_usage(); 
		    exit(1); 
	    } 
	    inet_pton(AF_INET, target_dotted, &target_addr);
	    memset(&targets[ntargets], 0, sizeof(struct sockaddr_in)); 
	    targets[ntargets].sin_family = PF_INET;
	    memcpy(&targets[ntargets].sin_addr,&target_addr,
		sizeof(struct in_addr)); 
	    targets[ntargets].sin_port   = htons(target_port);

	    targets_[ntargets_].dotted = target_dotted;
            targets_[ntargets_].port = target_port;
            targets_[ntargets_].saddr = targets[ntargets_];
            ntargets++; // one target
            ntargets_++;
	}
        child_start = ntargets_;

    } else { 
	fprintf(stderr, "%s: wrong number of arguments\n",argv[0]); 
	messenger_usage(); 
        exit(1); 
    } 
/* get the primary IP address of this host */ 
    get_primary_addr(&messenger_addr); 
    inet_ntop(AF_INET, &messenger_addr, messenger_dotted, INET_ADDRSTRLEN);

/* make a socket*/ 
    messenger_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (messenger_sockfd<0) { 
	perror("can't open socket"); 
	exit(1); 
    } 

/* construct an endpoint address with primary address and random port */ 
    memset(&messenger_sockaddr, 0, sizeof(messenger_sockaddr));
    messenger_sockaddr.sin_family = PF_INET;
    memcpy(&messenger_sockaddr.sin_addr,&messenger_addr,
	sizeof(struct in_addr)); 
    messenger_sockaddr.sin_port   = 0; // choose port randomly 

/* bind it to an appropriate local address and choose port */ 
    if (bind(messenger_sockfd, (struct sockaddr *) &messenger_sockaddr, 
	     sizeof(messenger_sockaddr))<0) { 
	perror("can't bind local address"); 
	exit(1); 
    } 

/* recover and report random port assignment */ 
    socklen_t len = sizeof(struct sockaddr); 
    if (getsockname(messenger_sockfd, (struct sockaddr *) &messenger_sockaddr, 
          &len)<0) { 
	perror("can't recover bound local port"); 
	exit (1); 
    } 
    messenger_port = ntohs(messenger_sockaddr.sin_port); 

// print informative message about how to proceed
    fprintf(stderr, "Type '%s %s %d' to contact this server\n", 
	argv[0], messenger_dotted, messenger_port); 

// contact other targets, if any, with registration commands 
// this code does not run if the number of targets is zero
    for (i=0; i<ntargets; i++) { 
	send_register(messenger_sockfd, (struct sockaddr *) &targets[i]);
    } 
    registry ** regs = NULL;
    int nreg = 0;
    while (TRUE) { // infinite loop: end with control-C 

        // Watch stdin, sockfd to see when either has input.
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(messenger_sockfd, &rfds);
	FD_SET(fileno(stdin), &rfds);
	if (select(messenger_sockfd+1, &rfds, NULL, NULL, NULL)<0) { 
	    perror("select"); 
	    exit(1); 
	} 

	// check for input from socket 
	if (FD_ISSET(messenger_sockfd, &rfds)) { 
	    struct packet net; 		  // network order 
	    struct packet local; 	  // local order 
	    struct sockaddr_in send_addr; // sender address 
	    ssize_t retval = recv_packet(messenger_sockfd, &net, 
		    (struct sockaddr *)&send_addr); 

	    if (retval<0) { 
	    /* error */ 
		perror("recv_packet"); 
		exit(1); 
	    } else { 
	    /* get human-readable internet address */
		char send_dotted[INET_ADDRSTRLEN]; /* sender address */ 
		int send_port; 			   /* sender port */ 
		char red_dotted[INET_ADDRSTRLEN];  /* redirect address */ 
		int red_port; 			   /* redirect port */ 

		// convert sender address to local human-readable format 
		inet_ntop(AF_INET, (void *)&(send_addr.sin_addr.s_addr),  
		    send_dotted, INET_ADDRSTRLEN);
		send_port = ntohs(send_addr.sin_port); 

		// convert sent packet to local ordering 
		packet_network_to_local(&local, &net); 

		switch (local.magic) { 
		    case MAGIC_MESSAGE: 
                        if(message_found(local.u.mes.digest) < 0);
                        {
                            // HANDLE MESSAGE PRINTING HERE. 
                        
                        
                            fprintf(stderr, "%s:%d: %s\n", send_dotted, send_port,
                                                        local.u.mes.message);
                        

                            // Lower nodes sending upwards
                        
                            int r_found = registry_found(send_dotted, send_port);
                            if(r_found >= 0)
                            {    // Send to higher parts of tree
                                for (i=0; i<ntargets_; i++) { 
		                    if(i != r_found)
                                    {
                                        send_message(messenger_sockfd, (struct sockaddr *) 
			                    &(targets_[i].saddr),local.u.mes.digest, local.u.mes.message);
                                    }
	                        }
                            }
                            store_message(local.u.mes.digest);
                       }
                        break; 
		    case MAGIC_REGISTER: 
			// HANDLE REGISTRATION REQUESTS HERE. 
			
                        if(ntargets_ >= MAXTARGETS)
                        {
                            // Potentially send a redirect??!?!
                                
                            struct sockaddr * redaddr =  (struct sockaddr *) &(targets_[ntargets_-1].saddr);
                            send_redirect(messenger_sockfd, (struct sockaddr * )&send_addr, redaddr);
                        }               
                        // Filters out duplicates
                        else if(registry_found(send_dotted, send_port) < 0) 
                        {
                            targets_[ntargets_].dotted = send_dotted;
                            targets_[ntargets_].port = send_port;
                            targets_[ntargets_].saddr = send_addr;
                            
                            ntargets_++;
                        }
                        break; 
		    case MAGIC_REDIRECT: 
			for (i=0; i<local.u.red.nredirects; i++) { 

			    // convert redirect address to human-readable 
			    struct sockaddr_in *s = (struct sockaddr_in *) 
				&(local.u.red.redirects[i]); 

			    inet_ntop(AF_INET, 
				(void *)&(s->sin_addr.s_addr),  
				red_dotted, INET_ADDRSTRLEN);
			    red_port = ntohs(s->sin_port); 
			    
                            if(red_port == messenger_port && 
                                        strcmp(red_dotted, messenger_dotted) == 0)
                            {
                                delete_registry(registry_found(send_dotted, send_port));
                            }
                            else{
                                // deal with null case
                                registry *  r = calloc(1, sizeof(registry));
                                r->port = red_port;
                                r->dotted = red_dotted;
                                r->saddr = *s;

                                replace_registry(registry_found(send_dotted, send_port), r);
                            }
                        }

			break; 
		    default: 
			fprintf(stderr, "%s:%d: UNKNOWN PACKET TYPE %d\n", 
			    send_dotted, send_port, local.magic); 
			break; 
		} 
	    } 
	} 

	// check for input from terminal (user typed \n)
	if (FD_ISSET(fileno(stdin), &rfds)) { 
	    // flood message to each destination in "targets" 
	    char message[MAXMESSAGE]; 
	    fgets(message, MAXMESSAGE, stdin); 
            message[strlen(message)-1]='\0'; 
	
            char * s = message_record(messenger_dotted, messenger_port, message);
            unsigned char digest[MD5_DIGEST_LENGTH]; 
	    
            MD5((const unsigned char *)s, strlen(s), s);
	    for (i=0; i<ntargets_; i++) { 
		send_message(messenger_sockfd, (struct sockaddr *) 
			&(targets_[i].saddr),digest,message);
	    }
	} 
    } 
}

