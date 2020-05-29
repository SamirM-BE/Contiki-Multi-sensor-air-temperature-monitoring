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
#include "mobileP1/linkedListChild.h" // Handle linkedlist
#include "mobileP1/linkedListHello.h" // Handle linkedlist
#include "lib/list.h" //for runicast
#include "lib/memb.h" //for runicast
#include "dev/serial-line.h"
#define MAX_RETRANSMISSIONS 10 //for runicast
#define NUM_HISTORY_ENTRIES 10 //for runicas

/* ------ PROCESSES DEFINITION ------ */

PROCESS(broadcast_routing_process, "routing broadcast");
PROCESS(test_serial, "Serial line test process");
PROCESS(action_process, "action process");
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
static struct Node me;

static struct runicast_conn runicast_routing_conn;
static struct runicast_conn runicast_lost_conn;
static struct runicast_conn runicast_data_conn;

static struct broadcast_conn broadcast_routing_conn;
static struct broadcast_conn broadcast_action_conn;
static struct broadcast_conn broadcast_lost_conn;


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
   
	//we update the timestamp of our child
	//const linkaddr_t ch = packet -> addr;
	//headChild = update(headChild, ch, clock_seconds());
	
	//as we are a sensor node, we have to set the forwarded boolean to true

	printf("packet forwared is equal to: %d\n",forwarded);

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

static void broadcast_action_recv(struct broadcast_conn * c,const linkaddr_t * from){
	printf("recevied action broadcast \n");
}

// the hello message we received in broadcast
static void broadcast_routing_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	printf("Hello message receved, ignored because  I'm border router ! \n");
}

/* ------ Static connexion's callbacks ------ */

static const struct broadcast_callbacks broadcast_action_callbacks = {broadcast_action_recv};
static const struct broadcast_callbacks broadcast_routing_callbacks = {broadcast_routing_recv};
static const struct runicast_callbacks runicast_routing_callbacks = {recv_runicast_routing};
static const struct runicast_callbacks runicast_data_callbacks = {recv_runicast_data};

/* -------- PROCESSES ------- */ 



PROCESS_THREAD(broadcast_routing_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_routing_conn);)
	
	PROCESS_BEGIN();
	printf("Broadcast routing begin ! \n");
	me.addr = linkaddr_node_addr;
	me.dist_to_server = 1;

	printf("Sensor Node Process runing \n");
	
	// Open connexions
	runicast_open(&runicast_routing_conn, 144, &runicast_routing_callbacks);
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
			char *tempo = (char *)data;
           char *a = malloc(2);
           char b;

           if(strlen(tempo)==3)
           {
               a[0] = tempo[0];
               b = tempo[2];
           }
           if(strlen(tempo)==4)
           {
               a[0] = tempo[0];
               a[1] = tempo[1];
               b = tempo[3];
           }
           int aa = atoi(a);
           int bb = b - '0';  
			
		   // preparing argument to pass to the process
		   linkaddr_t dest_add;
		   dest_add.u8[0] = aa;
		   dest_add.u8[1] = bb;
		   process_data_t arg = &dest_add;
		
		   // Start action process, sending action
		   process_start(&action_process, arg);
	   }
	   PROCESS_END();
	}
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
