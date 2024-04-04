// SPDX-License-Identifier: GPL-3.0-only

#define pr_fmt(fmt) "sysfs: " fmt

#include "mos/filesystem/sysfs/sysfs.h"

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/vfs.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/mm/mm.h"
#include "mos/mm/physical/pmm.h"
#include "mos/printk.h"
#include "mos/setup.h"

#include <mos/filesystem/fs_types.h>
#include <mos/io/io_types.h>
#include <mos/lib/structures/list.h>
#include <mos/types.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

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

static void sysfs_do_register(sysfs_dir_t *sysfs_dir);

static u64 sysfs_get_ino(void)
{
    static u64 ino = 1;
    return ino++;
}

void sysfs_register(sysfs_dir_t *dir)
{
    linked_list_init(list_node(dir));
    list_node_append(&sysfs_dirs, list_node(dir));
    pr_dinfo2(sysfs, "registering '%s'", dir->name);
    MOS_ASSERT(sysfs_sb);
    sysfs_do_register(dir);
}

static void sysfs_expand_buffer(sysfs_file_t *file, size_t new_npages)
{
    phyframe_t *oldbuf_page = file->buf_page;
    const size_t oldbuf_npages = file->buf_npages;

    // We need to allocate more pages
    file->buf_npages = new_npages;
    file->buf_page = mm_get_free_pages(file->buf_npages);

    if (oldbuf_page)
    {
        memcpy((char *) phyframe_va(file->buf_page), (char *) phyframe_va(oldbuf_page), oldbuf_npages * MOS_PAGE_SIZE);
        mm_free_pages(oldbuf_page, oldbuf_npages);
    }
}

ssize_t sysfs_printf(sysfs_file_t *file, const char *fmt, ...)
{
retry_printf:;
    const size_t spaces_left = file->buf_npages * MOS_PAGE_SIZE - file->buf_head_offset;

    va_list args;
    va_start(args, fmt);
    const size_t should_write = vsnprintf((char *) phyframe_va(file->buf_page) + file->buf_head_offset, spaces_left, fmt, args);
    va_end(args);

    if (should_write >= spaces_left)
    {
        sysfs_expand_buffer(file, file->buf_npages + 1);
        goto retry_printf;
    }

    file->buf_head_offset += should_write;
    return should_write;
}

ssize_t sysfs_put_data(sysfs_file_t *file, const void *data, size_t count)
{
    const size_t spaces_left = file->buf_npages * MOS_PAGE_SIZE - file->buf_head_offset;

    if (count > spaces_left)
        sysfs_expand_buffer(file, file->buf_npages + (count - spaces_left) / MOS_PAGE_SIZE + 1);

    memcpy((char *) phyframe_va(file->buf_page) + file->buf_head_offset, data, count);
    file->buf_head_offset += count;
    return count;
}

void sysfs_file_set_data(sysfs_file_t *file, void *data)
{
    file->data = data;
}

void *sysfs_file_get_data(sysfs_file_t *file)
{
    return file->data;
}

static bool sysfs_fops_open(inode_t *i, file_t *file, bool create)
{
    pr_dinfo2(sysfs, "opening %s in %s", file->dentry->name, dentry_parent(file->dentry)->name);
    MOS_UNUSED(create);
    sysfs_file_t *f = i->private;
    f->buf_page = NULL;
    f->buf_npages = 0;
    f->buf_head_offset = 0;
    return true;
}

static void sysfs_fops_release(file_t *file)
{
    pr_dinfo2(sysfs, "closing %s in %s", file->dentry->name, dentry_parent(file->dentry)->name);
    sysfs_file_t *f = file->dentry->inode->private;
    if (f->buf_page)
        mm_free_pages(f->buf_page, f->buf_npages);
}

__nodiscard static bool sysfs_file_ensure_ready(const file_t *file)
{
    sysfs_file_t *f = file->dentry->inode->private;
    if (f->buf_head_offset == 0)
    {
        if (!f->item->show(f))
            return false;
    }

    return true;
}

static ssize_t sysfs_fops_read(const file_t *file, void *buf, size_t size, off_t offset)
{
    sysfs_file_t *f = file->dentry->inode->private;
    if (f->item->type != SYSFS_RO && f->item->type != SYSFS_RW)
        return -ENOTSUP;

    if (!sysfs_file_ensure_ready(file))
        return -ETXTBSY;

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
    if (f->item->type != SYSFS_WO && f->item->type != SYSFS_RW)
        return -ENOTSUP;
    return f->item->store(f, buf, size, offset);
}

static off_t sysfs_fops_seek(file_t *file, off_t offset, io_seek_whence_t whence)
{
    if (offset != 0)
        return -1; // cannot change its internal buffer state

    if (whence == IO_SEEK_DATA || whence == IO_SEEK_HOLE)
        return -1; // not supported

    if (whence == IO_SEEK_SET)
        return -1;

    sysfs_file_t *f = file->dentry->inode->private;

    if (f->item->type == SYSFS_MEM)
        return -1;

    if (!sysfs_file_ensure_ready(file))
        return -1;

    return f->buf_head_offset;
}

bool sysfs_fops_mmap(file_t *file, vmap_t *vmap, off_t offset)
{
    MOS_UNUSED(vmap);
    MOS_UNUSED(offset);

    sysfs_file_t *f = file->dentry->inode->private;
    if (f->item->type == SYSFS_MEM)
    {
        return f->item->mem.mmap(f, vmap, offset);
    }
    else
    {
        if (!sysfs_file_ensure_ready(file))
            return false;

        if (offset > f->buf_head_offset)
            return false; // cannot map past the end of the file
    }

    return true;
}

bool sysfs_fops_munmap(file_t *file, vmap_t *vmap, bool *unmapped)
{
    sysfs_file_t *f = file->dentry->inode->private;
    if (f->item->type == SYSFS_MEM)
    {
        return f->item->mem.munmap(f, vmap, unmapped);
    }

    return true;
}

static const file_ops_t sysfs_file_ops = {
    .open = sysfs_fops_open,
    .release = sysfs_fops_release,
    .read = sysfs_fops_read,
    .write = sysfs_fops_write,
    .seek = sysfs_fops_seek,
    .mmap = sysfs_fops_mmap,
    .munmap = sysfs_fops_munmap,
};

static void sysfs_iops_iterate_dir(dentry_t *dentry, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
    // root directory
    if (dentry->inode == sysfs_sb->root->inode)
    {
        // list all the directories in the dcache
        vfs_generic_iterate_dir(dentry, state, add_record);
        return;
    }

    sysfs_dir_t *dir = dentry->inode->private;
    MOS_ASSERT_X(dir, "invalid sysfs entry, possibly a VFS bug");

    // non-dynamic directory
    if (list_is_empty(&dir->_dynamic_items))
    {
        vfs_generic_iterate_dir(dentry, state, add_record);
        return;
    }

    for (size_t j = 0; j < dir->num_items; j++)
    {
        sysfs_item_t *item = &dir->items[j];
        if (item->type == _SYSFS_INVALID || item->type == SYSFS_DYN)
            continue;

        add_record(state, item->ino, item->name, strlen(item->name), FILE_TYPE_REGULAR);
    }

    // iterate the dynamic items
    list_node_foreach(item_node, &dir->_dynamic_items)
    {
        sysfs_item_t *const dynitem = container_of(item_node, sysfs_item_t, dyn.list_node);
        dynitem->dyn.iterate(dynitem, dentry, state, add_record);
    }
}

static bool sysfs_iops_lookup(inode_t *dir, dentry_t *dentry)
{
    // if we get here, it means the expected dentry cannpt be found in the dentry cache
    // that means either it's a dynamic item, or the user is trying to access a file that doesn't exist

    sysfs_dir_t *sysfs_dir = dir->private;

    // if it's NULL, it implies that the dir must be the root directory /sys
    MOS_ASSERT_X(sysfs_dir || dir == sysfs_sb->root->inode, "invalid sysfs entry, possibly a VFS bug");

    if (!sysfs_dir)
        return false; // sysfs root dir doesn't have any dynamic items

    if (list_is_empty(&sysfs_dir->_dynamic_items))
        return false;

    list_node_foreach(item_node, &sysfs_dir->_dynamic_items)
    {
        sysfs_item_t *const dynitem = container_of(item_node, sysfs_item_t, dyn.list_node);
        if (dynitem->dyn.lookup(dir, dentry))
            return true;
    }

    return false;
}

static bool sysfs_iops_create(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    sysfs_dir_t *sysfs_dir = dir->private;
    MOS_ASSERT_X(sysfs_dir || dir == sysfs_sb->root->inode, "invalid sysfs entry, possibly a VFS bug");

    if (!sysfs_dir)
        return false; // sysfs root dir doesn't have any dynamic items

    if (list_is_empty(&sysfs_dir->_dynamic_items))
        return false;

    list_node_foreach(item_node, &sysfs_dir->_dynamic_items)
    {
        sysfs_item_t *const dynitem = container_of(item_node, sysfs_item_t, dyn.list_node);
        if (dynitem->dyn.create(dir, dentry, type, perm))
            return true;
    }

    return false;
}

static const inode_ops_t sysfs_dir_i_ops = {
    .iterate_dir = sysfs_iops_iterate_dir,
    .lookup = sysfs_iops_lookup,
    .newfile = sysfs_iops_create,
};

static dentry_t *sysfs_fsop_mount(filesystem_t *fs, const char *dev, const char *options)
{
    MOS_ASSERT(fs == &fs_sysfs);
    if (strcmp(dev, "none") != 0)
    {
        mos_warn("device not supported");
        return NULL;
    }

    if (options && strlen(options) != 0 && strcmp(options, "defaults") != 0)
    {
        mos_warn("options '%s' not supported", options);
        return NULL;
    }

    return sysfs_sb->root;
}

static filesystem_t fs_sysfs = {
    .list_node = LIST_HEAD_INIT(fs_sysfs.list_node),
    .name = "sysfs",
    .mount = sysfs_fsop_mount,
};

static const file_perm_t sysfs_dir_perm = PERM_READ | PERM_EXEC; // r-xr-xr-x

static void sysfs_do_register(sysfs_dir_t *sysfs_dir)
{
    inode_t *dir_i = inode_create(sysfs_sb, sysfs_get_ino(), FILE_TYPE_DIRECTORY);
    dir_i->perm = sysfs_dir_perm;
    dir_i->ops = &sysfs_dir_i_ops;

    dentry_t *vfs_dir = dentry_create(sysfs_sb, sysfs_sb->root, sysfs_dir->name);
    vfs_dir->inode = dir_i;
    vfs_dir->inode->private = sysfs_dir; ///< for convenience
    sysfs_dir->_dentry = vfs_dir;

    for (size_t i = 0; i < sysfs_dir->num_items; i++)
        sysfs_register_file(sysfs_dir, &sysfs_dir->items[i], NULL);
}

inode_t *sysfs_create_inode(file_type_t type, void *data)
{
    inode_t *inode = inode_create(sysfs_sb, sysfs_get_ino(), type);
    inode->private = data;
    return inode;
}

void sysfs_register_file(sysfs_dir_t *sysfs_dir, sysfs_item_t *item, void *data)
{
    if (item->type == _SYSFS_INVALID)
        return;

    if (item->type == SYSFS_DYN)
    {
        MOS_ASSERT(item->dyn.iterate);
        linked_list_init(list_node(&item->dyn));
        list_node_append(&sysfs_dir->_dynamic_items, list_node(&item->dyn));
        return;
    }

    sysfs_file_t *sysfs_file = kmalloc(sizeof(sysfs_file_t));
    sysfs_file->item = item;
    sysfs_file->data = data;

    inode_t *file_i = inode_create(sysfs_sb, sysfs_get_ino(), FILE_TYPE_REGULAR);
    file_i->file_ops = &sysfs_file_ops;
    file_i->private = sysfs_file;
    item->ino = file_i->ino;

    switch (item->type)
    {
        case _SYSFS_INVALID: MOS_UNREACHABLE();
        case SYSFS_DYN: MOS_UNREACHABLE();
        case SYSFS_RO: file_i->perm |= PERM_READ; break;
        case SYSFS_RW: file_i->perm |= PERM_READ | PERM_WRITE; break;
        case SYSFS_WO: file_i->perm |= PERM_WRITE; break;
        case SYSFS_MEM:
            file_i->perm |= PERM_READ | PERM_WRITE | PERM_EXEC;
            file_i->size = item->mem.size;
            break;
    }

    if (unlikely(!item->name || strlen(item->name) == 0))
        pr_warn("no name specified for sysfs entry '%s'", sysfs_dir ? sysfs_dir->name : "/");

    dentry_t *const target_dentry = sysfs_dir ? sysfs_dir->_dentry : sysfs_sb->root;
    MOS_ASSERT_X(target_dentry, "registering sysfs entry '%s' failed", item->name);
    dentry_create(sysfs_sb, target_dentry, item->name)->inode = file_i;
}

static void register_sysfs(void)
{
    vfs_register_filesystem(&fs_sysfs);

    sysfs_sb = kmalloc(superblock_cache);
    sysfs_sb->fs = &fs_sysfs;
    sysfs_sb->root = dentry_create(sysfs_sb, NULL, NULL);
    sysfs_sb->root->inode = inode_create(sysfs_sb, sysfs_get_ino(), FILE_TYPE_DIRECTORY);
    sysfs_sb->root->inode->ops = &sysfs_dir_i_ops;
}

MOS_INIT(VFS, register_sysfs);
