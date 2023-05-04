#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace std;
#endif /* __PROGTEST__ */

struct BFLElement {
	// Pointer to next element
	uint32_t next;
	// Address
	uint32_t address;
};

struct BFLElement* usedBuddyList[32];
struct BFLElement* freeBuddyList[32];

struct BFLElement allMem;

int maxOrder;

#define MINBUDDYSIZE 32

#if 0
void BuddyUsedListInsert(union Block* blk, int order)
{
	union Block* head = usedBuddyList[order];
	if (head == NULL) {
		usedBuddyList[order] = blk;
		return;
	}

	while (1) {
		union Block* next = head->buddyFreeListBlock.next;
		assert(head->buddyFreeListBlock.address < blk->buddyFreeListBlock.address);

		if ((next == NULL) || (next->buddyFreeListBlock.address > blk->buddyFreeListBlock.address)) {
			// Insert blk after head before next
			head->buddyFreeListBlock.next = blk;
			blk->buddyFreeListBlock.next = head;
			break;
		}
		head = next;
	}
}
#endif

void PrintBuddyList(struct BFLElement** buddyList, const char* msg)
{
	cout << msg << "\n"; 
	for (int i = 0; i < 32; i++) {
		if (buddyList[i] == NULL) {
			continue;
		}
		struct BFLElement* head = buddyList[i];
		cout << "buddy size = " << (1 << i) << ", addresses: ";
		while (head != NULL) {
			cout << " " << head->address;
			head = head->next;  
		}
		cout << "\n";
	}
}

#if 0
void BuddyFreeListInsert(union Block* blk, int order)
{
	union Block* head = freeBuddyList[order];
	if (head == NULL) {
		freeBuddyList[order] = blk;
		return;
	}

	while (1) {
		union Block* next = head->buddyFreeListBlock.next;
		assert(head->buddyFreeListBlock.address < blk->buddyFreeListBlock.address);

		if ((next == NULL) || (next->buddyFreeListBlock.address > blk->buddyFreeListBlock.address)) {
			// Insert blk after head before next
			head->buddyFreeListBlock.next = blk;
			blk->buddyFreeListBlock.next = head;
			break;
		}
		head = next;
	}
}
#endif

void* memory;

struct BFLElement* reservedMemory;

uint32_t firstFreeBFLE;

void ReserveMemInit(int memSize)
{
	reservedMemory = memory;
	int nrBFLElements = memSize / MINBUDDYSIZE;
	int reservedMemSize = nrBFLElements * sizeof(struct BFLElement); 
	int power = 1;
	
	
	while (power < reservedMemSize) {
		power *= 2;
	}
	reservedMemSize = power;
	firstFreeBFLE = 0;
	for (int i = 0; i < (nrBFLElements - 1); i++) {
		reservedMemory[i].next = i + 1;
	}
	reservedMemory[nrBFLElements - 1].next = 0xffffffff;
}

struct BFLElement* AllocBFLE(void)
{
	if (firstFreeBFLE == 0xffffffff) {
		return NULL;
	}
	struct BFLElement* res = reservedMemory + firstFreeBFLE;
	firstFreeBFLE = res->next;

	return res;
}


void* FreeBFLE(struct BFLElement* el)
{
	el->next = firstFreeBFLE;
	firstFreeBFLE = el - reservedMemory;
}

void   HeapInit    ( void * memPool, int memSize )
{
	memory = memPool;
	int power = 1;
	int order = 0;	
	
	while (power < memSize) {
		power *= 2;
		order++;
	}


	// Init memory and call function for initialization
	PrintBuddyList(freeBuddyList, "Free buddy list");	
	
	PrintBuddyList(usedBuddyList, "Used buddy list");
       
}

void* BuddyAlloc(int order) {
	struct BFLElement* tmp;
	struct BFLElement* cur;
	struct BFLElement* prev;

	for (int i = order; i < maxOrder; i++) {
		if (freeBuddyList[i] == NULL) {
			continue;
		}
		if (i == order) {
			  tmp = freeBuddyList[i];
			  freeBuddyList[i] = tmp->next;
			  // Insert tmp to used buddy list
			  cur = usedBuddyList[i];  
			  while (cur != NULL) {
				  if (cur->address < tmp->address) {
					  prev = cur;
					  cur = cur->next;
					  continue;
				  }
				  break;
			  }
			  tmp->next = prev->next;
			  prev->next = tmp;
			  return (char*)memory + (1 << order) * tmp->address;
		}
	}
	return NULL;
}

void * HeapAlloc   ( int    size )
{
	return NULL;
}
bool   HeapFree    ( void * blk )
{
	return false;
}
void   HeapDone    ( int  * pendingBlk )
{
  /* todo */
}

#ifndef __PROGTEST__
int main ( void )
{
  uint8_t       * p0, *p1, *p2, *p3, *p4;
  int             pendingBlk;
  static uint8_t  memPool[3 * 1048576];

  HeapInit ( memPool, 2 * 1048576);
  return 0;
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 512000 ) ) != NULL );
  memset ( p0, 0, 512000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 511000 ) ) != NULL );
  memset ( p1, 0, 511000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 26000 ) ) != NULL );
  memset ( p2, 0, 26000 );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 3 );


  HeapInit ( memPool, 2097152 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p1, 0, 250000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p2, 0, 250000 );
  assert ( ( p3 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p3, 0, 250000 );
  assert ( ( p4 = (uint8_t*) HeapAlloc ( 50000 ) ) != NULL );
  memset ( p4, 0, 50000 );
  assert ( HeapFree ( p2 ) );
  assert ( HeapFree ( p4 ) );
  assert ( HeapFree ( p3 ) );
  assert ( HeapFree ( p1 ) );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p1, 0, 500000 );
  assert ( HeapFree ( p0 ) );
  assert ( HeapFree ( p1 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 0 );


  HeapInit ( memPool, 2359296 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p1, 0, 500000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p2, 0, 500000 );
  assert ( ( p3 = (uint8_t*) HeapAlloc ( 500000 ) ) == NULL );
  assert ( HeapFree ( p2 ) );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 300000 ) ) != NULL );
  memset ( p2, 0, 300000 );
  assert ( HeapFree ( p0 ) );
  assert ( HeapFree ( p1 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 1 );


  HeapInit ( memPool, 2359296 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ! HeapFree ( p0 + 1000 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 1 );


  return 0;
}
#endif /* __PROGTEST__ */

