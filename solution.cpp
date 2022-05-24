#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <cassert>
#include "common.h"
using namespace std;
#endif /* __PROGTEST__ */

struct pte {
	uint32_t present : 1;
	uint32_t bitW : 1;
	uint32_t bitU : 1;
	uint32_t swaped : 1; // Normally unused 
	uint32_t unused2 : 1;
	uint32_t bitR : 1;
	uint32_t bitD : 1;
	uint32_t unused3 : 5;
	uint32_t frameNumber : 20;
};

class FreeSpaceManager {
public:
	FreeSpaceManager(uint8_t* mem, uint32_t pageNum, uint32_t swapPageNum,
		             bool  (*readPage) (uint32_t memFrame, uint32_t diskPage),
		             bool  (*writePage) (uint32_t memFrame, uint32_t diskPage));
	bool readPage(uint32_t memFrame, uint32_t diskPage);
	uint32_t allocateSwapPage(void);
	void freeSwapPage(uint32_t pageNum);
	uint32_t allocatePage(bool isForPageDir);
	void freePage(uint32_t pageNum);
	uint32_t nrPages() { return m_PageNum; }
	void savePageDir(uint32_t pageNum);
	void freePageDirs(uint32_t pageNum);
	void printFreeList(const char* text);
private:
	uint32_t m_MemFreeListHead;
	uint32_t m_SwapFreeListHead;
	uint32_t* m_MemFreeList;
	uint32_t* m_SwapFreeList;
	uint32_t m_PageNum;
	uint32_t m_SwapPageNum;
	uint32_t m_PageDirs[PROCESS_MAX];
	bool  (*m_readPage) (uint32_t memFrame, uint32_t diskPage);
	bool  (*m_writePage) (uint32_t memFrame, uint32_t diskPage);
};

/* Args:
	pageNum - number of frames(pages) in main memory.
	swapPageNum - number of frames in swap space
*/

FreeSpaceManager::FreeSpaceManager(uint8_t* mem, uint32_t pageNum, uint32_t swapPageNum,
				   bool  (*readPage) (uint32_t memFrame, uint32_t diskPage),
				   bool  (*writePage) (uint32_t memFrame, uint32_t diskPage)) {
	m_PageNum = pageNum;
	m_SwapPageNum = swapPageNum;
	// Number of pages needed for free lists
	int pages = ((pageNum + swapPageNum) * sizeof(uint32_t) + (CCPU::PAGE_SIZE - 1)) / CCPU::PAGE_SIZE;
	m_MemFreeList = (uint32_t*)mem;
	m_MemFreeListHead = pages;
	for (unsigned i = m_MemFreeListHead; i < pageNum; i++) {
		if (i == pageNum - 1) {
			// End of free list
			m_MemFreeList[i] = UINT32_MAX;
		}
		else {
			m_MemFreeList[i] = i + 1;
		}
	}
	m_SwapFreeList = m_MemFreeList + pageNum;
	m_SwapFreeListHead = 0;
	for (unsigned i = 0; i < swapPageNum; i++) {
		if (i == swapPageNum - 1) {
			m_SwapFreeList[i] = UINT32_MAX;
		}
		else {
			m_SwapFreeList[i] = i + 1;
		}
	}

	m_readPage = readPage;
	m_writePage = writePage;

	for (unsigned i = 0; i < PROCESS_MAX; i++) {
		m_PageDirs[i] = 0;
	}
}

bool FreeSpaceManager::readPage(uint32_t memFrame, uint32_t diskPage) {
	return m_readPage(memFrame, diskPage);
}

uint32_t FreeSpaceManager::allocateSwapPage(void) {
	uint32_t pageNum = this->m_SwapFreeListHead;
	if (pageNum == UINT32_MAX) {
		return pageNum;
	}
	this->m_SwapFreeListHead = this->m_SwapFreeList[pageNum];
	return pageNum;
}

void FreeSpaceManager::freeSwapPage(uint32_t pageNum) {
	this->m_SwapFreeList[pageNum] = this->m_SwapFreeListHead;
	this->m_SwapFreeListHead = pageNum;
}

// Save pageNum into empty slot of array m_PageDirs.
void FreeSpaceManager::savePageDir(uint32_t pageNum) {
	for (unsigned i = 0; i < PROCESS_MAX; i++) {
		if (m_PageDirs[i] == 0) {
			m_PageDirs[i] = pageNum;
			break;
		}
	}
}

//  Find slot containing pageNum and write 0 into it. 
void FreeSpaceManager::freePageDirs(uint32_t pageNum) {
	for (unsigned i = 0; i < PROCESS_MAX; i++) {
		if (m_PageDirs[i] == pageNum) {
			m_PageDirs[i] = 0;
			break;
		}
	}
}

// Args:
//	isForPageDir: true if page is allocated for page directory
uint32_t FreeSpaceManager::allocatePage(bool isForPageDir) {
	uint32_t pageNum = this->m_MemFreeListHead;
	if (pageNum != UINT32_MAX) {
		// There is free space
		this->m_MemFreeListHead = this->m_MemFreeList[pageNum];
		if (isForPageDir) {
			savePageDir(pageNum);
		}
		return pageNum;
	}
	// There is no free space. Find page candidate for swapping out
	for (unsigned i = 0; i < PROCESS_MAX; i++) {
		if (m_PageDirs[i] == 0) {
			continue;
		}
		struct pte* pde = (struct pte*)((uint8_t*)m_MemFreeList + m_PageDirs[i] * CCPU::PAGE_SIZE);
		for (unsigned k = 0; k < CCPU::PAGE_SIZE / sizeof(pde[0]); k++) {
			if (!pde[k].present) {
				continue;
			}
			struct pte* pte = (struct pte*)((uint8_t*)m_MemFreeList + pde[k].frameNumber * CCPU::PAGE_SIZE);
			for (unsigned l = 0; l < CCPU::PAGE_SIZE / sizeof(pte[0]); l++) {
				if (!pte[l].present) {
					continue;
				}
				// Found candidate for swapping out
				pageNum = pte[l].frameNumber;
				uint32_t swapPageNum = allocateSwapPage();
				m_writePage(pte[l].frameNumber, swapPageNum);
				pte[l].present = 0;
				if (swapPageNum >= m_SwapPageNum) {
					exit(6);
				}
				pte[l].frameNumber = swapPageNum;
				pte[l].swaped = 1;
				if (isForPageDir) {
					savePageDir(pageNum);
				}
				return pageNum;
			}
		}
	}

	// Candidate for swap out not found.
	return UINT32_MAX;
}

void FreeSpaceManager::freePage(uint32_t pageNum) {
	assert(pageNum < this->m_PageNum);
	this->m_MemFreeList[pageNum] = this->m_MemFreeListHead;
	this->m_MemFreeListHead = pageNum;
	for (unsigned i = 0; i < PROCESS_MAX; i++) {
		if (m_PageDirs[i] == pageNum) {
			m_PageDirs[i] = 0;
			break;
		}
	}
}

void FreeSpaceManager::printFreeList(const char* text) {
	uint32_t cur = this->m_MemFreeListHead;
	while (cur != UINT32_MAX) {
		cout << cur << ", ";
		cur = this->m_MemFreeList[cur];
	}
	cout << "\n";
	cur = this->m_SwapFreeListHead;
	cout << "head " << cur << "; " << "SwapFreeList:\n";
	while (cur != UINT32_MAX) {
		cout << cur << ", ";
		cur = this->m_SwapFreeList[cur];
	}
	cout << "\n";
}

FreeSpaceManager* g_FSMan;

class CMM : public CCPU
{
public:
	/**
	 * constructor
	 */
	CMM( uint8_t * memStart, uint32_t  pageTableRoot ): CCPU(memStart, pageTableRoot) {
		struct pte* pageDirPte = (struct pte*)(m_MemStart + m_PageTableRoot);
		for (unsigned i = 0; i < PAGE_SIZE / sizeof (struct pte); i++) {
			pageDirPte[i].present = 0;
		}
	}

	/**
	 * destructor
	 *
	 * free all memory and swap pages
	 */
	~CMM() {
		struct pte* pageDirPte = (struct pte*)(m_MemStart + m_PageTableRoot);
		for (unsigned i = 0; i < PAGE_SIZE / sizeof (struct pte); i++) {
			if (pageDirPte[i].present) {
				struct pte *pageTablePte = (struct pte*)(m_MemStart + pageDirPte[i].frameNumber * PAGE_SIZE);
				for (unsigned j = 0; j < PAGE_SIZE / sizeof (struct pte); j++) {
					if (pageTablePte[j].present) {
						/* free address space page */
						g_FSMan->freePage(pageTablePte[j].frameNumber);
					}
					if (pageTablePte[j].swaped) {
						/* free swap page */
						g_FSMan->freeSwapPage(pageTablePte[j].frameNumber);
					}
				}
				/* free level 2 page table */
				g_FSMan->freePage(pageDirPte[i].frameNumber);
			}
		}

		/* free level 1 page directory */
		g_FSMan->freePage(m_PageTableRoot / CCPU::PAGE_SIZE);
	}

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

bool CMM::pageFaultHandler(uint32_t address, bool write)
{
	int level1index = address >> 22;
	struct pte* pageDirPte = (struct pte*)(m_MemStart + m_PageTableRoot);
	struct pte* pageTablePte;

	if (pageDirPte[level1index].present == 0) {
		// Level2 pageTable is not present
		uint32_t frameNum = g_FSMan->allocatePage(false);
		if (frameNum == UINT32_MAX) {
			cerr << "Fail to allocate page for level2 page table\n";
			exit(1);
		}
		pageDirPte[level1index].present = 1;
		pageDirPte[level1index].bitU = 1;
		pageDirPte[level1index].bitW = write;
		if (frameNum >= g_FSMan->nrPages()) {
			cerr << "Non-existing page is allocated\n";
			exit(1);
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
		uint32_t frameNum = g_FSMan->allocatePage(false);
		if (frameNum == UINT32_MAX) {
			cerr << "Fail to allocate page for address space\n";
			exit(1);
		}
		pageTablePte[level2index].present = 1;
		pageTablePte[level2index].bitU = 1;
		pageTablePte[level2index].bitW = write;
		if (pageTablePte[level2index].swaped == 1) {
			// Read page from swap space
			g_FSMan->readPage(frameNum, pageTablePte[level2index].frameNumber);
			pageTablePte[level2index].swaped = 0;
			g_FSMan->freeSwapPage(pageTablePte[level2index].frameNumber);
		}
		if (frameNum >= g_FSMan->nrPages()) {
			cerr << "Non-existing page is allocated for address space page\n";
			exit(1);
		}
		pageTablePte[level2index].frameNumber = frameNum;
	}

	if (pageTablePte[level2index].bitW != write) {
		cerr << "Right bit mismatch\n";
		exit(1);
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
	// Create free space manager
	g_FSMan = new FreeSpaceManager((uint8_t*)mem, memPages, diskPages, readPage, writePage);
	
	// Create instance of CCPU
	uint32_t pTable = g_FSMan->allocatePage(true);
	CMM* mm = new CMM((uint8_t*)mem, pTable * CCPU::PAGE_SIZE);
    
	// Start init process
	mainProcess(mm, processArg);
	delete mm;
	delete g_FSMan;
}
