#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include <stdio.h> /* For printf() */
#include <stdlib.h> //for malloc
#include <string.h> // for string operations
#include "random.h" // for the generation of fake data sensor
#include "dev/button-sensor.h"
#include "dev/cc2420/cc2420.h" // In order to recognize the variable cc2420_last_rssi

#include "lib/list.h" //for runicast
#include "lib/memb.h" //for runicast
#define MAX_RETRANSMISSIONS 4 //for runicast
#define NUM_HISTORY_ENTRIES 4 //for runicast


//-----------------------------------------------------------------   
PROCESS(runicast_process, "runicast mechanism");
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
	signed char rss;
	char msg;
        int   min; //the minute corresponding ot the value sensor
        int   valSensor; // tab of valSensor
};
typedef struct unicastPacket ucPacket;

static signed char rss_val; 
int n;
uint8_t x; 
uint8_t y; 
ucPacket hello;




//Fake vals des capteurs ///TODO : générer des nb random
//static int minutes[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30};
//static int vals[] = {24, 57, 18, 19, 70, 37, 11, 24, 82, 74, 12, 18, 12, 27, 31, 71, 62, 58, 45, 92, 2, 13, 24, 57, 18, 19, 70, 37, 11, 24};

static int clock = 1; //our clock for the minute axis to send to the computational node
static int toToggle = 0;  //variable boolean which will help to toggel the led for 10 minutes

 


/*---------------------------Section destined to send data to computational node-----------------------------------*/


static void generate_random_data(){
  //we fill the hello packet with the value stored in the array

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
  hello.min = clock;
  hello.valSensor = proposal_value;
}




/*---------------------------End of section destined to send data to computational node---------------------------*/

/*---------------------------end of section for packet message structure definition and initialization-------------------------------------*/


static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
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

  printf("runicast message received from %d.%d, seqno %d\n",
	 from->u8[0], from->u8[1], seqno);

  //we print all the value of the array just to be sure
  
   

    char *msg;

  /* Grab the pointer to the incoming data. */
  msg = packetbuf_dataptr();
  
  //ici je print caca en test mais il faudra en fait lancer la computation de la slope pour voir s'il faut envoyer ou pas au sensor node la commande d'ouvrir sa valve pendant 10 min
  //si on reçoit un message du type openValve on toggle la LED pour 10 minutes, sais pas encore comment faire les 10 min
  if(strcmp(msg,"openValve") ==  0){
     printf("TOOOOOOGLE\n");
     //on étient d'abord toutes les LED
     leds_off(LEDS_ALL);
     //ensuite on les allumes
     
     
     //if(etimer_expired(&etLed)){
     leds_on(LEDS_ALL);
     //}

     //ensuite on lance le process destiné à géré le truc de 10 minutes mais avant quand ce sera fait pas oublié de mettre en pause le processus qui le toggle toutes les minutes
     //surtout pas oublié de faire le process yield dns le processus qui gère le blinking régulier
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

/*----------------------------end of runicast section-----------------------------------------------*/


/*----------------------------------------section broadcast for network discovery--------------------------------*/

//Fonction appelée quand un paquet broadcast a été reçu
static void broadcast_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	
	printf("Broadcast message received from %d.%d in Sensor Node: '%s'\n", from->u8[0], from->u8[1], (char * ) packetbuf_dataptr());
	
	//On récupère l'adresse du noeud qui nous envoit un paquet broadcast,
	//ça va nous servir pour lui envoyer des paquets unicast plus tard.
	x = from->u8[0];
	y = from->u8[1];
	  
	//RSS = puissance signal  
	rss_val = cc2420_last_rssi;
	hello.rss = rss_val+45; // d'après la documentation il faut toujours rajouter 45 au rssi
	printf("RSSI of Last Received Packet = %d dBm\n",hello.rss);

        //Since we've received our first broadcast message for discovery, we can start to send runicast message direclty to a receiver, not before because we don't know anyone yet
        process_start(&runicast_process, NULL);
}

static const struct broadcast_callbacks broadcast_call = {
  broadcast_recv
};
static struct broadcast_conn broadcast;

/*------------------------------end of boradcast section for network discovery--------------------------------*/
//-----------------------------------------------------------------
PROCESS_THREAD(runicast_process, ev, data) {

  PROCESS_EXITHANDLER(broadcast_close(&broadcast););
  PROCESS_BEGIN(); 

  printf("RUNICAST STARTED\n");

/*------section to open socket connection-------*/

  runicast_open(&runicast, 144, &runicast_callbacks);

/*----end of section to open socket conenction--*/

  //Cette partie est utilisée pour runicast mais j'ai pas encore compris à quoi elle pouvait servir
  /* OPTIONAL: Sender history */
  list_init(history_table);
  memb_init(&history_mem);

  while (1) {

    /*--------------timer handling section----------------*/ 
	
    ////Timer handling the rate at which we'll send the runicast message with fake sensor data	
    static struct etimer etRunicast;
    etimer_set(&etRunicast,CLOCK_SECOND); //timer d'une seconde
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etRunicast)); //attend que la seconde expire


    /*------------end of time handling section------------*/

    //hello.msg = malloc(5);
    hello.msg = 'C'; //We send him the letter C representing the COMPUTE operation
    generate_random_data();

    //the clock is used to tell to the receiver to which time corresponds the sensor value it receives
    clock++;

    if(clock == 31){
      clock = 1;
    }
    

   /*------section for runicast message sending-----------*/
   

   if(!runicast_is_transmitting(&runicast)) {
      linkaddr_t recv;

      packetbuf_copyfrom( &hello, sizeof(hello));
      recv.u8[0] = x;
      recv.u8[1] = y;

      printf("%u.%u: sending runicast to address %u.%u\n",
	     linkaddr_node_addr.u8[0],
	     linkaddr_node_addr.u8[1],
	     recv.u8[0],
	     recv.u8[1]);

      runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
    }

   /*------end of section for runicast message sending-----*/
  }

PROCESS_END();
}

/*-----------------------------the other process responsible for blinking-----------------------*/
PROCESS_THREAD(openValve_process, ev, data)
{
  PROCESS_BEGIN(); 
  printf("PROCESS LANCE\n");

  static struct etimer etLed;
  etimer_set(&etLed,2*CLOCK_SECOND); //timer de 2 secondes

  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etLed)); //attend que la seconde expire

  leds_off(LEDS_ALL);//après 5 sec on éteint la led, normalement c'est 10 minutes mais pour test on laisse 5 sec
  PROCESS_EXIT();//ensuite on exit le process sinon il va toggle toutes les 5 secondes car le process restera actif pour tjs

  PROCESS_END();
}
/*-------------------------------------------Process handling broadcasting------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data)
{
  PROCESS_BEGIN(); 
  printf("PROCESS LANCE\n");

  /*------section to open socket connection-------*/

  broadcast_open(&broadcast, 129, &broadcast_call);

  /*----end of section to open socket conenction--*/

    while (1) {

    /*--------------timer handling section----------------*/ 
	
    //Timer handling the rate at which we'll send the DISCOVER message	
    static struct etimer etBroadcast;
    etimer_set(&etBroadcast,10*CLOCK_SECOND); //we send the message every 10 seconds
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etBroadcast)); //attend que la seconde expire


    /*------------end of timer handling section------------*/
  

    /*---------section to broadcast the discovery message--------*/
   
	
    packetbuf_copyfrom("Discover",10); //ce message est envoyé pour découvrir les nodes dans le réseau
    broadcast_send(&broadcast);
    printf("Broadcast message sent from Sensor Node\n");
    

    /*-------end of section to broadcast the discovery message----*/

    
  }

  PROCESS_END();
}
