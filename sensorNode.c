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
PROCESS(openValve_process, "open the valve for 10 minutes");
AUTOSTART_PROCESSES(&broadcast_process);

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
	bool child_confirmation; //Set to true if unicast is send to confirm childhood
};
typedef struct unicastPacket ucPacket;

struct broadCastPacket
{
	bool hasPathToServer; //TODO hasPathToServer - rename
	char* msg;
	linkaddr_t src;
};
typedef struct broadCastPacket broadcastPacket;


struct Parent
{
	bool valid;
	int rss;
	linkaddr_t addr;
};
typedef struct Parent Parent;

typedef struct Child Child;
struct Child
{
	linkaddr_t addr;
	Child* next;
};

int n;

static Child childs;
static Parent parent;
static bool pathToServer = false;
static int clock = 1; //our clock for the minute axis to send to the computational node
static int toToggle = 0;  //variable boolean which will help to toggel the led for 10 minutes
static int isRunicastStarted = 0; //boolean to tell the system if the runicast process has started, important in order not to relaunch it at every boradcast message received



static ucPacket generate_random_data(ucPacket sendPacket){
  //we fill the packet with the value stored in the array
  //si on a finit de mesurer pendant 30 min, on remet le i à zéro
  //here we generate a random value between 1 and 100
  int max_val = 100;
  int min_val = 1;
  int random_val = random_rand();

   if(random_val < 0)
 {
   random_val *= -1;
 }
 
  int proposal_value = (random_val % max_val) + min_val;
  sendPacket.min = clock;
  sendPacket.valSensor = proposal_value;
  return sendPacket;
}


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
  bool child_conf = received_packet->child_confirmation;
  int min = received_packet->min;
  int val_sens = received_packet->valSensor;

  printf("Min: %d, sensor value: %d, child_conf:%d, msg: %s \n", min, val_sens, child_conf, msg);
  
  //ici je print caca en test mais il faudra en fait lancer la computation de la slope pour voir s'il faut envoyer ou pas au sensor node la commande d'ouvrir sa valve pendant 10 min
  //si on reçoit un message du type openValve on toggle la LED pour 10 minutes, sais pas encore comment faire les 10 min
  
  if(strcmp(msg,"openValve") ==  0){
     printf("TOOOOOOGLE\n");
     //we have to toggle the led for 10 minutes so we set the flag
     toToggle = 1;
     
     //}

     //ensuite on lance le process destiné à géré le truc de 10 minutes mais avant quand ce sera fait pas oublié de mettre en pause le processus qui le toggle toutes les minutes
     //surtout pas oublié de faire le process yield dns le processus qui gère le blinking régulier
     leds_off(LEDS_ALL);
     leds_toggle(LEDS_ALL);
     process_start(&openValve_process, NULL);
  }
   

  //printf("value of received packet :%d\n", msg->min);
  
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
	
	static signed char rss_val; 
	broadcastPacket* rcv = packetbuf_dataptr();
	bool serverPath = rcv->hasPathToServer;
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
      leds_on(LEDS_ALL);

  while (1) {

     //if we have to open the valve for 10 minutes coming from a statement from the computational node, we exit this process so we don't send value anymore to computational node
     //this runicast activity will be resumed by the openValve process as soon as the 10 minutes delay will be done
     //we make sure we turn off all the leds before
      if(toToggle ==  1){
        PROCESS_EXIT();
      }

     


    /*--------------timer handling section----------------*/ 
	
    // Timer handling the rate at which we'll send the runicast message with fake sensor data	
    static struct etimer etRunicast;
    etimer_set(&etRunicast,CLOCK_SECOND); //timer d'une seconde
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etRunicast)); //attend que la seconde expire

    //as soon as the time has expired, we can turn off the leds, that means we have finished the sampling

    leds_toggle(LEDS_ALL);//après 5 sec on éteint la led, normalement c'est 10 minutes mais pour test on laisse 5 sec
    
     ucPacket sendPacket1; //Packet to send
     ucPacket sendPacket = generate_random_data(sendPacket1);
     printf("coucou \n");
     printf("Packet: %d %d \n", sendPacket.min, sendPacket.valSensor);


     //the clock is used to tell to the receiver to which time corresponds the sensor value it receives, it has to be under generate random data in order to begin at 1
    clock++;

    if(clock == 31){
      clock = 1;
    }
    sendPacket.msg = "C"; //We send him the letter C representing the COMPUTE operation
    sendPacket.child_confirmation = true;

   
   if(!runicast_is_transmitting(&runicast)) {
      linkaddr_t recv;

      packetbuf_copyfrom( &sendPacket, sizeof(sendPacket));
      recv.u8[0] = parent.addr.u8[0];
      recv.u8[1] = parent.addr.u8[1];

      printf("%u.%u: sending runicast to parent address %u.%u - Packet %d %d \n",
	     linkaddr_node_addr.u8[0],
	     linkaddr_node_addr.u8[1],
	     recv.u8[0],
	     recv.u8[1], sendPacket.min, sendPacket.valSensor);

      runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
    }

  }

PROCESS_END();
}


PROCESS_THREAD(openValve_process, ev, data)
{
  PROCESS_BEGIN(); 
  printf("PROCESS LANCE\n");

  static struct etimer etLed;
  etimer_set(&etLed,600*CLOCK_SECOND); //timer de 50 secondes

     //on étient d'abord toutes les LED
     //leds_off(LEDS_ALL);
     //ensuite on les allumes

  leds_toggle(LEDS_ALL);

  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etLed)); //attend que la seconde expire

  leds_off(LEDS_ALL);//après 5 sec on éteint la led, normalement c'est 10 minutes mais pour test on laisse 5 sec 
  toToggle = 0; //we reset this flag to 0 in order to let the normal toggling process resuming its task
  process_start(&runicast_process, NULL);
  isRunicastStarted = 1; //we don't forget to set the isRunicastStarted variable to 0
  PROCESS_EXIT();//ensuite on exit le process sinon il va toggle toutes les 5 secondes car le process restera actif pour tjs

  PROCESS_END();
}


PROCESS_THREAD(broadcast_process, ev, data)
{
  PROCESS_BEGIN(); 
  printf("PROCESS LANCE\n");
  parent.addr.u8[0] = 0;
  parent.addr.u8[1] = 0;
  parent.rss = INT_MIN;
  parent.valid = false;

  broadcast_open(&broadcast, 129, &broadcast_call);

    while (1) {
	
    //Timer handling the rate at which we'll send the DISCOVER message	
    static struct etimer etBroadcast;
    etimer_set(&etBroadcast,10*CLOCK_SECOND); //we send the message every 10 seconds
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etBroadcast)); //Wait for timer to end
   
    broadcastPacket packet;
    packet.src.u8[0] = linkaddr_node_addr.u8[0];
    packet.src.u8[1] = linkaddr_node_addr.u8[1];
    packet.msg = "Discover";
    packet.hasPathToServer = pathToServer;	
    packetbuf_copyfrom(&packet, sizeof(packet)); //Message to send to all reachable nodes
    broadcast_send(&broadcast);
    printf("Broadcast message sent from Sensor Node\n");

  }

  PROCESS_END();
}
