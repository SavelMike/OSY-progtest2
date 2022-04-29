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

class CMemoryBitMap {
private:
	char* m_BitMap;
	uint32_t m_MapSize; // in bits
	
public:
	CMemoryBitMap(void* memory, uint32_t mapSize)
	: m_BitMap((char*)memory), m_MapSize(mapSize)
	{
		// Use page number 0 for map of free used pages
		for (uint32_t i = 0; i < mapSize / 8; i++)
			m_BitMap[i] = 0;
		// Mark 0th page as used 	
		m_BitMap[0] |= 1;	
	}
	
	/*
	  * Scan for first zero bit in bitMap
	  *  If zero bit is found set it to one and return its number,
	  *  otherwise return -1
	  */ 
	uint32_t allocatePage(void) {
		uint8_t mask;
		for (uint32_t i = 0; i  < m_MapSize / 8; i++ ) {
			if (m_BitMap[i] == 255)
				continue;
			for (int j  = 0; j < 8; j++) {
				mask = 1 << j;
				if (m_BitMap[i] & mask)
					continue;
				// Mark the bit as used
				m_BitMap[i] |= mask;
				return i * 8 + j;
			}	
		}
		
		// No free page
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
};

CMemoryBitMap* g_Map;

struct pte {
	uint32_t present:1;
	uint32_t bitW:1;
	uint32_t bitU:1;
	uint32_t unused1:1; // 3
	uint32_t unused2:1; // 1
	uint32_t bitR:1;
	uint32_t bitD:1;
	uint32_t unused3:5;
	uint32_t frameNumber:20;
};

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
		uint32_t frameNum = g_Map->allocatePage();
		if (frameNum == (uint32_t)-1) {
			exit(1);
		}
		pageDirPte[level1index].present = 1;
		pageDirPte[level1index].bitU = 1;
		pageDirPte[level1index].bitW = write;
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
		// Virtual page is not present
		uint32_t frameNum = g_Map->allocatePage();
		if (frameNum == (uint32_t)-1) {
			exit(2);
		}
		pageTablePte[level2index].present = 1;
		pageTablePte[level2index].bitU = 1;
		pageTablePte[level2index].bitW = write;
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
	// Create map of free used pages
	g_Map = new CMemoryBitMap(mem, memPages);
	
	// Create instance of CCPU
	uint32_t pTable = g_Map->allocatePage();
	CMM mm((uint8_t*)mem, pTable * CCPU::PAGE_SIZE);
    
	// Start init process
	mainProcess(&mm, processArg);
	
	delete g_Map;
}
