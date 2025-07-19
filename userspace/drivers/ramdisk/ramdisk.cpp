// SPDX-License-Identifier: GPL-3.0-or-later

#include "ramdisk.hpp"

#include <abi-bits/vm-flags.h>
#include <string.h>
#include <sys/mman.h>

RAMDisk::RAMDisk(const size_t nbytes) : m_nbytes(nbytes), m_nblocks(nbytes / BLOCKDEV_BLOCK_SIZE)
{
    m_data = (uint8_t *) mmap(NULL, nbytes, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

RAMDisk::~RAMDisk()
{
    munmap(m_data, m_nbytes);
}

size_t RAMDisk::read_block(const size_t block, const size_t nblocks, uint8_t *buf)
{
    if (block + nblocks > m_nblocks)
        return 0;

    memcpy(buf, m_data + block * BLOCKDEV_BLOCK_SIZE, nblocks * BLOCKDEV_BLOCK_SIZE);
    return nblocks;
}

size_t RAMDisk::write_block(const size_t block, const size_t nblocks, const uint8_t *buf)
{
    if (block + nblocks > m_nblocks)
        return 0;

    memcpy(m_data + block * BLOCKDEV_BLOCK_SIZE, buf, nblocks * BLOCKDEV_BLOCK_SIZE);
    return nblocks;
}
