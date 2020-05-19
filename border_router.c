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

#include "lib/list.h" //for runicast
#include "lib/memb.h" //for runicast
#define MAX_RETRANSMISSIONS 4 //for runicast
#define NUM_HISTORY_ENTRIES 4 //for runicast

//-----------------------------------------------------------------   
PROCESS(runicast_process, "runicast mechanism, handle the toggling of the light every 1 minute too");
PROCESS(broadcast_process, "broadcast mechanism");
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process);

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

/*-------------------------------section for packet message structure definition and initialization---------------------------------*/

//Représente un paquet unicast, le contenu est encore à déterminer
//TODO : Mettre cette structure dans un .h ?
struct unicastPacket
{
	char* msg;
	int min; //the minute corresponding ot the value sensor
	int valSensor; //valSensor
	bool confirmChild;
	bool actionValve;
};
typedef struct unicastPacket ucPacket;

struct broadCastPacket
{
	bool asPathToServer;
	linkaddr_t src;
};
typedef struct broadCastPacket broadcastPacket;


typedef struct Child Child;
struct Child
{
	linkaddr_t addr;
	Child* next;
};

int n;


//Fake vals des capteurs ///TODO : générer des nb random
//static int minutes[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30};
//static int vals[] = {24, 57, 18, 19, 70, 37, 11, 24, 82, 74, 12, 18, 12, 27, 31, 71, 62, 58, 45, 92, 2, 13, 24, 57, 18, 19, 70, 37, 11, 24};

static bool pathToServer = true;
static int isRunicastStarted = 0; //boolean to tell the system if the runicast process has started, important in order not to relaunch it at every boradcast message received


static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("RUNICAST RECEIVED\n");
  /* OPTIONAL: Sender history */
  struct history_entry *e = NULL;
  for(e = list_head(history_table); e != NULL; e = e->next) {
    if(linkaddr_cmp(&e->addr, from)) {
      break;
    }
  }
  if(e == NULL) {
    e = memb_alloc(&history_mem); /* Create new history entry */
    if(e == NULL) {
      e = list_chop(history_table); /* Remove oldest at full history */
    }
    linkaddr_copy(&e->addr, from);
    e->seq = seqno;
    list_push(history_table, e);
  } else {
    if(e->seq == seqno) {  /* Detect duplicate callback */
      printf("runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
	     from->u8[0], from->u8[1], seqno);
      return;
    }
    e->seq = seqno;  /* Update existing history entry */
  }

  printf("runicast message received from %d.%d, seqno %d\n",
	 from->u8[0], from->u8[1], seqno);

  /* Grab the pointer to the incoming data. */ 
  ucPacket* received_packet = (ucPacket*) packetbuf_dataptr();
  char* msg = received_packet->msg;
  bool child_conf = received_packet->confirmChild;
  int min = received_packet->min;
  int val_sens = received_packet->valSensor;
  bool actionValve = received_packet->actionValve;

  printf("Min: %d, sensor value: %d, child_conf:%d, msg: %s \n", min, val_sens, child_conf, msg); 
}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message sent to %d.%d, retransmissions %d\n",
	 to->u8[0], to->u8[1], retransmissions);
}
static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message timed out when sending to %d.%d, retransmissions %d\n",
	 to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_callbacks = {recv_runicast,
							     sent_runicast,
							     timedout_runicast};
static struct runicast_conn runicast;


static void broadcast_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	printf("Broadcast received \n");
	/*
	static signed char rss_val; 
	broadcastPacket* rcv = packetbuf_dataptr();
	bool serverPath = rcv->asPathToServer;
	char* msg = rcv->msg;
	int rss;
	printf("Broadcast message received from %d.%d in Sensor Node. Msg: '%s' - PathServer: '%d'\n", from->u8[0], from->u8[1], msg, serverPath);
	
	rss_val = cc2420_last_rssi; //RSS = Signal Strength
	rss = rss_val+45; // Add 45 to RSS - read in documentation
	printf("RSSI of Last Received Packet = %d dBm\n", rss);

	if(rss > parent.rss && serverPath){ //TODO check node is not already a child
		printf("new rssi better than %d dBm\n", parent.rss);
		parent.addr.u8[0] = from->u8[0];
		parent.addr.u8[1] = from->u8[1];
		parent.rss = rss;
		parent.valid = true;
		pathToServer = true;
	}
	else{
		printf("lower signal received or noth path to server detected !\n");
	}

	if(parent.valid){
		 printf("Parent: %d.%d - RSS %d dBm \n", parent.addr.u8[0], parent.addr.u8[1], parent.rss);
	
		if(isRunicastStarted==0){ //we launch the runicast process thread only if it is not started yet
			printf("ONLY ONCE");
			process_start(&runicast_process, NULL);
			isRunicastStarted = 1;
		}
	}
	*/
}

static const struct broadcast_callbacks broadcast_call = {
  broadcast_recv
};
static struct broadcast_conn broadcast;


//-----------------------------------------------------------------
PROCESS_THREAD(runicast_process, ev, data) {

  PROCESS_EXITHANDLER(broadcast_close(&broadcast););
  PROCESS_BEGIN(); 

  printf("RUNICAST STARTED\n");

  runicast_open(&runicast, 144, &runicast_callbacks);


  //Cette partie est utilisée pour runicast mais j'ai pas encore compris à quoi elle pouvait servir
  /* OPTIONAL: Sender history */
  list_init(history_table);
  memb_init(&history_mem); 
  
  //we turn on all the leds, turning on the leds means starting generation of fake data
  //leds_on(LEDS_ALL);

  PROCESS_WAIT_EVENT_UNTIL(0);

  PROCESS_END();
}


PROCESS_THREAD(broadcast_process, ev, data)
{
  PROCESS_BEGIN(); 
  printf("PROCESS LANCE\n");

  broadcast_open(&broadcast, 129, &broadcast_call);

	while (1) {
	
		//Timer handling the rate at which we'll send the DISCOVER message	
		static struct etimer etBroadcast;
		etimer_set(&etBroadcast,10*CLOCK_SECOND); //we send the message every 10 seconds
		
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etBroadcast)); //Wait for timer to end

		broadcastPacket packet;
		packet.src.u8[0] = linkaddr_node_addr.u8[0];
		packet.src.u8[1] = linkaddr_node_addr.u8[1];
		packet.asPathToServer = pathToServer;	
		packetbuf_copyfrom(&packet, sizeof(packet)); //Message to send to all reachable nodes
		broadcast_send(&broadcast);
		printf("Broadcast message sent from Border Router\n");

  }

  PROCESS_END();
}
