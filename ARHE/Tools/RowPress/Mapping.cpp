#include "Mapping.h"

uintptr_t Mapping::base_address;

void Mapping::decode_new_address(uintptr_t addr)
{
    auto p = addr;
    int res = 0;
    for (unsigned long i : dram_cfg.DRAM_MTX) {
        res <<= 1ULL;
        res |= (int) __builtin_parityl(p & i);
    }
    bank = (res >> dram_cfg.BK_SHIFT) & dram_cfg.BK_MASK;
    row = (res >> dram_cfg.ROW_SHIFT) & dram_cfg.ROW_MASK;
    column = (res >> dram_cfg.COL_SHIFT) & dram_cfg.COL_MASK;
}

int Mapping::linearize() {
  return (this->bank << dram_cfg.BK_SHIFT) 
        | (this->row << dram_cfg.ROW_SHIFT) 
        | (this->column << dram_cfg.COL_SHIFT);
}

uintptr_t Mapping::to_virt() {
  int res = 0;
  int l = this->linearize();
  for (unsigned long i : dram_cfg.ADDR_MTX) {
    res <<= 1ULL;
    res |= (int) __builtin_parityl(l & i);
  }
  return res + this->base_address;
}

void Mapping::increment_row()
{
  this->row++;
}

void Mapping::increment_bank()
{
  this->bank++;
}

void Mapping::decrement_row()
{
  this->row--;
}

void Mapping::increment_column_dw()
{
  this->column+=8;
}

void Mapping::increment_column_cb()
{
  this->column+=64;
}

void Mapping::reset_column()
{
  this->column = 0;
}

int Mapping::get_bank()
{
    return bank;
}

int Mapping::get_row()
{
    return row;
}

int Mapping::get_column()
{
    return column;
}