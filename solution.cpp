#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <semaphore.h>
#include "common.h"
using namespace std;
#endif /* __PROGTEST__ */

struct pte {
	uint32_t present:1;
	uint32_t bitW:1;
	uint32_t bitU:1;
	uint32_t swaped:1; // Normally unused 
	uint32_t unused2:1;
	uint32_t bitR:1;
	uint32_t bitD:1;
	uint32_t unused3:5; 
	uint32_t frameNumber:20;
};

// Two bit per slot
// 00 page is free
// 01 - page is used, not swappable, level1 page table or BitMap page
// 10 - page is used. not swappable, level2 page table
// 11 - page swappable
class CMemoryBitMap {
private:
	uint8_t* m_BitMap;
	uint32_t m_MapSize; // in bits
	uint8_t* m_SwapBitMap;
	uint32_t m_SwapSize;
	bool  (* m_readPage) ( uint32_t memFrame, uint32_t diskPage );
	bool  (* m_writePage) ( uint32_t memFrame, uint32_t diskPage );
	
public:
	CMemoryBitMap(void* memory, uint32_t mapSize,  uint32_t swapSize,
					   bool  (* readPage) ( uint32_t memFrame, uint32_t diskPage ),  
					   bool  (*writePage) ( uint32_t memFrame, uint32_t diskPage ))
	: m_BitMap((uint8_t*)memory), m_MapSize(mapSize), m_SwapSize(swapSize), m_readPage(readPage), m_writePage(writePage) 
	{
		// Use page number 0 for map of free used pages
		for (uint32_t i = 0; i < mapSize / 4; i++)
			m_BitMap[i] = 0;
		// Mark 0th page as used, not swappable 	
		m_BitMap[0] |= 1;
		// Initialize swap bitMap
		m_SwapBitMap = m_BitMap + mapSize / 4;
		for (uint32_t i = 0; i < swapSize / 8; i++) {
			m_SwapBitMap[i] = 0;
		}
	}
	
	bool   readPage ( uint32_t memFrame, uint32_t diskPage ) {
		return m_readPage(memFrame, diskPage);
	}
	
	uint32_t allocateSwapPage(void) {
		uint8_t mask;
		for (uint32_t i = 0; i  < m_SwapSize / 8; i++ ) {
			if (m_SwapBitMap[i] == 255)
				continue;
			for (int j  = 0; j < 8; j++) {
				mask = 1 << j;
				if (m_SwapBitMap[i] & mask)
					continue;
				// Mark the bit as used
				m_SwapBitMap[i] |= mask;
				return i * 8 + j;
			}	
		}
		
		// No free page
		return (uint32_t)-1;
	}
	
	void freeSwapPage(uint32_t pageNum) {
		uint8_t mask; 
		uint32_t index = pageNum / 8;
		int shift = pageNum % 8;
		mask = 1 << shift;
		if (m_SwapBitMap[index] & mask) {
				// Mark the bit as free
				m_SwapBitMap[index] &= ~mask;
		} else {
			exit(1);
		}
	}
	
	/*
	  * Scan for first zero bit in bitMap
	  *  If zero bit is found set it to one and return its number,
	  *  otherwise return -1
	  */ 
	uint32_t allocatePage(int goal) {
		uint8_t mask;
		for (uint32_t i = 0; i  < m_MapSize / 4; i++ ) {
			for (int j = 0; j < 8; j += 2) {
				mask = 3 << j;
				if (m_BitMap[i] & mask) {
					// Page is used
					continue;
				}
				// Page is free
				switch (goal) {
				case 1:
					// level 1 page table. Mark as 01
					m_BitMap[i] |= (1 << j);
					break;
				 case 2: 
					// Level 2 page table mark as 10
					m_BitMap[i] |= (1 << (j + 1));
					break;
				case 3:
					// Address space page mark as 11
					m_BitMap[i] |= (3 << j);
					break;
				}
				if ((i * 4 + j / 2 ) >= m_MapSize) {
					exit(4);
				}
				return (i * 4 + j / 2);
			}
		}
		
		// No free page
		// Find level 2 page table
		for (uint32_t i = 0; i < m_MapSize / 4; i++) {
			for (int j = 0; j < 8; j  += 2) {
				mask = (2 << j);
				if ((m_BitMap[i] & mask) == mask) {
					// Found level 2 page table
					uint32_t pageNum = i * 4 + j / 2;
					// m_BitMap == MemStart
					struct pte* pte  = (struct pte*)(m_BitMap + pageNum * 4096);
					for (unsigned k = 0; k < CCPU::PAGE_SIZE / sizeof (pte[0]); k++) {
						if (pte[k].present == 1) {
							// Found page to be swaped out
							uint32_t rc = pte[k].frameNumber;
							uint32_t swapPageNum = allocateSwapPage();
							m_writePage(pte[k].frameNumber, swapPageNum);
							pte[k].present = 0;
							if (swapPageNum >= m_SwapSize) {
								exit(6);
							}
							pte[k].frameNumber = swapPageNum;
							pte[k].swaped = 1;
							// Mark the page in bitMap according to goal
							int i1 = rc / 4; 
							int i2 = (rc * 2) % 8;
							switch (goal) {
							case 1:
								// level 1 page table. Mark as 01
								m_BitMap[i1] |= (1 << i2);
								break;
							case 2: 
								// Level 2 page table mark as 10
								m_BitMap[i1] |= (1 << (i2 + 1));
								break;
							case 3:
								// Address space page mark as 11
								m_BitMap[i1] |= (3 << i2);
								break;
							}
							if (rc >= m_MapSize) {
								exit(5);
							}
							return rc;
						} 
					}
				}
			}
		}
		return (uint32_t)-1;
	}
	
	void freePage(uint32_t pageNum) {
		uint8_t mask; 
		uint32_t index = pageNum / 8;
		int shift = pageNum % 8;
		mask = 1 << shift;
		if (m_BitMap[index] & mask) {
				// Mark the bit as free
				m_BitMap[index] &= ~mask;
		} else {
			exit(1);
		}
	}
	
	uint32_t nrPages() {
		return m_MapSize;
	}
};

CMemoryBitMap* g_Map;


class CMM : public CCPU
{
public:
  
  CMM( uint8_t * memStart, uint32_t  pageTableRoot ): CCPU(memStart, pageTableRoot)
  {
	  struct pte* pageDirPte = (struct pte*)(m_MemStart + m_PageTableRoot);
	  for (unsigned i = 0; i < PAGE_SIZE / sizeof (struct pte); i++) {
		pageDirPte[i].present = 0;
	  }
  }
  
   ~CMM() {}

    virtual bool             newProcess                    ( void            * processArg,
                                                             void           (* entryPoint) ( CCPU *, void * ) ) override;
  protected:
    virtual bool             pageFaultHandler              ( uint32_t          address,
                                                             bool              write ) override;		
    /*
    optionally: 
  
    virtual void             memAccessStart                ( void ) override;
    virtual void             memAccessEnd                  ( void ) override;
    */
	
private:
	
};

bool CMM::newProcess( void * processArg, void  (* entryPoint) ( CCPU *, void * )) 
{
	return true;
}

bool CMM::pageFaultHandler(	uint32_t address, bool write)
{
	int level1index = address >> 22;
	struct pte* pageDirPte = (struct pte*)(m_MemStart + m_PageTableRoot);
	struct pte* pageTablePte;
	
	if (pageDirPte[level1index].present == 0) {
		// Level2 pageTable is not present
		uint32_t frameNum = g_Map->allocatePage(2);
		if (frameNum == (uint32_t)-1) {
			exit(1);
		}
		pageDirPte[level1index].present = 1;
		pageDirPte[level1index].bitU = 1;
		pageDirPte[level1index].bitW = write;
		if (frameNum >= g_Map->nrPages()) {
			exit(7);
		}
		pageDirPte[level1index].frameNumber = frameNum;
		pageTablePte = (struct pte*)(m_MemStart + frameNum * PAGE_SIZE);
		for (unsigned i = 0; i < PAGE_SIZE / sizeof (struct pte); i++) {
				pageTablePte[i].present = 0;
		}
	} else {
		//  Level2 pageTable is present
		pageTablePte = (struct pte*)(m_MemStart + pageDirPte[level1index].frameNumber * PAGE_SIZE);
	}
	
	// Level2 pageTable
	int level2index = (address >> OFFSET_BITS) & (PAGE_DIR_ENTRIES - 1);
	if (pageTablePte[level2index].present == 0) {
		// Virtual page is not present but can be swaped
		uint32_t frameNum = g_Map->allocatePage(3);
		if (frameNum == (uint32_t)-1) {
			exit(2);
		}
		pageTablePte[level2index].present = 1;
		pageTablePte[level2index].bitU = 1;
		pageTablePte[level2index].bitW = write;
		if (pageTablePte[level2index].swaped == 1) {
			g_Map->readPage(frameNum, pageTablePte[level2index].frameNumber);
			pageTablePte[level2index].swaped = 0;
			g_Map->freeSwapPage(pageTablePte[level2index].frameNumber);
		}
		if (frameNum >= g_Map->nrPages()) {
			exit(8);
		}
		pageTablePte[level2index].frameNumber = frameNum;
	} 
	
	if (pageTablePte[level2index].bitW != write) {
		exit(3);
	}
	
	return true;
} 


void               memMgr                                  ( void            * mem,
                                                             uint32_t          memPages,
                                                             uint32_t          diskPages,
                                                             bool           (* readPage) ( uint32_t memFrame, uint32_t diskPage ),
                                                             bool           (* writePage) ( uint32_t memFrame, uint32_t diskPage ),
                                                             void            * processArg,
                                                             void           (* mainProcess) ( CCPU *, void * ) )
{
	for (uint32_t i = 0; i < memPages * CCPU::PAGE_SIZE; i++) {
		((uint8_t*)mem)[i] = 0;
	}
	// Create map of free used pages
	g_Map = new CMemoryBitMap(mem, memPages, diskPages, readPage, writePage);
	
	// Create instance of CCPU
	uint32_t pTable = g_Map->allocatePage(1);
	CMM mm((uint8_t*)mem, pTable * CCPU::PAGE_SIZE);
    
	// Start init process
	mainProcess(&mm, processArg);
	
	delete g_Map;
}
