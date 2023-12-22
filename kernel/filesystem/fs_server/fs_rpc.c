// SPDX-License-Identifier: GPL-3.0-or-later
// filesystem server RPCs

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/vfs.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/printk.h"
#include "mos/setup.h"
#include "mos/tasks/kthread.h"
#include "proto/filesystem.pb.h"

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

RPC_DECL_SERVER_PROTOTYPES(fs_manager, FS_MANAGER_X)

RPC_CLIENT_DEFINE_SIMPLECALL(fs_client, FS_IMPL_X)

typedef struct
{
    filesystem_t fs;
    const char *rpc_server_name;
    rpc_server_stub_t *rpc_server;
} userfs_t;

static slab_t *userfs_slab = NULL;
SLAB_AUTOINIT("userfs", userfs_slab, userfs_t);

static const inode_ops_t userfs_iops;
static const file_ops_t userfs_fops;

static inode_t *i_from_pb(const pb_inode *pbi, superblock_t *sb)
{
    // enum pb_file_type_t -> enum file_type_t is safe here because they have the same values
    inode_t *i = inode_create(sb, pbi->stat.ino, (file_type_t) pbi->stat.type);
    i->created = pbi->stat.created;
    i->modified = pbi->stat.modified;
    i->accessed = pbi->stat.accessed;
    i->size = pbi->stat.size;
    i->uid = pbi->stat.uid;
    i->gid = pbi->stat.gid;
    i->perm = pbi->stat.perm;
    i->nlinks = pbi->stat.nlinks;
    i->suid = pbi->stat.suid;
    i->sgid = pbi->stat.sgid;
    i->sticky = pbi->stat.sticky;
    i->private = (void *) pbi->private_data;
    i->ops = &userfs_iops;
    i->file_ops = &userfs_fops;
    return i;
}

static pb_inode *i_to_pb(const inode_t *i, pb_inode *pbi)
{
    pbi->stat.ino = i->ino;
    pbi->stat.type = (pb_file_type_t) i->type;
    pbi->stat.created = i->created;
    pbi->stat.modified = i->modified;
    pbi->stat.accessed = i->accessed;
    pbi->stat.size = i->size;
    pbi->stat.uid = i->uid;
    pbi->stat.gid = i->gid;
    pbi->stat.perm = i->perm;
    pbi->stat.nlinks = i->nlinks;
    pbi->stat.suid = i->suid;
    pbi->stat.sgid = i->sgid;
    pbi->stat.sticky = i->sticky;
    pbi->private_data = (ptr_t) i->private;
    return pbi;
}
static void userfs_ensure_connected(userfs_t *userfs)
{
    if (userfs->rpc_server)
        return;

    userfs->rpc_server = rpc_client_create(userfs->rpc_server_name);
    if (!userfs->rpc_server)
    {
        pr_warn("userfs_ensure_connected: failed to connect to %s", userfs->rpc_server_name);
        return;
    }
}

static bool userfs_iop_hardlink(dentry_t *d, inode_t *i, dentry_t *new_d)
{
    MOS_UNUSED(d);
    MOS_UNUSED(i);
    MOS_UNUSED(new_d);
    return false;
}

static void userfs_iop_iterate_dir(dentry_t *dentry, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
    userfs_t *userfs = container_of(dentry->superblock->fs, userfs_t, fs);
    mos_rpc_fs_readdir_request req = { 0 };
    i_to_pb(dentry->inode, &req.inode);

    mos_rpc_fs_readdir_response resp = { 0 };
    userfs_ensure_connected(userfs);
    int result = fs_client_readdir(userfs->rpc_server, &req, &resp);
    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_iop_iterate_dir: failed to readdir %s: %d", dentry_name(dentry), result);
        goto bail_out;
    }

    if (!resp.entries_count)
    {
        pr_warn("userfs_iop_iterate_dir: failed to readdir %s: %s", dentry_name(dentry), resp.result.error);
        goto bail_out;
    }

    for (size_t i = 0; i < resp.entries_count; i++)
    {
        const pb_dirent *pbde = &resp.entries[i];
        MOS_ASSERT(pbde->name);
        add_record(state, pbde->ino, pbde->name, strlen(pbde->name), (file_type_t) pbde->type);
    }

bail_out:
    pb_release(mos_rpc_fs_readdir_response_fields, &resp);
}

static bool userfs_iop_lookup(inode_t *dir, dentry_t *dentry)
{
    bool ret = false;
    userfs_t *userfs = container_of(dir->superblock->fs, userfs_t, fs);
    mos_rpc_fs_lookup_request req = { 0 };
    i_to_pb(dir, &req.inode);
    req.name = (char *) dentry_name(dentry);

    mos_rpc_fs_lookup_response resp = { 0 };
    userfs_ensure_connected(userfs);

    int result = fs_client_lookup(userfs->rpc_server, &req, &resp);
    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_iop_lookup: failed to lookup %s: %d", dentry_name(dentry), result);
        goto leave;
    }

    if (!resp.result.success)
    {
        pr_warn("userfs_iop_lookup: failed to lookup %s: %s", dentry_name(dentry), resp.result.error);
        goto leave;
    }

    inode_t *i = i_from_pb(&resp.inode, dir->superblock);
    dentry->inode = i;
    dentry->superblock = i->superblock = dir->superblock;
    i->ops = &userfs_iops;
    i->file_ops = &userfs_fops;
    ret = true;

leave:
    pb_release(mos_rpc_fs_lookup_response_fields, &resp);
    return ret;
}

static bool userfs_iop_mkdir(inode_t *dir, dentry_t *dentry, file_perm_t perm)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(perm);
    return false;
}

static bool userfs_iop_mknode(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm, dev_t dev)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(type);
    MOS_UNUSED(perm);
    MOS_UNUSED(dev);
    return false;
}

static bool userfs_iop_newfile(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(type);
    MOS_UNUSED(perm);
    return false;
}

static size_t userfs_iop_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    mos_rpc_fs_readlink_request req = { 0 };
    i_to_pb(dentry->inode, &req.inode);

    mos_rpc_fs_readlink_response resp = { 0 };
    userfs_t *userfs = container_of(dentry->superblock->fs, userfs_t, fs);
    userfs_ensure_connected(userfs);

    int result = fs_client_readlink(userfs->rpc_server, &req, &resp);
    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_iop_readlink: failed to readlink %s: %d", dentry_name(dentry), result);
        goto bail_out;
    }

    if (!resp.result.success)
    {
        pr_warn("userfs_iop_readlink: failed to readlink %s: %s", dentry_name(dentry), resp.result.error);
        goto bail_out;
    }

    size_t len = strlen(resp.target);
    if (len > buflen)
        len = buflen;

    memcpy(buffer, resp.target, len);
    return len;

bail_out:
    pb_release(mos_rpc_fs_readlink_response_fields, &resp);
    return -EIO;
}

static bool userfs_iop_rename(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry)
{
    MOS_UNUSED(old_dir);
    MOS_UNUSED(old_dentry);
    MOS_UNUSED(new_dir);
    MOS_UNUSED(new_dentry);
    return false;
}

static bool userfs_iop_rmdir(inode_t *dir, dentry_t *dentry)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    return false;
}

static bool userfs_iop_symlink(inode_t *dir, dentry_t *dentry, const char *symname)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    MOS_UNUSED(symname);
    return false;
}

static bool userfs_iop_unlink(inode_t *dir, dentry_t *dentry)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
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
    .flush = NULL,
    .release = NULL,
    .seek = NULL,
    .mmap = NULL,
    .munmap = NULL,
};

static dentry_t *userfs_fsop_mount(filesystem_t *fs, const char *device, const char *options)
{
    userfs_t *userfs = container_of(fs, userfs_t, fs);
    userfs_ensure_connected(userfs);

    const mos_rpc_fs_mount_request req = {
        .fs_name = (char *) fs->name,
        .device = (char *) device,
        .options = (char *) options,
    };

    mos_rpc_fs_mount_response resp = { 0 };
    int result = fs_client_mount(userfs->rpc_server, &req, &resp);
    if (result != RPC_RESULT_OK)
    {
        pr_warn("userfs_fsop_mount: failed to mount %s: %d", fs->name, result);
        pb_release(mos_rpc_fs_mount_response_fields, &resp);
        return ERR_PTR(-EIO);
    }

    if (!resp.result.success)
    {
        pr_warn("userfs_fsop_mount: failed to mount %s: %s", fs->name, resp.result.error);
        pb_release(mos_rpc_fs_mount_response_fields, &resp);
        return ERR_PTR(-EIO);
    }

    superblock_t *sb = kmalloc(superblock_cache);
    inode_t *i = i_from_pb(&resp.root_i, sb);

    sb->fs = fs;
    sb->root = dentry_create(sb, NULL, NULL);
    sb->root->inode = i;
    sb->root->superblock = i->superblock = sb;
    return sb->root;
}

static int fs_manager_register(rpc_server_t *server, mos_rpc_fs_register_request *req, mos_rpc_fs_register_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    userfs_t *userfs = kmalloc(userfs_slab);
    if (!userfs)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    size_t userfs_fsnamelen = strlen("userfs.") + strlen(req->fs.name) + 1;
    userfs->fs.name = kmalloc(userfs_fsnamelen);
    if (!userfs->fs.name)
    {
        kfree(userfs);
        return RPC_RESULT_SERVER_INTERNAL_ERROR;
    }

    snprintf((char *) userfs->fs.name, userfs_fsnamelen, "userfs.%s", req->fs.name);
    userfs->rpc_server_name = strdup(req->rpc_server_name);

    resp->result.success = true;

    userfs->fs.mount = userfs_fsop_mount;
    vfs_register_filesystem(&userfs->fs);
    return RPC_RESULT_OK;
}

static int fs_manager_unregister(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);
    MOS_UNUSED(args);

    return RPC_RESULT_OK;
}

static void fs_rpc_execute_server(void *arg)
{
    MOS_UNUSED(arg);
    rpc_server_t *fs_server = rpc_server_create(FS_SERVER_RPC_NAME, NULL);
    rpc_server_register_functions(fs_server, fs_manager_functions, MOS_ARRAY_SIZE(fs_manager_functions));
    rpc_server_exec(fs_server);
    pr_emerg("fs_rpc_execute_server exited");
}

void fs_rpc_init()
{
    kthread_create(fs_rpc_execute_server, NULL, "fs_rpc_server");
}

MOS_INIT(KTHREAD, fs_rpc_init);
