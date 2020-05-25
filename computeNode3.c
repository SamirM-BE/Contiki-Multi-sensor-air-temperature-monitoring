#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include <stdio.h> 
#include <stdlib.h> 
#include <stdbool.h> 
#include <string.h>
#include <limits.h> 
#include "random.h" // for the generation of fake data sensor
#include "dev/button-sensor.h"
#include "dev/cc2420/cc2420.h" // In order to recognize the variable cc2420_last_rssi
#include "../PROJECT1/mobileP2/linkedListChild.h"
#include "../PROJECT1/mobileP2/linkedListHello.h"
#include "../PROJECT1/mobileP2/linkedListData.h" 
#include "lib/list.h" //for runicast
#include "lib/memb.h" //for runicast

#define MAX_RETRANSMISSIONS 10 //for runicast
#define NUM_HISTORY_ENTRIES 10 //for runicast
#define MAX_COMPUTE 5 // Maximum number of node handle by the computation node
#define SLOPE_MIN_TRESHOLD 0 // Treshold of least square slope to send openvalve action
#define MAX_KEEPALIVE 2 // Max number of minutes that a child node can stay our child without giving sign of life

/* ------ PROCESSES DEFINITION ------ */

PROCESS(compute_node_process, "compute node process");
PROCESS(broadcast_routing_process, "routing broadcast");
PROCESS(recv_action_process, "recv action process");
PROCESS(action_process, "action process");
PROCESS(recv_hello_process, "recv hello process");
PROCESS(runicast_data_process, "send runicast data");
AUTOSTART_PROCESSES(&compute_node_process, &broadcast_routing_process);

struct history_entry {
  struct history_entry *next;
  linkaddr_t addr;
  uint8_t seq;
};
LIST(history_table);
MEMB(history_mem, struct history_entry, NUM_HISTORY_ENTRIES);

// the hello message sent regularly
struct BROADCAST_ROUTING {
	linkaddr_t addr;
	int dist_to_server;
};

// response to the hello message after having selecting him as new parent, equivalent of the "ack hello"
struct RUNICAST_ROUTING {
	linkaddr_t addr;
	bool isChild;
};

// when a sensor node wants to send his data every minute to a computational node
struct RUNICAST_DATA {
	linkaddr_t addr;
	bool forwarded;
	int min;
	int val;
};

// sent by the server or the computational node to tell the sensor node to open his valve
struct BROADCAST_ACTION {
	linkaddr_t dest_addr;
	int dist_to_server;
};

struct Parent
{
	bool valid;
	int rss;
	linkaddr_t addr;
	int dist_to_server;
};
typedef struct Parent Parent;

struct Node
{
	linkaddr_t addr;
	int dist_to_server;
};


/* ----- STATIC VARIABLES -------- */
static struct Child *headChild = NULL;
static struct Hello *headHello = NULL; // List of Hello packet received
static struct Child* headComputedNode = NULL;

static struct Node me;
static Parent parent;
static bool allow_recv_hello = false;
static int compute_table_lenght = 0;
static struct Data *compute_table[MAX_COMPUTE] = {NULL};
static linkaddr_t addr_table [MAX_COMPUTE] = {{0, 0}};


static struct runicast_conn runicast_routing_conn;
static struct runicast_conn runicast_data_conn;

static struct broadcast_conn broadcast_action_conn;
static struct broadcast_conn broadcast_routing_conn;


/* ----- FUNCTIONS ------ */

static void resetParent(){
	parent.addr.u8[0] = 0;
	parent.addr.u8[1] = 0;
	parent.rss = INT_MIN;
	parent.valid = false;
}

// Return tab index of addr if tab contains 
static int getTabIndex(linkaddr_t *addr){
	int i = 0;
	for(i = 0; i < MAX_COMPUTE; i++){
		if(linkaddr_cmp(addr, &(addr_table[i])) != 0){
			printf("index: %d \n", i);
			return i;
		}
	}
	return -1;
}


// Compute least square of y value on axis from 0 to n-1 
static int least_square_slope(struct Data* data, int n){
	int i = 30;
	double sumx = 0;
	double sumx2 = 0;
	double sumy = 0;
	double sumyx = 0;
	double res = 0;
	
	struct Data *ptr = data;

	//start from the beginning
	while(ptr != NULL && i > 0) {
		sumx = sumx + i;
		sumx2 = sumx2 + i*i;
		sumy = sumy + ptr->val;
		sumyx = sumyx + (ptr->val)*i;
		ptr = ptr->next;
		i = i - 1;
	}
	
	res = (n*sumyx - sumx*sumx) / (n*sumx2 - sumx*sumx);
	if(res > SLOPE_MIN_TRESHOLD){
		return 1;
	}
	return 0;
}

struct Child* deleteOldComputedNode(struct Child *head){
    struct Child *ptr = head;
	
    //start from the beginning
    while(ptr != NULL) {
        unsigned long interval = clock_seconds() - ptr->timestamp;
		printf("interval: %d \n", interval);
        if(interval > MAX_KEEPALIVE*60){
            //Child seems probably down - remove from list
            printf("Delete ComputedNode %d.%d \n", ptr->addr.u8[0], ptr->addr.u8[1]);
            struct Child *newHead = delete(head, ptr->addr);
			// delete from compute_table addr_table
			int ind = getTabIndex(&ptr->addr);
			
			linkaddr_t def_addr = {{0, 0}};
			struct Data *def_data = NULL;
			
			compute_table[ind] = def_data;
			addr_table[ind] = def_addr;
			compute_table_lenght --;
            return deleteOldComputedNode(newHead);
        }
        if(ptr->next == NULL){
            return head;
        }
        else{
            ptr = ptr->next;
        }
    }
}

/* ------ CONNEXIONS FUNCTIONS ------ */

// the values of the sensor
static void recv_runicast_data(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	 /* OPTIONAL: Sender history */
	  struct history_entry *e = NULL;
	  for(e = list_head(history_table); e != NULL; e = e->next) {
		if(linkaddr_cmp(&e->addr, from)) {
		  break;
		}
	  }
	  if(e == NULL) {
		/* Create new history entry */
		e = memb_alloc(&history_mem);
		if(e == NULL) {
		  e = list_chop(history_table); /* Remove oldest at full history */
		}
		linkaddr_copy(&e->addr, from);
		e->seq = seqno;
		list_push(history_table, e);
	  } else {
		/* Detect duplicate callback */
		if(e->seq == seqno) {
		  printf("runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
			 from->u8[0], from->u8[1], seqno);
		  return;
		}
		/* Update existing history entry */
		e->seq = seqno;
	  }
	
	printf("RECIVED RUNICAST DATA from %d.%d seq: %d\n",  from->u8[0], from->u8[1], seqno);
	printList(headComputedNode);
	struct RUNICAST_DATA *packet = packetbuf_dataptr();
	
	// Checking if from is known in our computation table
	int index = getTabIndex(&(packet->addr));
	
	// addr not in table
	if(index == -1){ 
		printf("not in table \n");
		if(compute_table_lenght < MAX_COMPUTE){ // If their is place for new addr data
			// Adding address to translate_table
			linkaddr_t def_addr = {{0, 0}};
			int z = 0; 
			for(z=0; z < MAX_COMPUTE; z++){
				if(linkaddr_cmp(&addr_table[z], &def_addr) != 0){
					printf("adding compute node %d.%d \n", packet->addr.u8[0], packet->addr.u8[1]);
					headComputedNode = insert(headComputedNode, packet->addr, clock_seconds());
					addr_table[z] = packet->addr;
					compute_table_lenght ++;
					break;
				}
			}
		
			int newIndex = getTabIndex(&(packet->addr));
			if(newIndex != -1){
				compute_table[newIndex] = insertData(compute_table[newIndex], packet->val); // Adding data to table corresponding to new address
			}
			else{
				printf("Error adding add in table_addr of compute node ! \n");
			}
		}
		else{
			//Forward data to parent
			packet->forwarded = true;
	
			packetbuf_clear();
			packetbuf_copyfrom(packet, sizeof(struct RUNICAST_DATA));
			printf("Message form %d:%d forwareded to %d:%d\n",from->u8[0], from->u8[1], parent.addr.u8[0], parent.addr.u8[1]);
			runicast_send(&runicast_data_conn, &parent.addr, MAX_RETRANSMISSIONS); //the second argument is the address of our parent wich is our parent
		}
	}
	else{ // Addr already in addr_table for computation
		printf("in table \n");
		
		headComputedNode = update(headComputedNode, packet->addr, clock_seconds());		
		compute_table[index] = insertData(compute_table[index], packet->val);
		printf("newlength : %d \n", lengthData(compute_table[index]));
		// Table is full of data - compute least square
		if(lengthData(compute_table[index]) == 30){
			printf("30 DATA - computation \n");
			
			int slope = least_square_slope(compute_table[index], 30);
			printf("openslope: %d \n", slope);
			if(slope == 1){
				printf("send Action to child \n");
				// preparing argument to pass to the process				
			    process_data_t arg = &packet->addr;
			
			   // Start action process, sending action
			   process_start(&action_process, arg);
			}
			
			// Reset compute_table_value for this address
			compute_table[index] = NULL;
		}
	}
	
}

static void sent_runicast_data(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast data sent \n");
}

static void timeout_runicast_data(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast data timeout - Parent %d.%d down \n", parent.addr.u8[0], parent.addr.u8[1]);
	
	process_exit(&runicast_data_process);
	resetParent();
	process_start(&recv_hello_process, NULL); 
}


// Received routing runicast
static void recv_runicast_routing(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	printf("Child info received ! \n");

	struct RUNICAST_ROUTING *packet = packetbuf_dataptr();
	
	// Adding child to child's list
	headChild = insert(headChild, packet->addr, clock_seconds());
	printList(headChild);
	printf("Child %d.%d added \n", packet->addr.u8[0], packet->addr.u8[1]);
}

static void sent_runicast_routing(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast routing sent \n");
}

static void timeout_runicast_routing(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast routing timeout \n");
	
	process_exit(&runicast_data_process);
	resetParent();
	process_start(&recv_hello_process, NULL);
}

static void broadcast_action_recv(struct broadcast_conn * c, const linkaddr_t * from){
	struct BROADCAST_ACTION *packet = (struct BROADCAST_ACTION*) packetbuf_dataptr();
	printf("ACTION RCV from %d.%d, to %d.%d\n", from->u8[0], from->u8[1], packet->dest_addr.u8[0], packet->dest_addr.u8[1]);
	
		printf("dist packet: %d me: %d \n",packet->dist_to_server, me.dist_to_server);
		if(packet->dist_to_server < me.dist_to_server){
			if(linkaddr_cmp(&packet->dest_addr, &me.addr) != 0 ){
				printf("ADDRESED TO ME \n");
			}
			else{
				printf("dist min received -> transfer \n");
				packet->dist_to_server = me.dist_to_server;
				packetbuf_clear();
				packetbuf_copyfrom(packet, sizeof(struct BROADCAST_ACTION));
				broadcast_send(&broadcast_action_conn);
			}
		}
}


// the hello message we received in broadcast
static void broadcast_routing_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	if(allow_recv_hello){      
        printf("add addr to linked list \n");
		int rss;
        static signed char rss_val; 
    
        rss_val = cc2420_last_rssi; //RSS = Signal Strength
        rss = rss_val+45; // Add 45 to RSS - read in documentation
		
		struct BROADCAST_ROUTING *routing_packet = (struct BROADCAST_ROUTING*) packetbuf_dataptr();	
		
		if(me.dist_to_server == INT_MAX || routing_packet->dist_to_server < me.dist_to_server){
			headHello = insertHello(headHello, routing_packet->addr, rss, routing_packet->dist_to_server);
			printListHello(headHello);
		}
	}
}

/* ------ Static connexion's callbacks ------ */

static const struct broadcast_callbacks broadcast_routing_callbacks = {broadcast_routing_recv};
static const struct runicast_callbacks runicast_routing_callbacks = {recv_runicast_routing, sent_runicast_routing, timeout_runicast_routing};
static const struct runicast_callbacks runicast_data_callbacks = {recv_runicast_data, sent_runicast_data, timeout_runicast_data};
static const struct broadcast_callbacks broadcast_action_callbacks = {broadcast_action_recv};


/* -------- PROCESSES ------- */ 

/* ROUTING PROCESS */
// Send Hello broadcast every 120sec
PROCESS_THREAD(broadcast_routing_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	
	PROCESS_BEGIN();
	printf("Broadcast routing begin ! \n");
	
	broadcast_open(&broadcast_routing_conn, 129, &broadcast_routing_callbacks);
	
	PROCESS_WAIT_EVENT_UNTIL(1); // Necessary (but we don't understand why)
	
	static struct etimer hello_timer;
	while(1){
		if(me.dist_to_server != INT_MAX){
			etimer_set(&hello_timer, 120*CLOCK_SECOND + (random_rand() % 20*CLOCK_SECOND) );
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&hello_timer));
			
			struct BROADCAST_ROUTING broadcast_routing_packet;
			broadcast_routing_packet.addr = me.addr; 
			broadcast_routing_packet.dist_to_server = me.dist_to_server;
			
			packetbuf_clear();
			packetbuf_copyfrom(&broadcast_routing_packet, sizeof(broadcast_routing_packet));
			
			broadcast_send(&broadcast_routing_conn);
			printf("Hello message sent \n");
		}
	}
	
	PROCESS_END();
}

/* RECV HELLO MESSAGE PROCESS */
// Allow receiving hello messages during 250sec
PROCESS_THREAD(recv_hello_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	PROCESS_BEGIN();
	printf("Process recv hello started \n");
	
	while(1){
		// Timer init
		static struct etimer allow_recv;
		etimer_set(&allow_recv, 240*CLOCK_SECOND + (random_rand() % 30*CLOCK_SECOND) );
		
		// Allow receive hello message
		headHello = NULL;
		allow_recv_hello = true;
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&allow_recv));
		allow_recv_hello = false;
		
		//handle linked list
		if(!isEmpty(headHello)){
			// First pairing to parent -> need dist_to_server computation
			struct Hello *newHello = biggestRssHello(headHello);
			parent.addr = newHello->addr;
			parent.rss = newHello->rss;
			parent.dist_to_server = newHello->dist_to_server;
			parent.valid = true;
			
			if(me.dist_to_server == INT_MAX){ 
				me.dist_to_server = parent.dist_to_server +1 ;
			}
			
			printf("Parent: %d.%d - new_dist: %d \n", parent.addr.u8[0], parent.addr.u8[1], me.dist_to_server);
			
			//send childConfirmation to parent
			struct RUNICAST_ROUTING sendPacket;
			sendPacket.addr = me.addr;
			sendPacket.isChild = true;
			
			while(runicast_is_transmitting(&runicast_routing_conn)){}
			packetbuf_clear();
			packetbuf_copyfrom(&sendPacket, sizeof(sendPacket));
			runicast_send(&runicast_routing_conn, &parent.addr, MAX_RETRANSMISSIONS);
			
			// Starting data process
			process_start(&runicast_data_process, NULL);
			
			// Starting recv action process
			process_start(&recv_action_process, NULL);
			
			printf("Exiting recv hello process \n");
			PROCESS_EXIT();
		}
	}
	
	PROCESS_END()
}


/* GLOBAL PROCESS */
PROCESS_THREAD(compute_node_process, ev, data){
	// Init me node
	me.addr = linkaddr_node_addr;
	me.dist_to_server = INT_MAX;
	
	// Handling exit connexions
	PROCESS_EXITHANDLER(runicast_close(&runicast_routing_conn);)
	//PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	
	PROCESS_BEGIN(); 
	printf("Compute Node Process runing \n");
	
	resetParent();
	
	
	// Open connexions
	//broadcast_open(&broadcast_routing_conn, 129, &broadcast_routing_callbacks);
	runicast_open(&runicast_routing_conn, 144, &runicast_routing_callbacks);

	// Allow recv hello message to find parent and dist
	process_start(&recv_hello_process, NULL); 
	

	PROCESS_END();
}

/* DATA PROCESS */
PROCESS_THREAD(runicast_data_process, ev, data) {
	PROCESS_EXITHANDLER(runicast_close(&runicast_data_conn););
	PROCESS_BEGIN(); 
	
	// Init time shifting
	static struct etimer startTimer;
	etimer_set(&startTimer, (random_rand() % 30*CLOCK_SECOND));
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&startTimer)); 

	printf("RUNICAST DATA STARTED\n");
	
	runicast_open(&runicast_data_conn, 164, &runicast_data_callbacks);

	while (1) {
		static struct etimer checkChildAlive;
		etimer_set(&checkChildAlive,120*CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&checkChildAlive));
		
		headComputedNode = deleteOldComputedNode(headComputedNode);
	}

	PROCESS_END();
}

PROCESS_THREAD(recv_action_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_action_conn);)
	PROCESS_BEGIN(); 
	
	printf("recv action porcess started ! \n");
	
	//PROCESS_WAIT_EVENT_UNTIL(1);
	printf("open action conn \n");
	broadcast_open(&broadcast_action_conn, 139, &broadcast_action_callbacks);
	
	printf("wait for action \n");
	PROCESS_WAIT_EVENT_UNTIL(0);
	
	printf("recv action process ended \n");
	PROCESS_END();
}


PROCESS_THREAD(action_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_action_conn);)
	
	// Get address passed in data
	linkaddr_t *dest_add = data;
	printf("dst: %d.%d \n", dest_add->u8[0], dest_add->u8[1]);
	
	linkaddr_t recv_addr;
	recv_addr.u8[0] = dest_add->u8[0];
	recv_addr.u8[1] = dest_add->u8[1];

	PROCESS_BEGIN(); 
	printf("action process launched ! \n");
	
	broadcast_open(&broadcast_action_conn, 139, &broadcast_routing_callbacks);
	
	struct BROADCAST_ACTION *packet = malloc(sizeof(struct BROADCAST_ACTION));
	packet->dest_addr = recv_addr; 
	packet->dist_to_server = me.dist_to_server;
			
	packetbuf_clear();
	packetbuf_copyfrom(packet, sizeof(struct BROADCAST_ACTION));
			
	broadcast_send(&broadcast_action_conn);

	PROCESS_EXIT();
	PROCESS_END();
}
