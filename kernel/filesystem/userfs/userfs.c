// SPDX-License-Identifier: GPL-3.0-or-later
// userspace filesystems

#include "mos/filesystem/userfs/userfs.h"

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/misc/profiling.h"
#include "mos/syslog/printk.h"
#include "proto/filesystem.pb.h"
#include "proto/filesystem.services.h"

#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server.h>
#include <mos/filesystem/fs_types.h>
#include <mos/proto/fs_server.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>

RPC_CLIENT_DEFINE_SIMPLECALL(fs_client, USERFS_SERVICE_X)

static const inode_ops_t userfs_iops;
static const file_ops_t userfs_fops;
static const inode_cache_ops_t userfs_inode_cache_ops;
static const superblock_ops_t userfs_sb_ops;

#define userfs_get(_fs, _fmt, ...)                                                                                                                                       \
    statement_expr(userfs_t *, {                                                                                                                                         \
        retval = container_of(_fs, userfs_t, fs);                                                                                                                        \
        userfs_ensure_connected(retval);                                                                                                                                 \
        pr_dinfo2(userfs, "calling '%s' (rpc_server '%s'): " _fmt, _fs->name, retval->rpc_server_name __VA_OPT__(, __VA_ARGS__));                                        \
    })

inode_t *i_from_pbfull(const mosrpc_fs_inode_info *stat, superblock_t *sb, void *private)
{
    // enum pb_file_type_t -> enum file_type_t is safe here because they have the same values
    inode_t *i = inode_create(sb, stat->ino, (file_type_t) stat->type);
    i->created = stat->created;
    i->modified = stat->modified;
    i->accessed = stat->accessed;
    i->size = stat->size;
    i->uid = stat->uid;
    i->gid = stat->gid;
    i->perm = stat->perm;
    i->nlinks = stat->nlinks;
    i->suid = stat->suid;
    i->sgid = stat->sgid;
    i->sticky = stat->sticky;
    i->private_data = private;
    i->ops = &userfs_iops;
    i->file_ops = &userfs_fops;
    return i;
}

mosrpc_fs_inode_info *i_to_pb_full(const inode_t *i, mosrpc_fs_inode_info *pbi)
{
    pbi->ino = i->ino;
    pbi->type = i->type;
    pbi->created = i->created;
    pbi->modified = i->modified;
    pbi->accessed = i->accessed;
    pbi->size = i->size;
    pbi->uid = i->uid;
    pbi->gid = i->gid;
    pbi->perm = i->perm;
    pbi->nlinks = i->nlinks;
    pbi->suid = i->suid;
    pbi->sgid = i->sgid;
    pbi->sticky = i->sticky;
    // pbi->private_data = (ptr_t) i->private;
    return pbi;
}

mosrpc_fs_inode_ref i_to_pb_ref(const inode_t *i)
{
    mosrpc_fs_inode_ref ref = { .data = (ptr_t) i->private_data }; // for userfs, private_data is the inode reference used by the server
    return ref;
}

void userfs_ensure_connected(userfs_t *userfs)
{
    if (likely(userfs->rpc_server))
        return;

    userfs->rpc_server = rpc_client_create(userfs->rpc_server_name);
    if (unlikely(!userfs->rpc_server))
    {
        pr_warn("userfs_ensure_connected: failed to connect to %s", userfs->rpc_server_name);
        return;
    }
}

static bool userfs_iop_hardlink(dentry_t *d, inode_t *i, dentry_t *new_d)
{
    MOS_UNUSED(i);
    MOS_UNUSED(new_d);

    userfs_t *userfs = userfs_get(d->superblock->fs, "hardlink: %s", dentry_name(d));
    MOS_UNUSED(userfs);

    return false;
}

static void userfs_iop_iterate_dir(dentry_t *dentry, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
    userfs_t *userfs = userfs_get(dentry->superblock->fs, "iterate_dir: %s", dentry_name(dentry));

    mosrpc_fs_readdir_request req = { 0 };
    mosrpc_fs_readdir_response resp = { 0 };
    req.i_ref = i_to_pb_ref(dentry->inode);

    const pf_point_t ev = profile_enter();
    const int result = fs_client_readdir(userfs->rpc_server, &req, &resp);
    profile_leave(ev, "userfs.'%s'.readdir", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_iop_iterate_dir: failed to readdir %s: %d", dentry_name(dentry), result);
        goto bail_out;
    }

    if (!resp.entries_count)
    {
        pr_dwarn(userfs, "userfs_iop_iterate_dir: failed to readdir %s: %s", dentry_name(dentry), resp.result.error);
        goto bail_out;
    }

    for (size_t i = 0; i < resp.entries_count; i++)
    {
        const mosrpc_fs_pb_dirent *pbde = &resp.entries[i];
        MOS_ASSERT(pbde->name);
        add_record(state, pbde->ino, pbde->name, strlen(pbde->name), (file_type_t) pbde->type);
    }

bail_out:
    pb_release(mosrpc_fs_readdir_response_fields, &resp);
}

static bool userfs_iop_lookup(inode_t *dir, dentry_t *dentry)
{
    userfs_t *userfs = userfs_get(dir->superblock->fs, "lookup: %s", dentry_name(dentry));

    bool ret = false;
    mosrpc_fs_lookup_request req = { 0 };
    req.i_ref = i_to_pb_ref(dir);
    req.name = (char *) dentry_name(dentry);

    mosrpc_fs_lookup_response resp = { 0 };
    const pf_point_t ev = profile_enter();
    const int result = fs_client_lookup(userfs->rpc_server, &req, &resp);
    profile_leave(ev, "userfs.'%s'.lookup", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_iop_lookup: failed to lookup %s: %d", dentry_name(dentry), result);
        goto leave;
    }

    if (!resp.result.success)
        goto leave; // ENOENT is not a big deal

    inode_t *i = i_from_pbfull(&resp.i_info, dir->superblock, (void *) resp.i_ref.data);
    dentry_attach(dentry, i);
    dentry->superblock = i->superblock = dir->superblock;
    i->ops = &userfs_iops;
    i->cache.ops = &userfs_inode_cache_ops;
    i->file_ops = &userfs_fops;
    ret = true;

leave:
    pb_release(mosrpc_fs_lookup_response_fields, &resp);
    return ret;
}

static bool userfs_iop_mkdir(inode_t *dir, dentry_t *dentry, file_perm_t perm)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(perm);

    userfs_t *userfs = userfs_get(dir->superblock->fs, "mkdir: %s", dentry_name(dentry));
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_mknode(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm, dev_t dev)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(type);
    MOS_UNUSED(perm);
    MOS_UNUSED(dev);

    userfs_t *userfs = userfs_get(dir->superblock->fs, "mknode: %s", dentry_name(dentry));
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_newfile(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    userfs_t *userfs = userfs_get(dir->superblock->fs, "newfile: %s", dentry_name(dentry));

    bool ret = false;
    mosrpc_fs_create_file_request req;
    req.i_ref = i_to_pb_ref(dir);
    req.name = (char *) dentry_name(dentry);
    req.type = type;
    req.perm = perm;

    mosrpc_fs_create_file_response resp = { 0 };

    const pf_point_t pp = profile_enter();
    const int result = fs_client_create_file(userfs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.create_file", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_iop_newfile: failed to create file %s: %d", dentry_name(dentry), result);
        goto bail_out;
    }

    if (!resp.result.success)
    {
        pr_dwarn(userfs, "userfs_iop_newfile: failed to create file %s: %s", dentry_name(dentry), resp.result.error);
        goto bail_out;
    }

    inode_t *i = i_from_pbfull(&resp.i_info, dir->superblock, (void *) resp.i_ref.data);
    dentry_attach(dentry, i);
    dentry->superblock = i->superblock = dir->superblock;
    i->ops = &userfs_iops;
    i->cache.ops = &userfs_inode_cache_ops;
    i->file_ops = &userfs_fops;
    ret = true;

bail_out:
    pb_release(mosrpc_fs_create_file_response_fields, &resp);
    return ret;
}

static size_t userfs_iop_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    userfs_t *userfs = userfs_get(dentry->superblock->fs, "readlink: %s", dentry_name(dentry));

    mosrpc_fs_readlink_request req = { 0 };
    mosrpc_fs_readlink_response resp = { 0 };
    req.i_ref = i_to_pb_ref(dentry->inode);

    const pf_point_t pp = profile_enter();
    const int result = fs_client_readlink(userfs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.readlink", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_iop_readlink: failed to readlink %s: %d", dentry_name(dentry), result);
        goto bail_out;
    }

    if (!resp.result.success)
    {
        pr_dwarn(userfs, "userfs_iop_readlink: failed to readlink %s: %s", dentry_name(dentry), resp.result.error);
        goto bail_out;
    }

    size_t len = strlen(resp.target);
    if (len > buflen)
        len = buflen;

    memcpy(buffer, resp.target, len);
    return len;

bail_out:
    pb_release(mosrpc_fs_readlink_response_fields, &resp);
    return -EIO;
}

static bool userfs_iop_rename(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry)
{
    MOS_UNUSED(old_dir);
    MOS_UNUSED(old_dentry);
    MOS_UNUSED(new_dir);
    MOS_UNUSED(new_dentry);

    userfs_t *userfs = userfs_get(old_dir->superblock->fs, "rename: %s -> %s", dentry_name(old_dentry), dentry_name(new_dentry));
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_rmdir(inode_t *dir, dentry_t *dentry)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);

    userfs_t *userfs = userfs_get(dir->superblock->fs, "rmdir: %s", dentry_name(dentry));
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_symlink(inode_t *dir, dentry_t *dentry, const char *symname)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(symname);

    userfs_t *userfs = userfs_get(dir->superblock->fs, "symlink: %s", dentry_name(dentry));
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_unlink(inode_t *dir, dentry_t *dentry)
{
    userfs_t *userfs = userfs_get(dir->superblock->fs, "unlink: %s", dentry_name(dentry));

    mosrpc_fs_unlink_request req;
    mosrpc_fs_unlink_response resp = { 0 };

    req.i_ref = i_to_pb_ref(dir);
    req.dentry.inode_id = dentry->inode->ino;
    req.dentry.name = (char *) dentry_name(dentry);

    const pf_point_t pp = profile_enter();
    const int result = fs_client_unlink(userfs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.unlink", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_iop_unlink: failed to unlink %s: %d", dentry_name(dentry), result);
        goto bail_out;
    }

    if (!resp.result.success)
    {
        pr_dwarn(userfs, "userfs_iop_unlink: failed to unlink %s: %s", dentry_name(dentry), resp.result.error);
        goto bail_out;
    }

    return true;

bail_out:
    pb_release(mosrpc_fs_unlink_response_fields, &resp);
    return false;
}

static const inode_ops_t userfs_iops = {
    .hardlink = userfs_iop_hardlink,
    .iterate_dir = userfs_iop_iterate_dir,
    .lookup = userfs_iop_lookup,
    .mkdir = userfs_iop_mkdir,
    .mknode = userfs_iop_mknode,
    .newfile = userfs_iop_newfile,
    .readlink = userfs_iop_readlink,
    .rename = userfs_iop_rename,
    .rmdir = userfs_iop_rmdir,
    .symlink = userfs_iop_symlink,
    .unlink = userfs_iop_unlink,
};

static bool userfs_fop_open(inode_t *inode, file_t *file, bool created)
{
    MOS_UNUSED(inode);
    MOS_UNUSED(file);
    MOS_UNUSED(created);

    return true;
}

static const file_ops_t userfs_fops = {
    .open = userfs_fop_open,
    .read = vfs_generic_read,
    .write = vfs_generic_write,
    .release = NULL,
    .seek = NULL,
    .mmap = NULL,
    .munmap = NULL,
};

static phyframe_t *userfs_inode_cache_fill_cache(inode_cache_t *cache, off_t pgoff)
{
    userfs_t *userfs = userfs_get(cache->owner->superblock->fs, "fill_cache", );

    mosrpc_fs_getpage_request req = { 0 };
    req.i_ref = i_to_pb_ref(cache->owner);
    req.pgoff = pgoff;

    mosrpc_fs_getpage_response resp = { 0 };

    const pf_point_t pp = profile_enter();
    const int result = fs_client_get_page(userfs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.getpage", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_inode_cache_fill_cache: failed to getpage: %d", result);
        goto bail_out;
    }

    if (!resp.result.success)
    {
        pr_dwarn(userfs, "userfs_inode_cache_fill_cache: failed to getpage: %s", resp.result.error);
        goto bail_out;
    }

    // allocate a page
    phyframe_t *page = pmm_ref_one(mm_get_free_page());
    if (!page)
    {
        pr_warn("userfs_inode_cache_fill_cache: failed to allocate page");
        goto bail_out;
    }

    // copy the data from the server
    memcpy((void *) phyframe_va(page), resp.data->bytes, MIN(resp.data->size, MOS_PAGE_SIZE));
    return page;

bail_out:
    pb_release(mosrpc_fs_getpage_response_fields, &resp);
    return ERR_PTR(-EIO);
}

long userfs_inode_cache_flush_page(inode_cache_t *cache, off_t pgoff, phyframe_t *page)
{
    userfs_t *userfs = userfs_get(cache->owner->superblock->fs, "flush_page", );

    long ret = 0;
    mosrpc_fs_putpage_request req = { 0 };
    req.i_ref = i_to_pb_ref(cache->owner);
    req.pgoff = pgoff;
    req.data = kmalloc(PB_BYTES_ARRAY_T_ALLOCSIZE(MOS_PAGE_SIZE));
    req.data->size = MOS_PAGE_SIZE;
    memcpy(req.data->bytes, (void *) phyframe_va(page), MOS_PAGE_SIZE);

    mosrpc_fs_putpage_response resp = { 0 };

    const pf_point_t pp = profile_enter();
    const int result = fs_client_put_page(userfs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.putpage", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_inode_cache_flush_page: failed to putpage: %d", result);
        ret = -EIO;
        goto bail_out;
    }

    if (!resp.result.success)
    {
        pr_dwarn(userfs, "userfs_inode_cache_flush_page: failed to putpage: %s", resp.result.error);
        ret = -EIO;
        goto bail_out;
    }

    ret = 0;

bail_out:
    pb_release(mosrpc_fs_putpage_request_fields, &req);
    pb_release(mosrpc_fs_putpage_response_fields, &resp);
    return ret;
}

static const inode_cache_ops_t userfs_inode_cache_ops = {
    .fill_cache = userfs_inode_cache_fill_cache,
    .page_write_begin = simple_page_write_begin,
    .page_write_end = simple_page_write_end,
    .flush_page = userfs_inode_cache_flush_page,
};

long userfs_sync_inode(inode_t *inode)
{
    userfs_t *userfs = userfs_get(inode->superblock->fs, "sync_inode: %llu", inode->ino);

    mosrpc_fs_sync_inode_request req = { 0 };
    req.i_ref = i_to_pb_ref(inode);
    req.i_info = *i_to_pb_full(inode, &req.i_info);

    mosrpc_fs_sync_inode_response resp = { 0 };
    const pf_point_t pp = profile_enter();
    const int result = fs_client_sync_inode(userfs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.sync_inode", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_sync_inode: failed to sync inode %llu: %d", inode->ino, result);
        return -EIO;
    }

    if (!resp.result.success)
    {
        pr_dwarn(userfs, "userfs_sync_inode: failed to sync inode %llu: %s", inode->ino, resp.result.error);
        return -EIO;
    }

    return 0;
}

static const superblock_ops_t userfs_sb_ops = {
    .drop_inode = NULL,
    .sync_inode = userfs_sync_inode,
};

dentry_t *userfs_fsop_mount(filesystem_t *fs, const char *device, const char *options)
{
    userfs_t *userfs = userfs_get(fs, "mount: %s", fs->name);

    const mosrpc_fs_mount_request req = {
        .fs_name = (char *) fs->name,
        .device = (char *) device,
        .options = (char *) options,
    };

    mosrpc_fs_mount_response resp = { 0 };

    const pf_point_t pp = profile_enter();
    const int result = fs_client_mount(userfs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.mount", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_fsop_mount: failed to mount %s: %d", fs->name, result);
        pb_release(mosrpc_fs_mount_response_fields, &resp);
        return ERR_PTR(-EIO);
    }

    if (!resp.result.success)
    {
        pr_warn("userfs_fsop_mount: failed to mount %s: %s", fs->name, resp.result.error);
        pb_release(mosrpc_fs_mount_response_fields, &resp);
        return ERR_PTR(-EIO);
    }

    superblock_t *sb = kmalloc(superblock_cache);
    sb->ops = &userfs_sb_ops;

    inode_t *i = i_from_pbfull(&resp.root_info, sb, (void *) resp.root_ref.data);

    sb->fs = fs;
    sb->root = dentry_get_from_parent(sb, NULL, NULL);
    sb->root->superblock = i->superblock = sb;
    dentry_attach(sb->root, i);
    return sb->root;
}
