#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include <stdio.h> 
#include <stdlib.h> 
#include <stdbool.h> 
#include <string.h> 
#include <limits.h> 
#include "random.h" // fake data sensors
#include "dev/button-sensor.h"
#include "dev/cc2420/cc2420.h" // variable cc2420_last_rssi
#include "mobileP1/linkedListChild.h" 
#include "mobileP1/linkedListHello.h" 
#include "lib/list.h" //runicast
#include "lib/memb.h" //runicast
#define MAX_RETRANSMISSIONS 10 //runicast
#define NUM_HISTORY_ENTRIES 10 //runicas

/* ------ PROCESSES DEFINITION ------ */

PROCESS(sensor_node_process, "sensor node process");
PROCESS(broadcast_routing_process, "routing broadcast");
PROCESS(recv_hello_process, "recv hello process");
PROCESS(runicast_data_process, "send runicast data");
PROCESS(recv_action_process, "receive action process");
PROCESS(valve_process, "open the valve for 10 minutes");
AUTOSTART_PROCESSES(&sensor_node_process, &broadcast_routing_process);


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

struct BROADCAST_ACTION{
	linkaddr_t dest_addr;
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
static Parent parent;
static int clock_s = 1; //our clock_s for the minute axis to send to the computational node
static bool allow_recv_hello = false;
static int toToggle = 0; //when we received the instruction to toggle the LED

static struct runicast_conn runicast_routing_conn;
static struct runicast_conn runicast_data_conn;

static struct broadcast_conn broadcast_action_conn;
static struct broadcast_conn broadcast_routing_conn;


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
	
	printf("RECIVED RUNICAST DATA from %d.%d seq: %d\n",  from->u8[0], from->u8[1], seqno);
	
	struct RUNICAST_DATA *packet = packetbuf_dataptr();
   
	//we update the timestamp of our child
	const linkaddr_t ch = packet -> addr;
	headChild = update(headChild, ch, clock_seconds());
	
	//as we are a sensor node, we have to set the forwarded boolean to true
	packet->forwarded = true;
	
	linkaddr_t recv;
	recv.u8[0] = parent.addr.u8[0];
	recv.u8[1] = parent.addr.u8[1];
	
	// then we send the packet
	packetbuf_clear();
	packetbuf_copyfrom(packet, sizeof(struct RUNICAST_DATA));
	printf("Message form %d:%d forwareded to %d:%d\n",from->u8[0], from->u8[1], parent.addr.u8[0], parent.addr.u8[1]);
	runicast_send(&runicast_data_conn, &recv, MAX_RETRANSMISSIONS); //the second argument is the address of our parent wich is our parent

}

static void sent_runicast_data(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast data sent \n");
}

static void timeout_runicast_data(struct runicast_conn *c, const linkaddr_t *from, uint8_t retransmissions){
	printf("Runicast data timeout - Parent %d.%d down \n", parent.addr.u8[0], parent.addr.u8[1]);
	
	process_exit(&runicast_data_process);
	//process_exit(&recv_action_process);
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
	//process_exit(&recv_action_process);
	resetParent();
	process_start(&recv_hello_process, NULL);
}

static void broadcast_action_recv(struct broadcast_conn * c, const linkaddr_t * from){	
	struct BROADCAST_ACTION *packet = (struct BROADCAST_ACTION*) packetbuf_dataptr();
	
		if(packet->dist_to_server < me.dist_to_server){
			if(linkaddr_cmp(&packet->dest_addr, &me.addr) != 0 ){
				printf("I need to open my valve \n");
				if(process_is_running(&valve_process) == 0){
					toToggle = 1;
					process_start(&valve_process, NULL);
				}
			}
			else{
				printf("action received and transfered \n");
				packet->dist_to_server = me.dist_to_server;
				packetbuf_clear();
				packetbuf_copyfrom(packet, sizeof(struct BROADCAST_ACTION));
				broadcast_send(&broadcast_action_conn);
			}
		}
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
		
		if(me.dist_to_server == INT_MAX || routing_packet->dist_to_server < me.dist_to_server){
			headHello = insertHello(headHello, routing_packet->addr, rss, routing_packet->dist_to_server);
			printListHello(headHello);
		}
	}
}

/* ------ Static connexion's callbacks ------ */

static const struct broadcast_callbacks broadcast_routing_callbacks = {broadcast_routing_recv};
static const struct broadcast_callbacks broadcast_action_callbacks = {broadcast_action_recv};
static const struct runicast_callbacks runicast_routing_callbacks = {recv_runicast_routing, sent_runicast_routing, timeout_runicast_routing};
static const struct runicast_callbacks runicast_data_callbacks = {recv_runicast_data, sent_runicast_data, timeout_runicast_data};

/* -------- PROCESSES ------- */ 

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

PROCESS_THREAD(valve_process, ev, data){
	PROCESS_BEGIN(); 
	printf("OpenValve process launched ! \n");
	
	
	static struct etimer etLed;
	etimer_set(&etLed,600*CLOCK_SECOND); //timer de 60 secondes

	leds_toggle(LEDS_ALL);

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etLed)); //attend que la seconde expire

	leds_off(LEDS_ALL);//après 5 sec on éteint la led, normalement c'est 10 minutes mais pour test on laisse 5 sec 
	toToggle = 0; //we reset this flag to 0 in order to let the normal toggling process resuming its task
	//process_start(&runicast_process, NULL);
	// isRunicastStarted = 1; //we don't forget to set the isRunicastStarted variable to 0
	PROCESS_EXIT();//ensuite on exit le process sinon il va toggle toutes les 5 secondes car le process restera actif pour tjs
	PROCESS_END();
}

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
PROCESS_THREAD(sensor_node_process, ev, data){
	// Init me node
	me.addr = linkaddr_node_addr;
	me.dist_to_server = INT_MAX;
	
	// Handling exit connexions
	PROCESS_EXITHANDLER(runicast_close(&runicast_routing_conn);)
	
	PROCESS_BEGIN(); 
	printf("Sensor Node Process runing \n");
	
	resetParent();
	
	// Open connexions
	runicast_open(&runicast_routing_conn, 144, &runicast_routing_callbacks);
	

	// Allow recv hello message to find parent and dist
	process_start(&recv_hello_process, NULL); 

	PROCESS_END();
}

/* DATA PROCESS */
// Send data to parent every 60sec
PROCESS_THREAD(runicast_data_process, ev, data) {
	PROCESS_EXITHANDLER(runicast_close(&runicast_data_conn););
	PROCESS_BEGIN(); 
	
	// Init time shifting
	static struct etimer startTimer;
	etimer_set(&startTimer, (random_rand() % 30*CLOCK_SECOND));
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&startTimer)); 

	printf("RUNICAST DATA STARTED\n");
	
	runicast_open(&runicast_data_conn, 164, &runicast_data_callbacks);
  
	//we turn on all the leds, turning on the leds means starting generation of fake data
	leds_on(LEDS_ALL);

	while (1) {
		//if we have to open the valve for 10 minutes coming from a statement from the computational node, we exit this process so we don't send value anymore to computational node
		//this runicast activity will be resumed by the openValve process as soon as the 10 minutes delay will be done
		//we make sure we turn off all the leds before
		/*
		if(toToggle ==  true){
			PROCESS_EXIT(); // Change with process_exit() from another process
		}
		*/
	   
		static struct etimer etRunicast;
		etimer_set(&etRunicast,60*CLOCK_SECOND);
    
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etRunicast)); //attend que la seconde expire
		//Turn LED's OFF
		if(toToggle != 1){
			leds_toggle(LEDS_ALL);//après 5 sec on éteint la led, normalement c'est 10 minutes mais pour test on laisse 5 sec			
		}
	
		struct RUNICAST_DATA sendPacket1;
		struct RUNICAST_DATA sendPacket = generate_random_data(sendPacket1);
		sendPacket.forwarded = false; //peut être à enlevé car elle va crée des complications non?
		sendPacket.addr = me.addr;

		//the clock is used to tell to the receiver to which time corresponds the sensor value it receives, it has to be under generate random data in order to begin at 1
		clock_s++;
		if(clock_s == 31){
			clock_s = 1;
		}

		while(runicast_is_transmitting(&runicast_data_conn)){}
		/* OPTIONAL: Sender history */ //TODO test when moved before while(1)
		list_init(history_table);
		memb_init(&history_mem);
		
		packetbuf_clear();
		packetbuf_copyfrom( &sendPacket, sizeof(sendPacket));
		
		printf("Sending runicast to address %u.%u\n",parent.addr.u8[0],parent.addr.u8[1]);

		runicast_send(&runicast_data_conn, &parent.addr, MAX_RETRANSMISSIONS);
	}

	PROCESS_END();
}
