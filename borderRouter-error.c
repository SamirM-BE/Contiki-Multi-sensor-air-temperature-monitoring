#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include <stdio.h> /* For printf() */
#include <stdlib.h> //for malloc
#include <stdbool.h> // for boolean
#include <string.h> // for string operations
#include <limits.h> 
#include "random.h" // for the generation of fake data sensor
#include "dev/button-sensor.h"
#include "dev/cc2420/cc2420.h" // In order to recognize the variable cc2420_last_rssi
#include "../PROJECT1/mobileP2/linkedListChild.h" // Handle linkedlist
#include "../PROJECT1/mobileP2/linkedListHello.h" // Handle linkedlist
#include "lib/list.h" //for runicast
#include "lib/memb.h" //for runicast
#include "dev/serial-line.h"

#define MAX_RETRANSMISSIONS 10 //for runicast
#define NUM_HISTORY_ENTRIES 10 //for runicas

/* ------ PROCESSES DEFINITION ------ */

PROCESS(border_router_process, "border router process");
PROCESS(broadcast_routing_process, "routing broadcast");
PROCESS(server_comm_process, "Server communication process");
AUTOSTART_PROCESSES(&border_router_process, &broadcast_routing_process);

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

struct RUNICAST_ACTION {
	linkaddr_t dest_addr;
	bool openValve;
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
	bool openValve;
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
static struct Node me;

static struct runicast_conn runicast_routing_conn;
static struct runicast_conn runicast_data_conn;
static struct runicast_conn runicast_action_conn;

static struct broadcast_conn broadcast_routing_conn;


/* ----- FUNCTIONS ------ */



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
	  
	  
	
	//printf("RECIVED RUNICAST DATA from %d.%d seq: %d\n",  from->u8[0], from->u8[1], seqno);
	
	struct RUNICAST_DATA *packet = (struct RUNICAST_DATA *) packetbuf_dataptr();
	int val = packet->val;
	int min = packet->min;
	bool forwarded = packet->forwarded;
	printf("DATA: %d.%d, %d, %d\n", packet->addr.u8[0], packet->addr.u8[1], min, val);

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

// the action to open the valve for 10 minutes coming from the computational node or the server
static void recv_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	printf("RUNICAST ACTION RECEIVED ! \n");
}

static void sent_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast action sent \n");
}

static void timeout_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast action timeout \n");
	//TODO Delete child
}


// the hello message we received in broadcast
static void broadcast_routing_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	printf("receved hello \n");
}

/* ------ Static connexion's callbacks ------ */

static const struct broadcast_callbacks broadcast_routing_callbacks = {broadcast_routing_recv};
static const struct runicast_callbacks runicast_routing_callbacks = {recv_runicast_routing};
static const struct runicast_callbacks runicast_data_callbacks = {recv_runicast_data};
static const struct runicast_callbacks runicast_action_callbacks = {recv_runicast_action, sent_runicast_action, timeout_runicast_action};

/* -------- PROCESSES ------- */ 

PROCESS_THREAD(broadcast_routing_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	
	PROCESS_BEGIN();
	printf("Broadcast routing begin ! \n");
	
	broadcast_open(&broadcast_routing_conn, 129, &broadcast_routing_callbacks);
	//PROCESS_WAIT_EVENT_UNTIL(1); // Necessary (but we don't understand why)
	
	static struct etimer hello_timer;
	while(1){
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
	
	PROCESS_END()
}

PROCESS_THREAD(border_router_process, ev, data){
	// Init me node
	me.addr = linkaddr_node_addr;
	me.dist_to_server = 1;
	
	// Handling exit connexions
	PROCESS_EXITHANDLER(runicast_close(&runicast_routing_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_data_conn);)
	//PROCESS_EXITHANDLER(runicast_close(&runicast_action_conn);)
	
	PROCESS_BEGIN(); 
	printf("Border Router Process runing \n");
		
	// Open connexions
	runicast_open(&runicast_routing_conn, 144, &runicast_routing_callbacks);
	runicast_open(&runicast_data_conn, 164, &runicast_data_callbacks);
	//runicast_open(&runicast_action_conn, 174, &runicast_action_callbacks);
	

	// Start any needed process
	process_start(&server_comm_process, NULL);
	
	PROCESS_WAIT_EVENT_UNTIL(0);
	PROCESS_END();
}

PROCESS_THREAD(server_comm_process, ev, data){
	PROCESS_EXITHANDLER(runicast_close(&runicast_action_conn);)
	PROCESS_BEGIN();
   
	printf("Server comm process started ! \n");
	
	//PROCESS_WAIT_EVENT_UNTIL(1); 
	printf("br open conn \n");
	runicast_open(&runicast_action_conn, 174, &runicast_action_callbacks);
	
	
	static struct etimer testtimer;
	while(1) {   
		//PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message && data != NULL);
		etimer_set(&testtimer, 30*CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&testtimer));
		//printf("%s\n", (char *)data);
		//char *tempo = (char *)data;
		//char a = tempo[0];
		//char b = tempo[2];
		//int aa = a - '0';
		//int bb = b - '0';
		int aa = 3;
		int bb = 0;
			
		static struct etimer action_timer;
		struct Child *current = headChild;
		while(current != NULL){
			printf("current: %d.%d \n", current->addr.u8[0], current->addr.u8[1]);
			struct RUNICAST_ACTION packet;
			packet.dest_addr.u8[0] = aa;
			packet.dest_addr.u8[1] = bb;
			packet.openValve = true;
				
			while(runicast_is_transmitting(&runicast_action_conn)){
				//printf("stuck in loop \n");
			}
			packetbuf_clear();
			packetbuf_copyfrom(&packet, sizeof(packet));
			runicast_send(&runicast_action_conn, &(current->addr), MAX_RETRANSMISSIONS);
				
			printf("border router sent action to child ! \n");
				
			current = current->next;
		}
	}
	
	PROCESS_END();
 }

