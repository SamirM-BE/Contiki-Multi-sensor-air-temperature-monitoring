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
#include "../PROJECT1/mobileP2/linkedList.h" // Handle linkedlist
#include "lib/list.h" //for runicast
#include "lib/memb.h" //for runicast
#define MAX_RETRANSMISSIONS 4 //for runicast
#define NUM_HISTORY_ENTRIES 4 //for runicast

//-----------------------------------------------------------------   
PROCESS(sensor_node_process, "sensor node process");
PROCESS(openValve_process, "open the valve for 10 minutes");
AUTOSTART_PROCESSES(&sensor_node_process);
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
struct BROADCAST_PACKET_LOST {
        linkaddr_t addr;
};

//answer of a new potential node for lonely child :'(
struct RUNICAST_PACKET_LOST {
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
};
typedef struct Parent Parent;

/* ----- STATIC VARIABLES -------- */
static struct Child *head = NULL;
static Parent parent;
static int dist_to_server = INT_MAX;
static int clock_s = 1; //our clock_s for the minute axis to send to the computational node


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

// Receivred routing runicast
static void recv_runicast_routing(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
}

static void sent_runicast_routing(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){
	printf("Runicast routing sent \n");
}

static void timeout_runicast_routing(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast routing timeout \n");
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
	
    if(dist_to_server == INT_MAX){ // equivalent to say that the node does not have parent yet and just spawned in an existing network
        //here we set the time, still have doubt regarding whether or not it will work in an outside method like that, maybe we should call a process to handle the timer?
        //in the meantime we retrieve every received packet in on the channel and insert it in the linked list
        
        int rss;
        static signed char rss_val; 
    
        rss_val = cc2420_last_rssi; //RSS = Signal Strength
        rss = rss_val+45; // Add 45 to RSS - read in documentation
        // at this step rss contains the last rss of the packet so we insert this value with the corresponding address of the sender int eh linked list
        // we fetch the received packet
        //struct BROADCAST_ROUTING routing = (struct BROADCAST_ROUTING*) packetbuf_dataptr();
        //const linkaddr_t newPotentialParent = {{from->u8[0], from->u8[1]}};
         //headHello = insertHello(headHello, newPotentialParent, rss, routing -> dist_to_server);
         
         //likaddr_t addr = biggestRSS(headHello);
         //Parent parent = addr;
         //dist_to_server = parent/distToserver +1 ;
        //processe wait end, the timer is finished
        
        //pas oublier à la fin de ce processus de free la list des hello histoire de pas créer des memory leak
    }
}

// broadcast message received when another node lost his parent because we are here for solidarity :D  
static void broadcast_lost_recv(struct broadcast_conn * c,const linkaddr_t * from) {
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

PROCESS_THREAD(sensor_node_process, ev, data)
{
	
	// Handling exit connexions
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_routing_conn);)
	PROCESS_EXITHANDLER(broadcast_close(&broadcast_lost_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_routing_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_lost_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_data_conn);)
	PROCESS_EXITHANDLER(runicast_close(&runicast_action_conn);)
	
	PROCESS_BEGIN(); 
	printf("Sensor Node Process runing \n");
	
	resetParent();
	
	// Open connexions
	broadcast_open(&broadcast_routing_conn, 129, &broadcast_routing_callbacks);
	broadcast_open(&broadcast_lost_conn, 139, &broadcast_lost_callbacks);
	runicast_open(&runicast_routing_conn, 144, &runicast_routing_callbacks);
	runicast_open(&runicast_lost_conn, 154, &runicast_lost_callbacks);
	runicast_open(&runicast_data_conn, 164, &runicast_data_callbacks);
	runicast_open(&runicast_action_conn, 174, &runicast_action_callbacks);
	
	PROCESS_END();
}