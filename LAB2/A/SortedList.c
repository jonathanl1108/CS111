// //
// //  SortedList.c
// //  lab2_list
// //
// //  Created by Jonathan.
// //  Copyright © 2019-2021 Jonathan. All rights reserved.
// //

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include "SortedList.h"

/**
 * SortedList_insert ... insert an element into a sorted list
 *
 *	The specified element will be inserted in to
 *	the specified list, which will be kept sorted
 *	in ascending order based on associated keys
 *
 * @param SortedList_t *list ... header for the list
 * @param SortedListElement_t *element ... element to be added to the list
 */
void SortedList_insert(SortedList_t *list, SortedListElement_t *element) 
{
    // base case 
	if (list == NULL) {
        return;
	}

	if (list->next == NULL) {
        
        /* critical section */
        if (opt_yield & INSERT_YIELD) {
          	sched_yield();
        }

		list->next = element;
		element->prev = list;
		element->next = NULL;
        return;
	}
	SortedListElement_t* curr = list;
	SortedListElement_t* curr_next = list->next;
	
   	while (curr_next != NULL && strcmp(element->key, curr_next->key) >= 0 ){
        curr = curr_next;
        curr_next = curr_next->next;
    } 

    if (opt_yield & INSERT_YIELD) 
    {
        sched_yield();
	}

    if( curr_next != NULL )// in middle case
    {
        // reconnect 
        curr->next = element;
        element->prev = curr;
        element->next = curr_next;
        curr_next->prev = element;
    
    }else
    {
        curr->next = element;
		element->prev = curr;
		element->next = NULL;
    }
	
   
}
/**
 * SortedList_delete ... remove an element from a sorted list
 *
 *	The specified element will be removed from whatever
 *	list it is currently in.
 *
 *	Before doing the deletion, we check to make sure that
 *	next->prev and prev->next both point to this node
 *
 * @param SortedListElement_t *element ... element to be removed
 *
 * @return 0: element deleted successfully, 1: corrtuped prev/next pointers
 *
 */
int SortedList_delete(SortedListElement_t *element)
{
	if (element == NULL || element->key == NULL) 
        return 1;

    SortedListElement_t* killme_prv = element->prev;
    SortedListElement_t* killme_next = element->next;

    /* critical section */
    if (opt_yield & DELETE_YIELD) {
        	sched_yield();
    }

    if (killme_prv != NULL) 
    {
        // do idenity check in case of corrupted list 
        //identity theft is not a joke,JIM
        if (killme_prv->next == element)
            killme_prv->next= killme_next;
       else {
           return 1; 
        }
    }

    if (killme_next != NULL) 
    {
        // do idenity check in case of corrupted list 
        //identity theft is not a joke,JIM
        if( killme_next->prev == element)
           killme_next->prev=killme_prv;
        else
            return 1;
    }

    return 0;
}

/**
 * SortedList_lookup ... search sorted list for a key
 *
 *	The specified list will be searched for an
 *	element with the specified key.
 *
 * @param SortedList_t *list ... header for the list
 * @param const char * key ... the desired key
 *
 * @return pointer to matching element, or NULL if none is found
 */


SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) 
{
    if( !key) // possibally corrupted the head implementation
        return NULL;
    if (list == NULL)
        return NULL;

    SortedListElement_t* curr = list->next;

    while (curr != NULL) {
        if (opt_yield & LOOKUP_YIELD)
            sched_yield();
        if (strcmp(key, curr->key) == 0) 
            return curr;
        // get next 
       	curr = curr->next;
    }
     //cant find any
    return NULL;
}

/**
 * SortedList_length ... count elements in a sorted list
 *	While enumeratign list, it checks all prev/next pointers
 *
 * @param SortedList_t *list ... header for the list
 *
 * @return int number of elements in list (excluding head)
 *	   -1 if the list is corrupted
 */

int SortedList_length(SortedList_t *list) 
{
    if (list == NULL)
        return -999;

    int count = 0;
    SortedListElement_t* curr = list->next;

    while (curr != NULL) {
        if (opt_yield & LOOKUP_YIELD) 
          	sched_yield();
        curr = curr->next;
        count++;
    }
    return count;
}