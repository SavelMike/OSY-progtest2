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

#define UNITSIZE 128

// Number of allocation units available for allocation
int alloc_units;
char *bitmap_p;
int bitmap_size;
void *mempool_p;

void   HeapInit    ( void * memPool, int memSize )
{	
	mempool_p = memPool;
	
	// Compute number of units in memPool
	int n_units = memSize / UNITSIZE;
	
	// Compute size of bitmap in bytes, 2 bits address 1 alloc_unit
	bitmap_size = (n_units + 7) / 4;
	
	// Compute how many units does bitmap take 
	int bitmap_units = (bitmap_size + UNITSIZE - 1) / UNITSIZE;
	
	// Compute how many units are available
	alloc_units = n_units - bitmap_units;
	
	bitmap_p = (char *)memPool + alloc_units * UNITSIZE;
	
	// Initilize bitmap with zeroes
	for (int i = 0; i < bitmap_size; i++) {
		bitmap_p[i] = 0;
	}
	
	// Mark unused bits of bitmap with 1s
	for (int i = alloc_units * 2; i < bitmap_size * 8; i++) {
		int bit_nr = i % 8;
		bitmap_p[i / 8] |= (1 << bit_nr);
	}

//	cout << "alloc_units: " << alloc_units << "\n";
}

/*
	Args:
	    unit_nr is number of unit to be marked as used
	    is_first this is 1 if unit_nr is first in allocated memory
	if is_first is 1, its odd bit has to be set to 1 
*/
void mark_unit_used(int unit_nr, int is_first) {
	unit_nr *= 2;
	int n_byte = unit_nr / 8;
	int n_bit = unit_nr % 8;
	bitmap_p[n_byte] |= (1 << n_bit);
	if (is_first != 0) {
		// Mark odd bit of first alloc_unit of sequence as 1
		bitmap_p[n_byte] |= (1 << (n_bit + 1)); 
	} else {
		// Mark odd bit of not first alloc_unit of sequence as 0
		bitmap_p[n_byte] &= ~(1 << (n_bit + 1));
	}
}

/*
	Args:
	    unit_nr is number of alloc_unit to be free
	     is_first this is true if unit_nr is first in allocated memory
	return value:
	     true if unit_nr was free
*/ 
bool mark_unit_free(int unit_nr, int is_first) {
	unit_nr *= 2;
	int n_byte = unit_nr / 8;
	int n_bit = unit_nr % 8;
	if (is_first) {
		if ((bitmap_p[n_byte] & (3 << n_bit)) == (3 << n_bit)) {
			bitmap_p[n_byte] &= ~(1 << (n_bit));
			return true;
		}
		return false;
	}
	
	if ((bitmap_p[n_byte] & (3 << n_bit)) == (1 << n_bit)){
		bitmap_p[n_byte] &= ~(1 << (n_bit));
		return true;
	}
	return false;
}

void * HeapAlloc   ( int    size )
{
	int zeros_in_row = 0;
	int start_unit;
	
	// Compute num of units required for this size
	int n_units = (size + UNITSIZE -1) / UNITSIZE;
	
	// Find free fragment for desired size
	for (int i = 0; i < bitmap_size; i++) {
		if ((bitmap_p[i] & 0x55) == 0x55) {
			// All alloc_units addressed by bitmap_p[i] are used
			zeros_in_row = 0;
			continue;
		}
#if 0
		hfghfgh
		if ((bitmap_p[i] & 0x55) == 0) {
			// All alloc_units addressed by bitmap_p[i] are free
			if (zeros_in_row == 0) {
				start_unit = (i * 8) / 2;
			}
			zeros_in_row++;
			if ((n_units - zeros_in_row) > 3) {
				zeros_in_row += 3;
			} else {
				zeros_in_row = n_units;
				// We found sufficient number of free alloc_units
				// Mark units as used
				for (int k = 0; k < zeros_in_row; k++) {
					mark_unit_used(start_unit + k, (k == 0) ? 1 : 0);
				}
				return (void*)((char *)mempool_p + start_unit * UNITSIZE);
			}
			continue;
		}
#endif
		for (int j = 0; j < 8; j += 2) {
			if (bitmap_p[i] & (1 << j)) {
				zeros_in_row = 0;
				continue;
			}
			if (zeros_in_row == 0) {
				// Remember start of fragment
				start_unit = (i * 8 + j) / 2;
			}
			zeros_in_row++;
			
			if (zeros_in_row == n_units) {
				// We found sufficient number of free alloc_units
				// Mark units as used
				for (int k = 0; k < zeros_in_row; k++) {
					mark_unit_used(start_unit + k, (k == 0) ? 1 : 0);
				}
				return (void*)((char *)mempool_p + start_unit * UNITSIZE);
			}
		}	
	}
	
	return nullptr; 
}

bool is_valid_address(void* blk) {
	int start_unit;
	
	if ((blk < mempool_p) || (blk >= bitmap_p)) {
		return false;
	}
	
	if ((((char*)blk - (char*)mempool_p) % UNITSIZE) != 0) {
		return false;
	}
	
	start_unit = ((char*)blk - (char*)mempool_p) / UNITSIZE;
	start_unit *= 2;
	int n_byte = start_unit / 8;
	int n_bit = start_unit % 8;
	
	if ((bitmap_p[n_byte] & (3 << n_bit)) != (3 << n_bit)) {
		return false;
	}
	
	return true;
}

bool   HeapFree    ( void * blk )
{
	if (!is_valid_address(blk)) {
		return false;
	}
	
	int start_unit = ((char*)blk - (char*)mempool_p) / UNITSIZE;
	int is_first = 1;
	
	while (mark_unit_free(start_unit, is_first)) {
		start_unit++;
		if (start_unit == alloc_units)
			break;
		is_first = 0;
	}
	
	return true;
}

void   HeapDone    ( int  * pendingBlk )
{
	pendingBlk[0] = 0;
	for (int i = 0; i < alloc_units; i++) {
		int n_byte = (i * 2) / 8;
		int n_bit = (i * 2) % 8;
		if ((bitmap_p[n_byte] & (3 << n_bit)) == (3 << n_bit)) {
			pendingBlk[0]++;
		}
	}
}

#ifndef __PROGTEST__
int main ( void )
{
  uint8_t       * p0, *p1, *p2, *p3, *p4;
  int             pendingBlk;
  static uint8_t  memPool[3 * 1048576];

  HeapInit ( memPool, 2097152 );
 
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

