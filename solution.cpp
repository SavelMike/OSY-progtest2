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



class CWhateverNiceNameYouLike : public CCPU
{
  public:

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
};



void               memMgr                                  ( void            * mem,
                                                             uint32_t          memPages,
                                                             uint32_t          diskPages,
                                                             bool           (* readPage) ( uint32_t memFrame, uint32_t diskPage ),
                                                             bool           (* writePage) ( uint32_t memFrame, uint32_t diskPage ),
                                                             void            * processArg,
                                                             void           (* mainProcess) ( CCPU *, void * ) )
{
  // todo
}
