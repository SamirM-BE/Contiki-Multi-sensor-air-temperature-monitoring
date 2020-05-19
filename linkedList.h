#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "net/rime/rime.h"
#include "contiki.h"

struct Child
{
	linkaddr_t addr;
	struct Child* next;
};


//insert link at the first location
struct Child* insert(struct Child* head, linkaddr_t addr) {
//printf("Rentre insert\n");

   //create a child
   struct Child *child = (struct Child*) malloc(sizeof(struct Child));

   child->addr.u8[0] = addr.u8[0];
   child->addr.u8[1] = addr.u8[1];
   
   //printf("Val de addr de child %d and %d \n : ", child->addr.u8[0], child->addr.u8[1]);

   //point it to old first node
   child->next = head;

   //point first to new first node
   head = child;
   //printf("Val de addr de head %d and %d \n : ", head->addr.u8[0], head->addr.u8[1]);
   return head;
}

void printList(struct Child* head) {
   struct Child *ptr = head;
   printf("\n[ ");

   //start from the beginning
   while(ptr != NULL) {
      printf("TRAVERSE %d:%d \n",ptr->addr.u8[0],ptr->addr.u8[1]);
      ptr = ptr->next;
   }

   printf(" ] \n");
}

//find a link with given addr, return true if contains and false otherwise
bool contains(struct Child* head, const linkaddr_t addr) {


   //start from the first link
   struct Child* current = head;

   //if list is empty
   if(head == NULL) {
      return false;
   }
   
   printf("val de curr %d:%d and val de head %d:%d\n", head->addr.u8[0],head->addr.u8[1], addr.u8[0] , addr.u8[1] );

   //navigate through list
   while(current->addr.u8[0] != addr.u8[0] && current->addr.u8[1] != addr.u8[1]) {
  
  printf("RENTRE WHILE\n");

      //if it is last node
      if(current->next == NULL) {
         return false;
      } else {
         //go to next link
         current = current->next;
      }
   }      
printf("Normalement il vient ici \n");
   //if data found, return true
   return true;
}

//delete a link with given key
struct Child* delete(struct Child *head, const linkaddr_t addr) {

   //start from the first link
   struct Child* current = head;
   struct Child* previous = NULL;

   //if list is empty
   if(head == NULL) {
      return NULL;
   }

   //navigate through list
   while(current->addr.u8[0] != addr.u8[0] && current->addr.u8[1] != addr.u8[1]){

      //if it is last node
      if(current->next == NULL) {
         return NULL;
      } else {
         //store reference to current link
         previous = current;
         //move to next link
         current = current->next;
      }
   }

   //found a match, update the link
   if(current == head) {
      //change first to point to next link
      head = head->next;
   } else {
      //bypass the current link
      previous->next = current->next;
   }    

   return current;
}

//is list empty
bool isEmpty(struct Child* head) {
   return head == NULL;
}
//return the length of the linked list
int length(struct Child* head) {
   int length = 0;
   struct Child *current;

   for(current = head; current != NULL; current = current->next) {
      length++;
   }

   return length;
}
