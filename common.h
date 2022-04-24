#ifndef COMMON_H_5872395623940562390452903457234
#define COMMON_H_5872395623940562390452903457234

const uint32_t     PROCESS_MAX = 64;

class CCPU
{
  public:
    static constexpr uint32_t OFFSET_BITS                  =                12;
    static constexpr uint32_t PAGE_SIZE                    =  1 << OFFSET_BITS;
    static constexpr uint32_t PAGE_DIR_ENTRIES             =     PAGE_SIZE / 4;
    static constexpr uint32_t ADDR_MASK                    = ~ (PAGE_SIZE - 1);
    static constexpr uint32_t BIT_PRESENT                  = 0x0001;
    static constexpr uint32_t BIT_WRITE                    = 0x0002;
    static constexpr uint32_t BIT_USER                     = 0x0004;
    static constexpr uint32_t BIT_REFERENCED               = 0x0020;
    static constexpr uint32_t BIT_DIRTY                    = 0x0040;
    //---------------------------------------------------------------------------------------------
                             CCPU                          ( uint8_t         * memStart,
                                                             uint32_t          pageTableRoot )
      : m_MemStart ( memStart ),
        m_PageTableRoot ( pageTableRoot )
    {
    }
    //---------------------------------------------------------------------------------------------
    virtual                  ~CCPU                         ( void ) noexcept = default;
    //---------------------------------------------------------------------------------------------
    virtual bool             newProcess                    ( void            * processArg,
                                                             void           (* entryPoint) ( CCPU *, void * ) ) = 0;
    //---------------------------------------------------------------------------------------------
    bool                     readInt                       ( uint32_t          address,
                                                             uint32_t        & value )
    {
      if ( address & 0x3 ) 
        return false; // not aligned
      
      memAccessStart ();
      uint32_t * addr = virtual2Physical ( address, false );
      if ( addr ) 
        value = *addr;
      memAccessEnd ();
      return addr != nullptr;
    }
    //---------------------------------------------------------------------------------------------
    bool                     writeInt                      ( uint32_t          address,
                                                             uint32_t          value )
    {
      if ( address & 0x3 ) 
        return false; // not aligned
      memAccessStart ();
      uint32_t * addr = virtual2Physical ( address, true );
      if ( addr ) 
        *addr = value;
      memAccessEnd ();
      return addr != nullptr;
    }
  protected:
    //---------------------------------------------------------------------------------------------
    uint32_t               * virtual2Physical              ( uint32_t          address,
                                                             bool              write )
    {
      const uint32_t reqMask = BIT_PRESENT | BIT_USER | (write ? BIT_WRITE : 0 );
      const uint32_t orMask = BIT_REFERENCED | (write ? BIT_DIRTY : 0);

      while ( 1 )
      {
        uint32_t & level1 = reinterpret_cast<uint32_t *>(m_MemStart + (m_PageTableRoot & ADDR_MASK)) [address >> 22]; 
        if ( (level1 & reqMask ) != reqMask )
        {
          if ( pageFaultHandler ( address, write ) ) 
            continue;
          return nullptr;
        }
        
        uint32_t & level2 = reinterpret_cast<uint32_t *> (m_MemStart + (level1 & ADDR_MASK )) [ (address >> OFFSET_BITS) & (PAGE_DIR_ENTRIES - 1)]; 
        if ( (level2 & reqMask ) != reqMask )
        {
          if ( pageFaultHandler ( address, write ) ) 
            continue;
          return nullptr;
        }
        
        level1 |= orMask;
        level2 |= orMask;
        return (uint32_t *)(m_MemStart + (level2 & ADDR_MASK) + (address & ~ADDR_MASK));
      }
    }
    //---------------------------------------------------------------------------------------------
    virtual bool             pageFaultHandler              ( uint32_t          address,
                                                             bool              write ) = 0;
    //---------------------------------------------------------------------------------------------
    virtual void             memAccessStart                ( void )
    {
    }
    //---------------------------------------------------------------------------------------------
    virtual void             memAccessEnd                  ( void )
    {
    }
    //---------------------------------------------------------------------------------------------
    uint8_t                * m_MemStart;
    uint32_t                 m_PageTableRoot;
};

void               memMgr                                  ( void            * mem,
                                                             uint32_t          memPages,
                                                             uint32_t          diskPages,
                                                             bool           (* readPage) ( uint32_t memFrame, uint32_t diskPage ),
                                                             bool           (* writePage) ( uint32_t memFrame, uint32_t diskPage ),
                                                             void            * processArg,
                                                             void           (* mainProcess) ( CCPU *, void * ) );

#endif /* COMMON_H_5872395623940562390452903457234 */
