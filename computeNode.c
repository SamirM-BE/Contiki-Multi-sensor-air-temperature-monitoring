#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include <stdio.h> /* For printf() */
#include <stdlib.h> //for malloc
#include <string.h> // for string operations
#include "random.h"
#include "dev/button-sensor.h"
#include "dev/cc2420/cc2420.h" // In order to recognize the variable cc2420_last_rssi

//-----------------------------------------------------------------   
PROCESS(blink_process, "blink example");
AUTOSTART_PROCESSES( & blink_process);

//Représente un paquet unicast, le contenu est encore à déterminer
struct unicastPacket
{
	signed char rss;
	char *msg;
	double slope;
};
typedef struct unicastPacket ucPacket;

static signed char rss_val; 
int n;
uint8_t x; 
uint8_t y; 
ucPacket hello;

//Fake vals des capteurs ///TODO : générer des nb random
int minutes[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30};
int vals[] = {24, 57, 18, 19, 70, 37, 11, 24, 82, 74, 12, 18, 12, 27, 31, 71, 62, 58, 45, 92, 2, 13, 24, 57, 18, 19, 70, 37, 11, 24};


//Retourne la pente obtenue àpd de la least square method
static double leastSquare(int minutes[], int vals[])
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
	return a;
}

//Fonction appelée quand un paquet broadcast a été reçu
static void broadcast_recv(struct broadcast_conn * c,const linkaddr_t * from) {
	
	printf("Broadcast message received from %d.%d in Computational Node: '%s'\n", from->u8[0], from->u8[1], (char * ) packetbuf_dataptr());
	
	//On récupère l'adresse du noeud qui nous envoit un paquet broadcast,
	//ça va nous servir pour lui envoyer des paquets unicast plus tard.
	x = from->u8[0];
	y = from->u8[1];
	  
	//RSS = puissance signal  
	rss_val = cc2420_last_rssi;
	hello.rss = rss_val+45; // d'après la documentation il faut toujours rajouter 45 au rssi
	printf("RSSI of Last Received Packet = %d dBm\n",hello.rss);
	hello.slope = leastSquare(minutes, vals); //slope = pente, Contiki ne print pas les floating type donc n'essayez pas.
}

//Fonction appelée quand un paquet unicast a été reçu
static void recv_uc(struct unicast_conn * c,
  const linkaddr_t * from) {
  printf("Unicast message received from %d.%d in Computational Node\n",
    from -> u8[0], from -> u8[1]);
}

//Fonction appelée quand un paquet unicast a été envoyé
static void sent_uc(struct unicast_conn * c, int status, int num_tx) {
  const linkaddr_t * dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if (linkaddr_cmp(dest, & linkaddr_null)) { // This function compares two Rime addresses and returns the result of the comparison. The function acts like the '==' operator and returns non-zero if the addresses are the same, and zero if the addresses are different.
    return; // This variable linkaddr_null contains the null Rime address. The null address is used in route tables to indicate that the table entry is unused. Nodes with no configured address  has the null address. Nodes with their node address set to the null address will have problems communicating with other nodes.
  }
  printf("Unicast message sent to %d.%d from Computational Node: status %d num_tx %d\n",
    dest->u8[0], dest->u8[1], status, num_tx);
}

static const struct broadcast_callbacks broadcast_call = {
  broadcast_recv
};
static struct broadcast_conn broadcast;

static const struct unicast_callbacks unicast_callbacks = {
  recv_uc,
  sent_uc
};
static struct unicast_conn uc;

//-----------------------------------------------------------------
PROCESS_THREAD(blink_process, ev, data) {

  PROCESS_EXITHANDLER(broadcast_close(&broadcast););
  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_call);
  unicast_open(&uc, 146, &unicast_callbacks);

  while (1) {
	linkaddr_t addr;  
	
	//Ce timer est essentiel pour traiter les paquets reçus, sans ça, ça ne fonctionne pas (IDK why)	
    static struct etimer et;
    etimer_set(&et,CLOCK_SECOND); //timer d'une seconde
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et)); //attend que la seconde expire

	hello.msg = malloc(5);
	hello.msg = "hello";
	
    packetbuf_copyfrom("Discover",10); //ce message est envoyé pour découvrir les nodes dans le réseau
    broadcast_send(&broadcast);
    printf("Broadcast message sent from Computational Node\n");

	///Envoi d'un paquet unicast contenant le message "hello", le rssi et la pente obtenue par la méthode des moindres carrés.///
    packetbuf_copyfrom(&hello, sizeof(hello));
    addr.u8[0] = x;	//x et y ont été récupéré dans la fonction broadcast_recv et
	addr.u8[1] = y; //correspondent à l'adresse d'un noeuf ayant envoyé un paquet broadcast
    if (!linkaddr_cmp(&addr,&linkaddr_node_addr)) { //linkaddr_node_addr contains the RIME address of the node
      packetbuf_copyfrom(&hello, sizeof(hello));
      unicast_send(&uc,&addr);
    }
  }

PROCESS_END();
}
