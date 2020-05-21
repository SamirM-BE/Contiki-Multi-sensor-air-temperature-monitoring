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
#define MAX_RETRANSMISSIONS 4 //for runicast
#define NUM_HISTORY_ENTRIES 4 //for runicast

//-----------------------------------------------------------------   
PROCESS(sensor_node_process, "sensor node process");
PROCESS(broadcast_routing_process, "routing broadcast");
PROCESS(broadcast_lost_process, "lost broadcast");
PROCESS(recv_hello_process, "recv hello process");
PROCESS(openValve_process, "open the valve for 10 minutes");
AUTOSTART_PROCESSES(&sensor_node_process, &broadcast_routing_process);
//-----------------------------------------------------------------

/*----------------------------------runicast section-----------------------------------------*/
/* OPTIONAL: Sender history.
 * Detects duplicate callbacks at receiving nodes.
 * Duplicates appear when ack messages are lost. */
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
//static struct Lost *headLost = NULL; // List of Lost packet received
static struct Node me;
static Parent parent;
static int clock_s = 1; //our clock_s for the minute axis to send to the computational node
static bool allow_recv_hello = false;
static bool allow_recv_lost = false;

static struct runicast_conn runicast_routing_conn;
static struct runicast_conn runicast_lost_conn;
static struct runicast_conn runicast_data_conn;
static struct runicast_conn runicast_action_conn;

static struct broadcast_conn broadcast_routing_conn;
static struct broadcast_conn broadcast_lost_conn;


/* ----- FUNCTIONS ------ */
static struct RUNICAST_DATA generate_random_data(struct RUNICAST_DATA sendPacket){
	//we fill the packet with the value stored in the array
	//si on a finit de mesurer pendant 30 min, on remet le i à zéro
	//here we generate a random value between 1 and 100
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

// the values of the sensor
static void recv_runicast_data(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
}

static void sent_runicast_data(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	printf("Runicast data sent \n");
}

static void timeout_runicast_data(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast data timeout \n");
	//parent down
	//TODO resetParent
	//TODO lostMode
}
 
// the action to open the valve for 10 minutes coming from the computational node or the server
static void recv_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
}

static void sent_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	printf("Runicast action sent \n");
}

static void timeout_runicast_action(struct runicast_conn *c, const linkaddr_t *from, uint8_t transmissions){
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

static void sent_runicast_routing(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	printf("Runicast routing sent \n");
}

static void timeout_runicast_routing(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast routing timeout \n");
	//TODO change parent
}

// Receivred lost runicast
static void recv_runicast_lost(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
}

static void sent_runicast_lost(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	printf("Runicast lost sent \n");
}

static void timeout_runicast_lost(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast lost timeout \n");
}

// the hello message we received in broadcast
static void broadcast_routing_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	if(me.dist_to_server == INT_MAX && allow_recv_hello){ // equivalent to say that the node does not have parent yet and just spawned in an existing network        
        printf("add addr to linked list \n");
		int rss;
        static signed char rss_val; 
    
        rss_val = cc2420_last_rssi; //RSS = Signal Strength
        rss = rss_val+45; // Add 45 to RSS - read in documentation

		struct BROADCAST_ROUTING *routing_packet = (struct BROADCAST_ROUTING*) packetbuf_dataptr();
		headHello = insertHello(headHello, routing_packet->addr, rss, routing_packet->dist_to_server);
		printListHello(headHello);
	}
}

// lost broadcast message received 
static void broadcast_lost_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	if(parent.valid){
		struct RUNICAST_LOST sendPacket;
		sendPacket.addr = me.addr;
		sendPacket.dist_to_server = me.dist_to_server;
		
		// Sending lost runicast with our dist_to_server
		packetbuf_clear();
		packetbuf_copyfrom(&sendPacket, sizeof(sendPacket));
		runicast_send(&runicast_lost_conn, from, MAX_RETRANSMISSIONS);
	}
}

/* ------ Static connexion's callbacks ------ */
static const struct broadcast_callbacks broadcast_routing_callbacks = {broadcast_routing_recv};
static const struct broadcast_callbacks broadcast_lost_callbacks = {broadcast_lost_recv};
static const struct runicast_callbacks runicast_routing_callbacks = {recv_runicast_routing, sent_runicast_routing, timeout_runicast_routing};
static const struct runicast_callbacks runicast_lost_callbacks = {recv_runicast_lost, sent_runicast_lost, timeout_runicast_lost};
static const struct runicast_callbacks runicast_data_callbacks = {recv_runicast_data, sent_runicast_data, timeout_runicast_data};
static const struct runicast_callbacks runicast_action_callbacks = {recv_runicast_action, sent_runicast_action, timeout_runicast_action};

/* -------- PROCESSES ------- */ 

PROCESS_THREAD(openValve_process, ev, data)
{
	PROCESS_BEGIN(); 
	printf("OpenValve process launched ! \n");

	static struct etimer etLed;
	etimer_set(&etLed,600*CLOCK_SECOND); //timer de 50 secondes

	//on étient d'abord toutes les LED
	//leds_off(LEDS_ALL);
	//ensuite on les allumes

	leds_toggle(LEDS_ALL);

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etLed)); //attend que la seconde expire

	leds_off(LEDS_ALL);//après 5 sec on éteint la led, normalement c'est 10 minutes mais pour test on laisse 5 sec 
	//toToggle = 0; //we reset this flag to 0 in order to let the normal toggling process resuming its task
	//process_start(&runicast_process, NULL);
	// isRunicastStarted = 1; //we don't forget to set the isRunicastStarted variable to 0
	PROCESS_EXIT();//ensuite on exit le process sinon il va toggle toutes les 5 secondes car le process restera actif pour tjs
	PROCESS_END();
}

PROCESS_THREAD(broadcast_lost_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_lost_conn);)
	PROCESS_BEGIN()
	printf("Process broadcast lost started \n");
	
	//Sending ONE broadcast Lost 
	struct BROADCAST_LOST sendPacket;
	sendPacket.addr = me.addr;
	
	packetbuf_clear();
	packetbuf_copyfrom(&sendPacket, sizeof(sendPacket));
	runicast_send(&runicast_routing_conn, &parent.addr, MAX_RETRANSMISSIONS);
		 
	
	static struct etimer allow_recv;
	etimer_set(&allow_recv, 5*CLOCK_SECOND);
	
	// headLost = NULL
	allow_recv_lost = true;
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&allow_recv));
	allow_recv_lost = false;
	
	//handle linked list
	//struct Lost *newLost = choice(headLost);
	//parent.addr = newLost->addr;
	//parent.rss = newLost->rss;
	//parent.dist_to_server = newLost->dist_to_server;
	//parent.valid = true;
	printf("New parent: %d.%d \n", parent.addr.u8[0], parent.addr.u8[1]);
	
	//send childConfirmation to parent
	struct RUNICAST_ROUTING sendPacketChild;
	sendPacketChild.addr = me.addr;
	sendPacketChild.isChild = true;
	
	packetbuf_clear();
	packetbuf_copyfrom(&sendPacketChild, sizeof(sendPacketChild));
	runicast_send(&runicast_routing_conn, &parent.addr, MAX_RETRANSMISSIONS);
		 
	PROCESS_END();
}

PROCESS_THREAD(broadcast_routing_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	
	PROCESS_BEGIN();
	printf("Broadcast routing begin ! \n");
	
	broadcast_open(&broadcast_routing_conn, 129, &broadcast_routing_callbacks);
	
	static struct etimer hello_timer;
	while(1){
		if(me.dist_to_server != INT_MAX){
			etimer_set(&hello_timer, 10*CLOCK_SECOND);
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

PROCESS_THREAD(recv_hello_process, ev, data){
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	PROCESS_BEGIN();
	printf("Process recv hello started \n");
	
	// Timer init
	static struct etimer allow_recv;
	etimer_set(&allow_recv, 20*CLOCK_SECOND);
	
	// Allow receive hello message
	headHello = NULL;
	allow_recv_hello = true;
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&allow_recv));
	allow_recv_hello = false;
	
	//handle linked list
	struct Hello *newHello = biggestRssHello(headHello);
	parent.addr = newHello->addr;
	parent.rss = newHello->rss;
	parent.dist_to_server = newHello->dist_to_server;
	parent.valid = true;
	me.dist_to_server = parent.dist_to_server +1 ;
	printf("Parent: %d.%d - new_dist: %d \n", parent.addr.u8[0], parent.addr.u8[1], me.dist_to_server);
	
	//send childConfirmation to parent
	struct RUNICAST_ROUTING sendPacket;
	sendPacket.addr = me.addr;
	sendPacket.isChild = true;
	
	packetbuf_clear();
	packetbuf_copyfrom(&sendPacket, sizeof(sendPacket));
	runicast_send(&runicast_routing_conn, &parent.addr, MAX_RETRANSMISSIONS);
		 
	printf("Exiting recv hello process \n");
	process_exit(&recv_hello_process);
	
	PROCESS_END()
}

PROCESS_THREAD(sensor_node_process, ev, data){
	// Init me node
	me.addr = linkaddr_node_addr;
	me.dist_to_server = INT_MAX;
	
	// Allow recv hello message to find parent and dist
	process_start(&recv_hello_process, NULL); 
	
	
	// Handling exit connexions
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_lost_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_routing_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_lost_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_data_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_action_conn);)
	
	PROCESS_BEGIN(); 
	printf("Sensor Node Process runing \n");
	
	resetParent();
	
	// Open connexions
	broadcast_open(&broadcast_lost_conn, 139, &broadcast_lost_callbacks);
	runicast_open(&runicast_routing_conn, 144, &runicast_routing_callbacks);
	runicast_open(&runicast_lost_conn, 154, &runicast_lost_callbacks);
	runicast_open(&runicast_data_conn, 164, &runicast_data_callbacks);
	runicast_open(&runicast_action_conn, 174, &runicast_action_callbacks);
		
	
	PROCESS_END();
}