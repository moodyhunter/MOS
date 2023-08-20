// SPDX-License-Identifier: GPL-3.0-only

#include "mos/filesystem/sysfs/sysfs.h"

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/vfs.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/mm/mm.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mm/slab.h"
#include "mos/setup.h"

#include <mos/filesystem/fs_types.h>
#include <mos/lib/structures/list.h>
#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _sysfs_file
{
    const sysfs_item_t *item;

    phyframe_t *buf_page;
    ssize_t buf_head_offset;
    ssize_t buf_npages;

    void *data;
} sysfs_file_t;

static list_head sysfs_dirs = LIST_HEAD_INIT(sysfs_dirs);
static filesystem_t fs_sysfs;
static superblock_t *sysfs_sb = NULL;

static void sysfs_do_register(sysfs_dir_t *entry);

void sysfs_register(sysfs_dir_t *entry)
{
    linked_list_init(list_node(entry));
    list_node_append(&sysfs_dirs, list_node(entry));
    pr_info("sysfs: registered '%s'", entry->name);
    if (sysfs_sb)
        sysfs_do_register(entry);
}

ssize_t sysfs_printf(sysfs_file_t *file, const char *fmt, ...)
{
retry_printf:;
    const size_t spaces_left = file->buf_npages * MOS_PAGE_SIZE - file->buf_head_offset;

    va_list args;
    va_start(args, fmt);
    const size_t written = vsnprintf((char *) phyframe_va(file->buf_page) + file->buf_head_offset, spaces_left, fmt, args);
    va_end(args);

    MOS_ASSERT_X(written <= spaces_left, "sysfs_printf: vsnprintf wrote more than it should have");

    if (written == spaces_left)
    {
        phyframe_t *oldbuf_page = file->buf_page;
        const size_t oldbuf_npages = file->buf_npages;

        // We need to allocate more pages
        file->buf_npages = (file->buf_head_offset + written) / MOS_PAGE_SIZE + 1;
        file->buf_page = mm_get_free_pages(file->buf_npages);

        memcpy((char *) phyframe_va(file->buf_page), (char *) phyframe_va(oldbuf_page), oldbuf_npages * MOS_PAGE_SIZE);
        mm_free_pages(oldbuf_page, oldbuf_npages);
        goto retry_printf;
    }

    file->buf_head_offset += written;
    return written;
}

void sysfs_file_set_data(sysfs_file_t *file, void *data)
{
    file->data = data;
}

void *sysfs_file_get_data(sysfs_file_t *file)
{
    return file->data;
}

static bool sysfs_fops_open(inode_t *i, file_t *file)
{
    mos_debug(vfs, "sysfs: opening %s in %s", file->dentry->name, dentry_parent(file->dentry)->name);
    sysfs_file_t *f = i->private;
    f->buf_page = mm_get_free_page();
    f->buf_npages = 1;
    f->buf_head_offset = 0;
    MOS_ASSERT(f->item->show);
    return true;
}

static void sysfs_fops_release(file_t *file)
{
    mos_debug(vfs, "sysfs: closing %s in %s", file->dentry->name, dentry_parent(file->dentry)->name);
    sysfs_file_t *f = file->dentry->inode->private;
    pmm_unref(f->buf_page, f->buf_npages);
}

static ssize_t sysfs_fops_read(const file_t *file, void *buf, size_t size, off_t offset)
{
    sysfs_file_t *f = file->dentry->inode->private;

    if (f->buf_head_offset == 0)
        if (!f->item->show(f))
            return -1;

    if (offset >= f->buf_head_offset)
        return 0;

    const char *const buf_va = (char *) phyframe_va(f->buf_page);
    const size_t begin = offset;
    const size_t end = MIN(offset + size, (size_t) f->buf_head_offset);

    memcpy((char *) buf, buf_va + begin, end - begin);
    return end - begin;
}

static ssize_t sysfs_fops_write(const file_t *file, const void *buf, size_t size, off_t offset)
{
    sysfs_file_t *f = file->dentry->inode->private;
    return f->item->store(f, buf, size, offset);
}

static const file_ops_t sysfs_file_ops = {
    .open = sysfs_fops_open,
    .release = sysfs_fops_release,
    .read = sysfs_fops_read,
    .write = sysfs_fops_write,
};

static dentry_t *sysfs_fsop_mount(filesystem_t *fs, const char *dev, const char *options)
{
    MOS_ASSERT(fs == &fs_sysfs);
    if (strcmp(dev, "none") != 0)
    {
        mos_warn("sysfs: device not supported");
        return NULL;
    }

    if (options && strlen(options) != 0 && strcmp(options, "defaults") != 0)
    {
        mos_warn("sysfs: options '%s' not supported", options);
        return NULL;
    }

    return sysfs_sb->root;
}

static filesystem_t fs_sysfs = {
    .list_node = LIST_HEAD_INIT(fs_sysfs.list_node),
    .name = "sysfs",
    .mount = sysfs_fsop_mount,
};

static const file_perm_t sysfs_file_perm = PERM_READ;            // r--r--r--
static const file_perm_t sysfs_dir_perm = PERM_READ | PERM_EXEC; // r-xr-xr-x

static u64 sysfs_get_ino(void)
{
    static u64 ino = 1;
    return ino++;
}

static void sysfs_do_register(sysfs_dir_t *entry)
{
    inode_t *dir_i = kmemcache_alloc(inode_cache);
    dir_i->type = FILE_TYPE_DIRECTORY;
    dir_i->perm = sysfs_dir_perm;
    dir_i->ino = sysfs_get_ino();

    dentry_t *d_child = dentry_create(sysfs_sb->root, entry->name);
    d_child->inode = dir_i;

    for (const sysfs_item_t *item = entry->items; item->name; item++)
    {
        sysfs_file_t *sysfs_file = kmalloc(sizeof(sysfs_file_t));
        sysfs_file->item = item;

        inode_t *file_i = kmemcache_alloc(inode_cache);
        file_i->ino = sysfs_get_ino();
        file_i->type = FILE_TYPE_REGULAR;
        file_i->file_ops = &sysfs_file_ops;
        file_i->private = sysfs_file;
        file_i->perm = sysfs_file_perm;
        dentry_create(d_child, item->name)->inode = file_i;
    }
}

static void register_sysfs(void)
{
    vfs_register_filesystem(&fs_sysfs);

    sysfs_sb = kmemcache_alloc(superblock_cache);
    sysfs_sb->fs = &fs_sysfs;

    dentry_t *root = dentry_create(NULL, NULL);
    sysfs_sb->root = root;
    root->inode = kmemcache_alloc(inode_cache);
    root->inode->type = FILE_TYPE_DIRECTORY;
    root->superblock = sysfs_sb;

    list_foreach(sysfs_dir_t, entry, sysfs_dirs)
    {
        sysfs_do_register(entry);
    }
}

MOS_INIT(VFS, register_sysfs);
