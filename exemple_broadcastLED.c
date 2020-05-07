/*
Yo Guys,
Ce fichier a juste pour vocation de noter nos "découvertes"
C'est juste pour pouvoir s'en inspirer plus tard, parce que c'était dans le main fichier et ça le surchargeait.
*/
/* Donc ici c'est quand on a réussi à broadcast des messages, et à faire clignoter une lumière différente selon
 * le message reçu.
 * 
*/
  
#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include <stdio.h> /* For printf() */
#include <stdlib.h> //for malloc
#include <string.h> // for string operations
#include "random.h"
#include "dev/button-sensor.h"


PROCESS(blink_process, "blink example");
AUTOSTART_PROCESSES(&blink_process);


int n;
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
	
  printf("broadcast message received from %d.%d: '%s'\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
		 
	if(strcmp((char *)packetbuf_dataptr(),"Hello")== 0){
		n = 1;
	}else{
		n = 0;
	}		 		
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
static int k=0;

//-----------------------------------------------------------------
PROCESS_THREAD(blink_process, ev, data)
{  
  static struct etimer et;
 
  PROCESS_EXITHANDLER(broadcast_close(&broadcast););
  PROCESS_BEGIN();
  
	broadcast_open(&broadcast, 129, &broadcast_call);
 
    while(1) {
	/* Do the rest of the stuff here. */
	/* Delay 2-4 seconds */
	etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	
	if(k%2==0)
	{
		packetbuf_copyfrom("Hello", 5);
		broadcast_send(&broadcast);
		k++;
	}
	else
	{
		packetbuf_copyfrom("Samir", 5);
		broadcast_send(&broadcast);
		k++;
	}
	printf("Broadcast message sent\n");

	
	if(n== 1){
		printf("n=1, dans le IF\n");
		leds_toggle(LEDS_GREEN);
	}else{
		printf("n=0, dans le ELSE\n");
		leds_toggle(LEDS_RED);
	}
	}
        PROCESS_END();
}
