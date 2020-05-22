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

//PROCESS(sensor_node_process, "sensor node process");
PROCESS(broadcast_routing_process, "routing broadcast");
//PROCESS(children_alive_process, "remove dead child from child's list");
PROCESS(test_serial, "Serial line test process");
AUTOSTART_PROCESSES(&broadcast_routing_process, &test_serial);

//TODO gérer process children alive

struct history_entry {
  struct history_entry *next;
  linkaddr_t addr;
  uint8_t seq;
};
LIST(history_table);
MEMB(history_mem, struct history_entry, NUM_HISTORY_ENTRIES);

// message sent when a node lost his parent
struct BROADCAST_LOST {
	linkaddr_t addr;
};

//answer of a new potential node for lonely child :'(
struct RUNICAST_LOST {
	linkaddr_t addr;
	int dist_to_server;
};

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
struct RUNICAST_ACTION {
	linkaddr_t dest_addr;
	bool openValve;
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
static struct Lost *headLost = NULL; // List of Lost packet received
static struct Node me;
static Parent parent;
static int clock_s = 1; //our clock_s for the minute axis to send to the computational node
static bool allow_recv_hello = false;
static bool allow_recv_lost = false;
static bool toToggle = false; //when we received the instruction to toggle the LED

static struct runicast_conn runicast_routing_conn;
static struct runicast_conn runicast_lost_conn;
static struct runicast_conn runicast_data_conn;
static struct runicast_conn runicast_action_conn;

static struct broadcast_conn broadcast_routing_conn;
static struct broadcast_conn broadcast_lost_conn;


/* ----- FUNCTIONS ------ */

static struct RUNICAST_DATA generate_random_data(struct RUNICAST_DATA sendPacket){
	//generate a random value between 1 and 100
	int max_val = 100;
	int min_val = 1;
	int random_val = random_rand();

	if(random_val < 0){
		random_val *= -1;
	}
	 
	int proposal_value = (random_val % max_val) + min_val;
	sendPacket.min = clock_s;
	sendPacket.val = proposal_value;
	return sendPacket;
}

static void resetParent(){
	parent.addr.u8[0] = 0;
	parent.addr.u8[1] = 0;
	parent.rss = INT_MIN;
	parent.valid = false;
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
	
	//printf("RECIVED RUNICAST DATA from %d.%d seq: %d\n",  from->u8[0], from->u8[1], seqno);
	
	struct RUNICAST_DATA *packet = packetbuf_dataptr();
	printf("DATA: %d.%d, %d, %d\n", packet->addr.u8[0], packet->addr.u8[1], packet->min, packet->val);
   
	//we update the timestamp of our child
	const linkaddr_t ch = packet -> addr;
	headChild = update(headChild, ch, clock_seconds());
	
	//as we are a sensor node, we have to set the forwarded boolean to true
	packet->forwarded = true;
	
	linkaddr_t recv;
	recv.u8[0] = parent.addr.u8[0];
	recv.u8[1] = parent.addr.u8[1];
}
 
// the action to open the valve for 10 minutes coming from the computational node or the server
static void recv_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	printf("Runicast action received \n");
}

static void sent_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast action sent \n");
}

static void timeout_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast action timeout \n");
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


// the hello message we received in broadcast
static void broadcast_routing_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	if(allow_recv_hello){ // equivalent to say that the node does not have parent yet and just spawned in an existing network        
        printf("add addr to linked list \n");
		int rss;
        static signed char rss_val; 
    
        rss_val = cc2420_last_rssi; //RSS = Signal Strength
        rss = rss_val+45; // Add 45 to RSS - read in documentation
		
		struct BROADCAST_ROUTING *routing_packet = (struct BROADCAST_ROUTING*) packetbuf_dataptr();	
		
		if(me.dist_to_server == INT_MAX || routing_packet->dist_to_server <= me.dist_to_server){
			headHello = insertHello(headHello, routing_packet->addr, rss, routing_packet->dist_to_server);
			printListHello(headHello);
		}
	}
}

/* ------ Static connexion's callbacks ------ */

static const struct broadcast_callbacks broadcast_routing_callbacks = {broadcast_routing_recv};
static const struct runicast_callbacks runicast_routing_callbacks = {recv_runicast_routing};
static const struct runicast_callbacks runicast_data_callbacks = {recv_runicast_data};
static const struct runicast_callbacks runicast_action_callbacks = {recv_runicast_action, sent_runicast_action, timeout_runicast_action};

/* -------- PROCESSES ------- */ 



PROCESS_THREAD(broadcast_routing_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_routing_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_action_conn);)
	
	PROCESS_BEGIN();
	printf("Broadcast routing begin ! \n");
		me.addr = linkaddr_node_addr;

	printf("Sensor Node Process runing \n");
	
	// Open connexions
	runicast_open(&runicast_routing_conn, 144, &runicast_routing_callbacks);
	runicast_open(&runicast_action_conn, 174, &runicast_action_callbacks);
	runicast_open(&runicast_data_conn, 164, &runicast_data_callbacks);
	
	broadcast_open(&broadcast_routing_conn, 129, &broadcast_routing_callbacks);
	
	static struct etimer hello_timer;
	while(1){
		if(me.dist_to_server != INT_MAX){
			etimer_set(&hello_timer, 120*CLOCK_SECOND);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&hello_timer));
			
			struct BROADCAST_ROUTING broadcast_routing_packet;
			broadcast_routing_packet.addr = me.addr; 
			broadcast_routing_packet.dist_to_server = me.dist_to_server;
			
			packetbuf_clear();
			packetbuf_copyfrom(&broadcast_routing_packet, sizeof(broadcast_routing_packet));
			
			broadcast_send(&broadcast_routing_conn);
			printf("Hello message sent \n");
		}
		else{
			PROCESS_WAIT_EVENT_UNTIL(0);
		}
	}
	
	PROCESS_END();
}




PROCESS_THREAD(test_serial, ev, data)
 {
   PROCESS_BEGIN();
	
   for(;;) {   
     PROCESS_YIELD();
     if(ev == serial_line_event_message) {
       printf("%s\n", (char *)data);
	   
	   if(strcmp((char *) data, "openValve")==0)
		   printf("OUVERTURE DES VALVES !!!\n");
     }
   }
   PROCESS_END();
 }
