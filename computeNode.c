#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include <stdio.h> /* For printf() */
#include <stdlib.h> //for malloc
#include <string.h> // for string operations
#include "random.h"
#include "dev/button-sensor.h"
#include "dev/cc2420/cc2420.h" // In order to recognize the variable cc2420_last_rssi

#include "lib/list.h" //for runicast
#include "lib/memb.h" //for runicast
#define MAX_RETRANSMISSIONS 4 //for runicast
#define NUM_HISTORY_ENTRIES 4 //for runicast

//-----------------------------------------------------------------   
PROCESS(blink_process, "blink example");
AUTOSTART_PROCESSES( & blink_process);

/*----------------------------------runicast initialization section-----------------------------------------*/
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

/*-------------------------------end of section runicast initilization----------------------------------------*/

/*-------------------------------section for packet message structure definition and initialization---------------------------------*/

//Représente un paquet unicast, le contenu est encore à déterminer
//TODO : Mettre cette structure dans un .h ?
struct unicastPacket
{
	signed char rss;
	char *msg;
        int min[30];
        int valSensor[30];
};
typedef struct unicastPacket ucPacket;

static signed char rss_val; 
int n;
uint8_t x; 
uint8_t y; 
ucPacket hello;
static double treshHold = 2.0;

/*---------------------------end of section for packet message structure definition and initialization-------------------------------------*/


/*----------------------------section related to the computation of sensor data---------------------*/

//This function returns 1 if the computed slope is above the treshhold, otherwise, it returns 0
static int leastSquare(int minutes[], int vals[])
{
    double sumx=0,sumx2=0,sumy=0,sumyx=0,a;
    int i;
    int n = 30;
	
    for(i=0; i<n; i++)
    {
        sumx=sumx+minutes[i];
        sumx2=sumx2+minutes[i]*minutes[i];
        sumy=sumy+vals[i];
        sumyx=sumyx+vals[i]*minutes[i];
    }
    a=(n*sumyx-sumx*sumy)/(n*sumx2-sumx*sumx);

    if(a > treshHold){
       return 1;
     }

     return 0;
}

/*---------------------------end fo section related to the computation of sensor data------------------*/


/*--------------------------Section runicast----------------------------------------------------*/



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
}

static const struct broadcast_callbacks broadcast_call = {
  broadcast_recv
};
static struct broadcast_conn broadcast;

/*------------------------------end of boradcast section for network discovery--------------------------------*/

//-----------------------------------------------------------------
PROCESS_THREAD(blink_process, ev, data) {

  PROCESS_EXITHANDLER(broadcast_close(&broadcast););
  PROCESS_BEGIN();

/*------section to open socket connection-------*/

  broadcast_open(&broadcast, 129, &broadcast_call);
  //unicast_open(&uc, 146, &unicast_callbacks);
  runicast_open(&runicast, 144, &runicast_callbacks);

/*----end of section to open socket conenction--*/

  //Cette partie est utilisée pour runicast mais j'ai pas encore compris à quoi elle pouvait servir
  /* OPTIONAL: Sender history */
  list_init(history_table);
  memb_init(&history_mem);

  while (1) {

    //linkaddr_t recv; 

    /*--------------timer handling section----------------*/ 
	
	//Ce timer est essentiel pour traiter les paquets reçus, sans ça, ça ne fonctionne pas (IDK why)	
    static struct etimer et;
    etimer_set(&et,CLOCK_SECOND); //timer d'une seconde
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et)); //attend que la seconde expire

    /*------------end of time handling section------------*/

    hello.msg = malloc(5);
    hello.msg = "hello";

    /*---------section to broadcast the discovery message--------*/

    
	
    packetbuf_copyfrom("Discover",10); //ce message est envoyé pour découvrir les nodes dans le réseau
    broadcast_send(&broadcast);
    printf("Broadcast message sent from Sensor Node\n");

    

    /*-------end of section to broadcast the discovery message----*/

    

   /*------section for runicast message sending-----------*/

   if(!runicast_is_transmitting(&runicast)) {
      linkaddr_t recv;
      
      //if the thresshold is exceeded, we send the OPEN VALVE message. 
      

      packetbuf_copyfrom("OPEN VALVE", 5);
      recv.u8[0] = 1;
      recv.u8[1] = 0;

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
