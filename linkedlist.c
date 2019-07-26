#include <stdlib.h>

#include "linkedlist.h"

struct linkedlist *linkedlist_new(void){
    struct linkedlist *list = malloc(sizeof(struct linkedlist));
    list->front = NULL;

    return list;
}

void linkedlist_add_front(struct linkedlist *list, void *data){
    if(!list->front){
        list->front = malloc(sizeof(struct node_t));
        list->front->next = NULL;
        list->front->data = data;
    }
}

void linkedlist_add(struct linkedlist *list, void *data_to_add){
    if(!list->front){
        linkedlist_add_front(list, data_to_add);
        return;
    }

    if(!data_to_add)
        return;

    struct node_t *current = list->front;

    while(current->next)
        current = current->next;

    struct node_t *add = malloc(sizeof(struct node_t));
    add->data = data_to_add;
    add->next = NULL;

    current->next = add;
}

int linkedlist_contains(struct linkedlist *list, void *data){
    if(!list->front)
        return 0;

    struct node_t *current = list->front;

    while(current->next){
        if(current->data == data)
            return 1;

        current = current->next;
    }

    return 0;
}

void linkedlist_delete(struct linkedlist *list, void *data_to_remove){
    if(!list->front)
        return;

    if(!data_to_remove)
        return;

    if(list->front->data == data_to_remove){
    //    printf("list->front %p\n", list->front);
        //free(list->front);
        struct node_t *freenode = list->front;
        list->front = list->front->next;
        free(freenode);
        return;
    }

    struct node_t *current = list->front;
    struct node_t *previous = NULL;

    while(current->next){
        previous = current;
        current = current->next;

        if(current->data == data_to_remove){
            /* We're at the node we want to remove,
             * modify connections to skip this one.
             */
            previous->next = current->next;
      //      printf("current %p\n", current);
            //free(current);
            current = NULL;
            return;
        }
    }
}

void linkedlist_free(struct linkedlist *list){
    printf("list->front %p\n", list->front);
    free(list->front);
    list->front = NULL;

    free(list);
    list = NULL;
}
