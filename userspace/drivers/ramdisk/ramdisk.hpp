// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <mos/types.h>

#define BLOCKDEV_BLOCK_SIZE 512

class RAMDisk
{
  public:
    RAMDisk(const size_t nbytes);
    ~RAMDisk();

    size_t read_block(const size_t block, const size_t nblocks, uint8_t *buf);
    size_t write_block(const size_t block, const size_t nblocks, const uint8_t *buf);

    u32 nblocks() const
    {
        return m_nblocks;
    }

    u32 block_size() const
    {
        return BLOCKDEV_BLOCK_SIZE;
    }

  private:
    const size_t m_nbytes;
    const size_t m_nblocks;
    uint8_t *m_data;
};
