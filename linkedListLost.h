#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "net/rime/rime.h"
#include "contiki.h"
#include <limits.h> // for the min value used to find the biggest rss in the list for the discovery of our new parent

struct Lost
{
	linkaddr_t addr;
	int rss;
	int dist_to_server;
	struct Lost* next;
};

//find a link with given addr, return true if contains and false otherwise
bool containsLost(struct Lost* head, const linkaddr_t addr) {


   //start from the first link
   struct Lost* current = head;

   //if list is empty
   if(head == NULL) {
      return false;
   }
   
   //printf("val de curr %d:%d and val de head %d:%d\n", head->addr.u8[0],head->addr.u8[1], addr.u8[0] , addr.u8[1] );

   //navigate through list
   while(current->addr.u8[0] != addr.u8[0] || current->addr.u8[1] != addr.u8[1]) {
  
      //printf("RENTRE WHILE\n");

      //if it is last node
      if(current->next == NULL) {
         return false;
      } else {
         //go to next link
         current = current->next;
      }
   }      
   //if data found, return true
   return true;
}

//insert link at the first location
struct Lost* insertLost(struct Lost* head, linkaddr_t addr, int rss, int dist_to_server) {
//printf("Rentre insert\n");

    //if the element is already in the list we do not add it again
    //we first need to veryfy if the list contains the address
    bool test = containsLost(head, addr);
    if(test == true){
        return head;
    }
   //create a child
   struct Lost *child = (struct Lost*) malloc(sizeof(struct Lost));

   child->addr.u8[0] = addr.u8[0];
   child->addr.u8[1] = addr.u8[1];
   child->rss = rss;

   //printf("Val de addr de child %d and %d \n : ", child->addr.u8[0], child->addr.u8[1]);

   //point it to old first node
   child->next = head;

   //point first to new first node
   head = child;
   //printf("Val de addr de head %d and %d \n : ", head->addr.u8[0], head->addr.u8[1]);
   return head;
}

void printListLost(struct Lost* head) {
   struct Lost *ptr = head;
   printf("\n[ ");

   //start from the beginning
   while(ptr != NULL) {
      printf("ADDRESS %d:%d RSS %d DIST %d\n",ptr->addr.u8[0],ptr->addr.u8[1], ptr->rss, ptr-> dist_to_server);
      ptr = ptr->next;
   }

   printf(" ] \n");
}


//is list empty
bool isEmptyLost(struct Lost* head) {
   return head == NULL;
}
//return the length of the linked list
int lengthLost(struct Lost* head) {
   int length = 0;
   struct Lost *current;

   for(current = head; current != NULL; current = current->next) {
      length++;
   }

   return length;
}

//function that return the address corresponding to the biggest RSS, return NULL if the list is empty

struct Lost* biggestRssLost(struct Lost* head){
	
	//start from the first link
   struct Lost* current = head;
   struct Lost* best = NULL;
   
   //bigRSS = INT_MIN;
   //distSmall = INT_MAX;
   
   int bigRSS = INT_MIN;
   int distSmall = INT_MAX;
   //linkaddr_t bestAddr = {{0,0}};

   //if list is empty
   if(head == NULL) {
      return best;
   }
   
   while(current != NULL){
	   //si le RSS est plus grand
	   if(current->rss > bigRSS){
		   bigRSS = current -> rss; 
		   distSmall = current->dist_to_server;
		   best->addr.u8[0] = current->addr.u8[0];
		   best->addr.u8[1] = current->addr.u8[1];
	   }
	   else if(current->rss == bigRSS){ // si les deux RSS sont égaux, on regarde pour prendre le plus petit dist_to_server
		   if(current->dist_to_server <= distSmall){
		    bigRSS = current->rss;
			distSmall = current->dist_to_server;
			best->addr.u8[0] = current->addr.u8[0];
			best->addr.u8[1] = current->addr.u8[1];
		   }
	   }
	   current = current->next; // we move on to the next node
   }
   
   best-> rss = bigRSS;
   best-> dist_to_server = distSmall;

   //we return the best selected node
   return best;
}
 

