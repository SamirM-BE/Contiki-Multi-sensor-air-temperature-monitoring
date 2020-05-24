#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "net/rime/rime.h"
#include "contiki.h"
#define MAX_INTERVAL_KEEPALIVE 5

 

struct Data
{
    int val;
    struct Child* next;
};

 

//insert link at the first location
struct Data* insertData(struct Data* head, int val) {
    
    //create a child
    struct Data *data = (struct Data*) malloc(sizeof(struct Data));

    data->val = val;
   
    data->next = head; //point it to old first node
    head = data; //point first to new first node

    return head;
}

 

void printListData(struct Data* data) {
   struct Data *ptr = data;
   printf("[ ");

 

   //start from the beginning
   while(ptr != NULL) {
      printf("\n Data: %d", ptr->val);
      ptr = ptr->next;
   }

   printf(" ] \n");
}

 
//is list empty
bool isEmptyData(struct Data* head) {
   return head == NULL;
}

 

//return the length of the linked list
int lengthData(struct Data* head) {
   int length = 0;
   struct Data *current;

 

   for(current = head; current != NULL; current = current->next) {
      length++;
   }

 

   return length;
}
