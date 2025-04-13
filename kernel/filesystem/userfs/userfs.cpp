// SPDX-License-Identifier: GPL-3.0-or-later
// userspace filesystems

#include "mos/filesystem/userfs/userfs.hpp"

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/filesystem/vfs_utils.hpp"
#include "mos/misc/profiling.hpp"
#include "proto/filesystem.pb.h"
#include "proto/filesystem.service.h"

#include <algorithm>
#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server.h>
#include <mos/filesystem/fs_types.h>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>
#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>

MOS_RPC_USERFS_CLIENT(fs_client)

extern const inode_ops_t userfs_iops;
extern const file_ops_t userfs_fops;
extern const inode_cache_ops_t userfs_inode_cache_ops;
extern const superblock_ops_t userfs_sb_ops;

template<typename... TPrint>
userfs_t *userfs_get(filesystem_t *fs, TPrint... print)
{
    const auto _userfs = container_of(fs, userfs_t, fs);
    userfs_ensure_connected(_userfs);
    auto stream = dInfo2<userfs> << "calling '" << fs->name << "' (rpc_server '" << _userfs->rpc_server_name << "'): ";
    ((stream << print), ...);
    return _userfs;
}

struct AutoCleanup
{
    const pb_msgdesc_t *const fields;
    void *data;

    void skip()
    {
        data = nullptr;
    }

    explicit AutoCleanup(const pb_msgdesc_t *fields, void *data) : fields(fields), data(data) {};
    ~AutoCleanup()
    {
        if (fields && data)
            pb_release(fields, data);
    }
};

inode_t *i_from_pbfull(const mosrpc_fs_inode_info *stat, superblock_t *sb, void *private_data)
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
    i->private_data = private_data;
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

    userfs->rpc_server = rpc_client_create(userfs->rpc_server_name.c_str());
    if (unlikely(!userfs->rpc_server))
    {
        mWarn << "userfs_ensure_connected: failed to connect to " << userfs->rpc_server_name;
        return;
    }
}

static bool userfs_iop_hardlink(dentry_t *d, inode_t *i, dentry_t *new_d)
{
    MOS_UNUSED(i);
    MOS_UNUSED(new_d);

    const auto name = dentry_name(d);
    userfs_t *userfs = userfs_get(d->superblock->fs, "hardlink", name);
    MOS_UNUSED(userfs);

    return false;
}

static void userfs_iop_iterate_dir(dentry_t *dentry, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
    const auto name = dentry_name(dentry);
    userfs_t *ufs = userfs_get(dentry->superblock->fs, "iterate_dir", name);

    mosrpc_fs_readdir_request req = { .i_ref = i_to_pb_ref(dentry->inode) };
    mosrpc_fs_readdir_response resp = {};

    const pf_point_t ev = profile_enter();
    const int result = fs_client_readdir(ufs->rpc_server, &req, &resp);
    profile_leave(ev, "userfs.'%s'.readdir", userfs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_readdir_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_iop_iterate_dir: failed to readdir " << name << ": " << result;
        return;
    }

    if (!resp.entries_count)
    {
        dWarn<userfs> << "userfs_iop_iterate_dir: failed to readdir " << name << ": " << resp.result.error;
        return;
    }

    for (size_t i = 0; i < resp.entries_count; i++)
    {
        const mosrpc_fs_pb_dirent *pbde = &resp.entries[i];
        MOS_ASSERT(pbde->name);
        add_record(state, pbde->ino, pbde->name, (file_type_t) pbde->type);
    }
}

static bool userfs_iop_lookup(inode_t *dir, dentry_t *dentry)
{
    const auto name = dentry_name(dentry);
    userfs_t *fs = userfs_get(dir->superblock->fs, "lookup", name);

    mosrpc_fs_lookup_request req = {
        .i_ref = i_to_pb_ref(dir),
        .name = (char *) name.c_str(),
    };

    mosrpc_fs_lookup_response resp = {};
    const pf_point_t ev = profile_enter();
    const int result = fs_client_lookup(fs->rpc_server, &req, &resp);
    profile_leave(ev, "userfs.'%s'.lookup", ufs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_lookup_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_iop_lookup: failed to lookup " << name << ": " << result;
        return false;
    }

    if (!resp.result.success)
    {
        return false; // ENOENT is not a big deal
    }

    inode_t *i = i_from_pbfull(&resp.i_info, dir->superblock, (void *) resp.i_ref.data);
    dentry_attach(dentry, i);
    dentry->superblock = i->superblock = dir->superblock;
    i->ops = &userfs_iops;
    i->cache.ops = &userfs_inode_cache_ops;
    i->file_ops = &userfs_fops;
    return true;
}

static bool userfs_iop_mkdir(inode_t *dir, dentry_t *dentry, file_perm_t perm)
{
    const auto name = dentry_name(dentry);
    userfs_t *fs = userfs_get(dir->superblock->fs, "mkdir", name);

    mosrpc_fs_make_dir_request req = {
        .i_ref = i_to_pb_ref(dir),
        .name = (char *) name.c_str(),
        .perm = perm,
    };
    mosrpc_fs_make_dir_response resp = {};

    const pf_point_t pp = profile_enter();
    const int result = fs_client_make_dir(fs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.make_dir", userfs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_make_dir_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_iop_mkdir: failed to mkdir " << name << ": " << result;
        return false;
    }

    if (!resp.result.success)
    {
        dWarn<userfs> << "userfs_iop_mkdir: failed to mkdir " << name << ": " << resp.result.error;
        return false;
    }

    inode_t *i = i_from_pbfull(&resp.i_info, dir->superblock, (void *) resp.i_ref.data);
    dentry_attach(dentry, i);
    dentry->superblock = i->superblock = dir->superblock;
    i->ops = &userfs_iops;
    i->cache.ops = &userfs_inode_cache_ops;
    i->file_ops = &userfs_fops;
    return true;
}

static bool userfs_iop_mknode(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm, dev_t dev)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(type);
    MOS_UNUSED(perm);
    MOS_UNUSED(dev);

    const auto name = dentry_name(dentry);
    userfs_t *userfs = userfs_get(dir->superblock->fs, "mknode", name);
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_newfile(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    const auto name = dentry_name(dentry);
    userfs_t *fs = userfs_get(dir->superblock->fs, "newfile", name);

    const mosrpc_fs_create_file_request req{
        .i_ref = i_to_pb_ref(dir),
        .name = (char *) name.c_str(),
        .type = type,
        .perm = perm,
    };
    mosrpc_fs_create_file_response resp = {};

    const pf_point_t pp = profile_enter();
    const int result = fs_client_create_file(fs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.create_file", userfs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_create_file_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_iop_newfile: failed to create file " << name << ": " << result;
        return false;
    }

    if (!resp.result.success)
    {
        dWarn<userfs> << "userfs_iop_newfile: failed to create file " << name << ": " << resp.result.error;
        return false;
    }

    inode_t *i = i_from_pbfull(&resp.i_info, dir->superblock, (void *) resp.i_ref.data);
    dentry_attach(dentry, i);
    dentry->superblock = i->superblock = dir->superblock;
    i->ops = &userfs_iops;
    i->cache.ops = &userfs_inode_cache_ops;
    i->file_ops = &userfs_fops;
    return true;
}

static size_t userfs_iop_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    const auto name = dentry_name(dentry);
    userfs_t *fs = userfs_get(dentry->superblock->fs, "readlink", name);

    const mosrpc_fs_readlink_request req = {
        .i_ref = i_to_pb_ref(dentry->inode),
    };
    mosrpc_fs_readlink_response resp = {};

    const pf_point_t pp = profile_enter();
    const int result = fs_client_readlink(fs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.readlink", userfs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_readlink_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_iop_readlink: failed to readlink " << name << ": " << result;
        return -EIO;
    }

    if (!resp.result.success)
    {
        dWarn<userfs> << "userfs_iop_readlink: failed to readlink " << name << ": " << resp.result.error;
        return -EIO;
    }

    size_t len = strlen(resp.target);
    if (len > buflen)
        len = buflen;

    memcpy(buffer, resp.target, len);
    return len;
}

static bool userfs_iop_rename(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry)
{
    MOS_UNUSED(old_dir);
    MOS_UNUSED(old_dentry);
    MOS_UNUSED(new_dir);
    MOS_UNUSED(new_dentry);

    const auto old_name = dentry_name(old_dentry);
    const auto new_name = dentry_name(new_dentry);
    userfs_t *userfs = userfs_get(old_dir->superblock->fs, "rename", old_name, " to ", new_name);
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_rmdir(inode_t *dir, dentry_t *dentry)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);

    const auto name = dentry_name(dentry);
    userfs_t *userfs = userfs_get(dir->superblock->fs, "rmdir", name);
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_symlink(inode_t *dir, dentry_t *dentry, const char *symname)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(symname);

    const auto name = dentry_name(dentry);
    userfs_t *userfs = userfs_get(dir->superblock->fs, "symlink", name);
    MOS_UNUSED(userfs);

    return false;
}

static bool userfs_iop_unlink(inode_t *dir, dentry_t *dentry)
{
    const auto name = dentry_name(dentry);
    userfs_t *fs = userfs_get(dir->superblock->fs, "unlink", name);

    const mosrpc_fs_unlink_request req = {
        .i_ref = i_to_pb_ref(dir),
        .dentry = { .inode_id = dentry->inode->ino, .name = (char *) name.c_str() },
    };
    mosrpc_fs_unlink_response resp = {};

    const pf_point_t pp = profile_enter();
    const int result = fs_client_unlink(fs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.unlink", userfs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_unlink_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_iop_unlink: failed to unlink " << name << ": " << result;
        return false;
    }

    if (!resp.result.success)
    {
        dWarn<userfs> << "userfs_iop_unlink: failed to unlink " << name << ": " << resp.result.error;
        return false;
    }

    return true;
}

const inode_ops_t userfs_iops = {
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

static bool userfs_fop_open(inode_t *inode, FsBaseFile *file, bool created)
{
    MOS_UNUSED(inode);
    MOS_UNUSED(file);
    MOS_UNUSED(created);

    return true;
}

const file_ops_t userfs_fops = {
    .open = userfs_fop_open,
    .read = vfs_generic_read,
    .write = vfs_generic_write,
    .release = NULL,
    .seek = NULL,
    .mmap = NULL,
    .munmap = NULL,
};

static PtrResult<phyframe_t> userfs_inode_cache_fill_cache(inode_cache_t *cache, uint64_t pgoff)
{
    userfs_t *fs = userfs_get(cache->owner->superblock->fs, "fill_cache");

    const mosrpc_fs_getpage_request req = {
        .i_ref = i_to_pb_ref(cache->owner),
        .pgoff = pgoff,
    };

    mosrpc_fs_getpage_response resp = {};

    const pf_point_t pp = profile_enter();
    const int result = fs_client_get_page(fs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.getpage", userfs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_getpage_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_inode_cache_fill_cache: failed to getpage: " << result;
        return -EIO;
    }

    if (!resp.result.success)
    {
        dWarn<userfs> << "userfs_inode_cache_fill_cache: failed to getpage: " << resp.result.error;
        return -EIO;
    }

    // allocate a page
    phyframe_t *page = pmm_ref_one(mm_get_free_page());
    if (!page)
    {
        mWarn << "userfs_inode_cache_fill_cache: failed to allocate page";
        return -ENOMEM;
    }

    // copy the data from the server
    memcpy((void *) phyframe_va(page), resp.data->bytes, std::min(resp.data->size, (pb_size_t) MOS_PAGE_SIZE));
    return page;
}

long userfs_inode_cache_flush_page(inode_cache_t *cache, uint64_t pgoff, phyframe_t *page)
{
    userfs_t *fs = userfs_get(cache->owner->superblock->fs, "flush_page");

    mosrpc_fs_putpage_request req = {
        .i_ref = i_to_pb_ref(cache->owner),
        .pgoff = pgoff,
        .data = nullptr,
    };
    req.data = (pb_bytes_array_t *) kcalloc<char>(PB_BYTES_ARRAY_T_ALLOCSIZE(MOS_PAGE_SIZE));
    req.data->size = MOS_PAGE_SIZE;
    memcpy(req.data->bytes, (void *) phyframe_va(page), MOS_PAGE_SIZE);

    mosrpc_fs_putpage_response resp = {};

    const pf_point_t pp = profile_enter();
    const int result = fs_client_put_page(fs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.putpage", userfs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_putpage_request_fields, &req);
    AutoCleanup cleanup2(mosrpc_fs_putpage_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_inode_cache_flush_page: failed to putpage: " << result;
        return -EIO;
    }

    if (!resp.result.success)
    {
        dWarn<userfs> << "userfs_inode_cache_flush_page: failed to putpage: " << resp.result.error;
        return -EIO;
    }

    return 0;
}

const inode_cache_ops_t userfs_inode_cache_ops = {
    .fill_cache = userfs_inode_cache_fill_cache,
    .page_write_begin = simple_page_write_begin,
    .page_write_end = simple_page_write_end,
    .flush_page = userfs_inode_cache_flush_page,
};

long userfs_sync_inode(inode_t *inode)
{
    userfs_t *fs = userfs_get(inode->superblock->fs, "sync_inode: %llu", inode->ino);

    mosrpc_fs_sync_inode_request req = {};
    req.i_ref = i_to_pb_ref(inode);
    req.i_info = *i_to_pb_full(inode, &req.i_info);

    mosrpc_fs_sync_inode_response resp = {};
    const pf_point_t pp = profile_enter();
    const int result = fs_client_sync_inode(fs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.sync_inode", userfs->rpc_server_name);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_sync_inode: failed to sync inode " << inode->ino << ": " << result;
        return -EIO;
    }

    if (!resp.result.success)
    {
        dWarn<userfs> << "userfs_sync_inode: failed to sync inode " << inode->ino << ": " << resp.result.error;
        return -EIO;
    }

    return 0;
}

const superblock_ops_t userfs_sb_ops = {
    .drop_inode = NULL,
    .sync_inode = userfs_sync_inode,
};

PtrResult<dentry_t> userfs_fsop_mount(filesystem_t *fs, const char *device, const char *options)
{
    userfs_t *userfs = userfs_get(fs, "mount", fs->name);

    const mosrpc_fs_mount_request req = {
        .fs_name = (char *) fs->name.c_str(),
        .device = (char *) device,
        .options = (char *) options,
    };

    mosrpc_fs_mount_response resp = {};

    const pf_point_t pp = profile_enter();
    const int result = fs_client_mount(userfs->rpc_server, &req, &resp);
    profile_leave(pp, "userfs.'%s'.mount", userfs->rpc_server_name);

    AutoCleanup cleanup(mosrpc_fs_mount_response_fields, &resp);

    if (result != RPC_RESULT_OK)
    {
        mWarn << "userfs_fsop_mount: failed to mount " << fs->name << ": " << result;
        return -EIO;
    }

    if (!resp.result.success)
    {
        mWarn << "userfs_fsop_mount: failed to mount " << fs->name << ": " << resp.result.error;
        return -EIO;
    }

    superblock_t *sb = mos::create<superblock_t>();
    sb->ops = &userfs_sb_ops;

    inode_t *i = i_from_pbfull(&resp.root_info, sb, (void *) resp.root_ref.data);

    sb->fs = fs;
    sb->root = dentry_get_from_parent(sb, NULL);
    sb->root->superblock = i->superblock = sb;
    dentry_attach(sb->root, i);
    return sb->root;
}
