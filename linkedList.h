#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "net/rime/rime.h"
#include "contiki.h"

 

struct ComputedNode
{
    linkaddr_t addr;
    unsigned long timestamp;
    struct ComputedNode* next;
};

 

//find a link with given addr, return true if contains and false otherwise
bool containsComputedNode(struct ComputedNode* head, const linkaddr_t addr) {

   //start from the first link
   struct ComputedNode* current = head;

 

   //if list is empty
   if(head == NULL) {
      return false;
   }
   
   //printf("val de curr %d:%d and val de head %d:%d\n", head->addr.u8[0],head->addr.u8[1], addr.u8[0] , addr.u8[1] );

   //navigate through list
   while(current->addr.u8[0] != addr.u8[0] || current->addr.u8[1] != addr.u8[1]) {
  
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
struct ComputedNode* insertComputedNode(struct ComputedNode* head, linkaddr_t addr, unsigned long timestamp) {
//printf("Rentre insert\n");

    //if the element is already in the list we do not add it again
    //we first need to veryfy if the list contains the address
    bool test = containsComputedNode(head, addr);
    if(test){
        return head;
    }
    
    //create a child
    struct ComputedNode *computedNode = (struct ComputedNode*) malloc(sizeof(struct ComputedNode));

 

    computedNode->addr.u8[0] = addr.u8[0];
    computedNode->addr.u8[1] = addr.u8[1];
    computedNode->timestamp = timestamp;
   
    computedNode->next = head; //point it to old first node
    head = computedNode; //point first to new first node

 

    return head;
}

 

void printListComputedNode(struct ComputedNode* head) {
   struct ComputedNode *ptr = head;
   printf("[ ");

 

   //start from the beginning
   while(ptr != NULL) {
      printf("\n ComputedNode: %d:%d",ptr->addr.u8[0],ptr->addr.u8[1]);
      ptr = ptr->next;
   }

 

   printf(" ] \n");
}

 

struct ComputedNode* updateComputedNode(struct ComputedNode * head, const linkaddr_t addr, unsigned long timestamp){
    //we first need to veryfy if the list contains the address
    bool test = containsComputedNode(head, addr);

	if(test ==  true){ //if it correctly contains the element in the list
        //start from the first link
        struct ComputedNode* current = head;

 

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
struct ComputedNode* deleteComputedNode(struct ComputedNode *head, const linkaddr_t addr) {
    
   //start from the first link
   struct ComputedNode* current = head;
   struct ComputedNode* prev = NULL;
   
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
           struct ComputedNode* one = current; // the struct we'll return;
           struct ComputedNode* follow = current -> next; 
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
   
   return head;
   
}
 

//is list empty
bool isEmptyComputedNode(struct ComputedNode* head) {
   return head == NULL;
}

 

//return the length of the linked list
int lengthComputedNode(struct ComputedNode* head) {
   int length = 0;
   struct ComputedNode *current;

   for(current = head; current != NULL; current = current->next) {
      length++;
   }

   return length;
}
