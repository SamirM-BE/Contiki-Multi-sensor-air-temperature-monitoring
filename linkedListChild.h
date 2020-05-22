#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "net/rime/rime.h"
#include "contiki.h"
#define MAX_INTERVAL_KEEPALIVE 5

 

struct Child
{
    linkaddr_t addr;
    unsigned long timestamp;
    struct Child* next;
};

 

//find a link with given addr, return true if contains and false otherwise
bool contains(struct Child* head, const linkaddr_t addr) {

 


   //start from the first link
   struct Child* current = head;

 

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
//printf("Normalement il vient ici \n");
   //if data found, return true
   return true;
}

 

//insert link at the first location
struct Child* insert(struct Child* head, linkaddr_t addr, unsigned long timestamp) {
//printf("Rentre insert\n");

 

    //if the element is already in the list we do not add it again
    //we first need to veryfy if the list contains the address
    bool test = contains(head, addr);
    if(test){
        return head;
    }
    
    //create a child
    struct Child *child = (struct Child*) malloc(sizeof(struct Child));

 

    child->addr.u8[0] = addr.u8[0];
    child->addr.u8[1] = addr.u8[1];
    child->timestamp = timestamp;
   
    child->next = head; //point it to old first node
    head = child; //point first to new first node

 

    return head;
}

 

void printList(struct Child* head) {
   struct Child *ptr = head;
   printf("[ ");

 

   //start from the beginning
   while(ptr != NULL) {
      printf("\n Child: %d:%d",ptr->addr.u8[0],ptr->addr.u8[1]);
      ptr = ptr->next;
   }

 

   printf(" ] \n");
}

 

struct Child* update(struct Child * head, const linkaddr_t addr, unsigned long timestamp){
    //we first need to veryfy if the list contains the address
    bool test = contains(head, addr);

	if(test ==  true){ //if it correctly contains the element in the list
        //start from the first link
        struct Child* current = head;

 

        //if list is empty
        if(head == NULL) {
            return NULL;
        }
        
        //navigate through list
        while(current->addr.u8[0] != addr.u8[0] || current->addr.u8[1] != addr.u8[1]) {
            //go to next link
            current = current->next;
        }
    //we arrives at the right node so here we can make the modification
    current -> timestamp = timestamp;
    return head;
  }
}

 

//delete a link with given key
struct Child* delete(struct Child *head, const linkaddr_t addr) {
    
    
    

 

   //start from the first link
   struct Child* current = head;
   struct Child* prev = NULL;
   
   //case of linkedlist with one node
   if(current != NULL && current->next == NULL){
       
       if(current->addr.u8[0] == addr.u8[0] && current->addr.u8[1] == addr.u8[1]){
           current == NULL;
           return current;
       }
       
   }
   
   //case of mutiple node in the liked list
   while(current != NULL){
       
       if(current->addr.u8[0] == addr.u8[0] && current->addr.u8[1] == addr.u8[1]){
           if(prev == NULL){
              head = head -> next;
              return head;
           }
       }
       
       if(current->addr.u8[0] == addr.u8[0] && current->addr.u8[1] == addr.u8[1]){ // we found it
           struct Child* one = current; // the struct we'll return;
           struct Child* follow = current -> next; 
           current -> next = NULL; 
           free(current);
           current = NULL;
           prev -> next = follow;
           follow = NULL;
           return head;
       }
       
       prev = current;
       current = current -> next;
       
   }
   
}


struct Child* deleteOldChild(struct Child *head){
    struct Child *ptr = head;

 

    //start from the beginning
    while(ptr != NULL) {
        unsigned long interval = clock_seconds() - ptr->timestamp;
        if(interval > MAX_INTERVAL_KEEPALIVE){
            //Child seems probably down - remove from list
            printf("Delete Child %d.%d \n", ptr->addr.u8[0], ptr->addr.u8[1]);
            struct Child *newHead = delete(head, ptr->addr);
            return deleteOldChild(newHead);
        }
        if(ptr->next == NULL){
            return head;
        }
        else{
            ptr = ptr->next;
        }
    }

 

}


/* 
struct Child* deleteOldChild(struct Child *head){
    struct Child *ptr = head;
    struct Child *newHead = NULL;

 

    //start from the beginning
    while(ptr != NULL) {
        unsigned long interval = clock_seconds() - ptr->timestamp;
        if(interval > MAX_INTERVAL_KEEPALIVE){
            //Child seems probably down - remove from list
            printf("Delete Child %d.%d \n", ptr->addr.u8[0], ptr->addr.u8[1]);
            newHead = delete(head, ptr->addr);
            //return newHead;
        }
        
        /*
        if(ptr->next == NULL){
            return head;
        }
        else{
            ptr = ptr->next;
        }
         
        
    }
    
    return newHead;
}
*/

 

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
