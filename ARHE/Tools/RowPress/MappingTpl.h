#include <inttypes.h>

/**
 * Stateful class to decode address mappings
 * Yanks a lot of code from Blacksmith: https://github.com/comsec-group/blacksmith
 * Modified by SNS group to be dynamic
 */

#define CHANS(x) ((x) << (8UL * 3UL))
#define DIMMS(x) ((x) << (8UL * 2UL))
#define RANKS(x) ((x) << (8UL * 1UL))
#define BANKS(x) ((x) << (8UL * 0UL))

static const int MTX_SIZE = 30;

struct MemConfiguration {
  int IDENTIFIER;
  int BK_SHIFT;
  int BK_MASK;
  int ROW_SHIFT;
  int ROW_MASK;
  int COL_SHIFT;
  int COL_MASK;
  int DRAM_MTX[MTX_SIZE];
  int ADDR_MTX[MTX_SIZE];
};


class Mapping
{
private:
    int bank;
    int row;
    int column;

// SNS Implementation | Attention! Do not change the below comments unless you know what you are doing!
/* START_DYNAMIC_INJECTION */

/* END_DYNAMIC_INJECTION */

public:
    Mapping() {};

    // copy constructor for mapping
    Mapping (const Mapping &m)
    {
        this->bank = m.bank;
        this->row = m.row;
        this->column = m.column;
    }

    static uintptr_t base_address;
    
    int get_column();
    int get_bank();
    int get_row();

    int linearize();

    void increment_row();
    void increment_column_dw();
    void increment_column_cb();
    void reset_column();
    void increment_bank();
    void decrement_row();

    uintptr_t to_virt();

    void decode_new_address(uintptr_t addr);

};
