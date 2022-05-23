#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <pthread.h>
#include <cassert>
#include <semaphore.h>
#include "common.h"
using namespace std;

const int MEM_PAGES  = 1024;
const int DISK_PAGES = 1024;
// MEM_PAGES + extra 4KiB for alignment
uint8_t            g_Memory [ MEM_PAGES * CCPU::PAGE_SIZE + CCPU::PAGE_SIZE ];
// align to a mutiple of 4KiB
uint8_t          * g_MemoryAligned = (uint8_t *) (( ((uintptr_t) g_Memory) + CCPU::PAGE_SIZE - 1) & ~(uintptr_t) ~CCPU::ADDR_MASK );
// swap file access
FILE             * g_Fp;
// mutex for swap file (swap read/write functions are thread-safe)
pthread_mutex_t    g_Mtx;
//-------------------------------------------------------------------------------------------------
static void        seqTest1                                ( CCPU            * cpu,
                                                             void            * arg )
{
  for ( uint32_t i = 0; i < 2000; i += 4 )
  {
    uint32_t x;
    assert ( cpu -> readInt ( i, x ) );
    assert ( x == 0 );
  }
  
  for ( uint32_t i = 19230400; i < 20230400; i += 4 )
  {
    uint32_t x;
    assert ( cpu -> readInt ( i, x ) );
    assert ( x == 0 );
  }
}
//-------------------------------------------------------------------------------------------------
static void        seqTest2                                ( CCPU            * cpu,
                                                             void            * arg )
{
  for ( uint32_t i = 0; i < 2000; i += 4 )
  {
    assert ( cpu -> writeInt ( i, i + 1234567 ) );
  }
  
  for ( uint32_t i = 19230400; i < 20230400; i += 4 )
  {
    assert ( cpu -> writeInt ( i, i + 7654321 ) );
  }
  
  for ( uint32_t i = 0; i < 2000; i += 4 )
  {
    uint32_t x;
    assert ( cpu -> readInt ( i, x ) );
    assert ( x == i + 1234567 );
  }
  
  for ( uint32_t i = 19230400; i < 20230400; i += 4 )
  {
    uint32_t x;
    assert ( cpu -> readInt ( i, x ) );
    assert ( x == i + 7654321 );
  }
}
//-------------------------------------------------------------------------------------------------
static void        parTest1                                ( CCPU            * cpu,
                                                             void            * arg )
{
  cpu -> newProcess ( nullptr, seqTest1 );
  cpu -> newProcess ( nullptr, seqTest2 );
  cpu -> newProcess ( nullptr, seqTest2 );
}
//-------------------------------------------------------------------------------------------------
bool               fnReadPage                              ( uint32_t          memFrame,
                                                             uint32_t          diskPage )
{
  pthread_mutex_lock ( &g_Mtx );
  fseek ( g_Fp, diskPage * CCPU::PAGE_SIZE, SEEK_SET );
  bool res = fread ( g_MemoryAligned + memFrame * CCPU::PAGE_SIZE, 1, CCPU::PAGE_SIZE, g_Fp ) == CCPU::PAGE_SIZE;
  pthread_mutex_unlock ( &g_Mtx );
  return res;
}
//-------------------------------------------------------------------------------------------------
bool               fnWritePage                             ( uint32_t          memFrame,
                                                             uint32_t          diskPage )
{
  pthread_mutex_lock ( &g_Mtx );
  fseek ( g_Fp, diskPage * CCPU::PAGE_SIZE, SEEK_SET );
  bool res = fwrite ( g_MemoryAligned + memFrame * CCPU::PAGE_SIZE, 1, CCPU::PAGE_SIZE, g_Fp ) == CCPU::PAGE_SIZE;
  pthread_mutex_unlock ( &g_Mtx );
  return res;
}
//-------------------------------------------------------------------------------------------------
int                main                                    ( void )
{
  g_Fp = fopen ( "/tmp/pagefile", "w+b" );
  if ( ! g_Fp )
  {
    printf ( "Cannot create swap file\n" );
    return 1;
  }
  pthread_mutex_init ( &g_Mtx, nullptr );
  

  memMgr ( g_MemoryAligned, MEM_PAGES, DISK_PAGES, fnReadPage, fnWritePage, nullptr, seqTest1 );
  
  memMgr ( g_MemoryAligned, MEM_PAGES, DISK_PAGES, fnReadPage, fnWritePage, nullptr, seqTest2 );

  memMgr ( g_MemoryAligned, 100, DISK_PAGES, fnReadPage, fnWritePage, nullptr, seqTest2 );
  
  memMgr ( g_MemoryAligned, 100, DISK_PAGES, fnReadPage, fnWritePage, nullptr, parTest1 );
  
  pthread_mutex_destroy ( &g_Mtx );
  fclose ( g_Fp );
  return 0;
}
