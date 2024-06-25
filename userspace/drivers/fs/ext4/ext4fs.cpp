// SPDX-License-Identifier: GPL-3.0-or-later

#include "ext4fs.hpp"

#include "ext4_dir.h"
#include "ext4_inode.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <sys/stat.h>

static std::optional<mos_rpc_blockdev_blockdev> open_blockdev(const std::string &name)
{
    open_device::request req{ .device_name = strdup(name.c_str()) };
    open_device::response resp;
    const auto result = blockdev_manager->open_device(&req, &resp);

    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cerr << "Failed to open block device" << std::endl;
        if (resp.result.error)
            std::cerr << "Error: " << resp.result.error << std::endl;
        return std::nullopt;
    }

    auto dev = resp.device;
    pb_release(&mos_rpc_blockdev_open_device_request_msg, &req);
    pb_release(&mos_rpc_blockdev_open_device_response_msg, &resp);

    std::cout << "Block Device '" << name << "' opened" << std::endl;
    return dev;
}

static size_t blockdev_size(std::string_view name)
{
    struct stat st;
    if (stat(("/dev/block/" + std::string(name)).c_str(), &st) != 0)
    {
        std::cerr << "Failed to get block device size" << std::endl;
        return 0;
    }

    return st.st_size;
}

static int no_op(struct ext4_blockdev *)
{
    return 0;
}

static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
    const auto state = static_cast<ext4_context_state *>(bdev->bdif->p_user);

    read_block::request req{ .device = state->blockdev, .n_boffset = blk_id, .n_blocks = blk_cnt };
    read_block::response resp;

    const auto result = blockdev_manager->read_block(&req, &resp);

    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cerr << "Failed to read block" << std::endl;
        if (resp.result.error)
            std::cerr << "Error: " << resp.result.error << std::endl;
        return -1;
    }

    assert(resp.data->size == 512 * blk_cnt); // 512 bytes per block (hardcoded)

    memcpy(buf, resp.data->bytes, resp.data->size);

    pb_release(&mos_rpc_blockdev_read_block_request_msg, &req);
    pb_release(&mos_rpc_blockdev_read_block_response_msg, &resp);

    return 0;
}

static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
    MOS_UNUSED(bdev);
    MOS_UNUSED(buf);
    MOS_UNUSED(blk_id);
    MOS_UNUSED(blk_cnt);
    std::cout << "bwrite(" << blk_id << ", " << blk_cnt << ")" << std::endl;
    return 0;
}

Ext4UserFS::Ext4UserFS(const std::string &name) : IExt4Server(name)
{
}

void Ext4UserFS::on_connect(rpc_context_t *ctx)
{
    set_data(ctx, new ext4_context_state());
}

void Ext4UserFS::on_disconnect(rpc_context_t *ctx)
{
    delete get_data<ext4_context_state>(ctx);
}

void Ext4UserFS::populate_pb_inode_info(pb_inode_info &info, ext4_sblock *sb, ext4_inode *inode, int ino)
{
    info.ino = ino;
    info.perm = ext4_inode_get_mode(sb, inode);
    info.uid = ext4_inode_get_uid(inode);
    info.gid = ext4_inode_get_gid(inode);
    info.size = ext4_inode_get_size(sb, inode);
    info.accessed = ext4_inode_get_access_time(inode);
    info.modified = ext4_inode_get_modif_time(inode);
    info.created = ext4_inode_get_change_inode_time(inode);
    info.nlinks = ext4_inode_get_links_cnt(inode);
    info.type = ext4_get_file_type(sb, inode);

    // TODO: Implement the following fields
    info.sticky = false;
    info.suid = false;
    info.sgid = false;
};

rpc_result_code_t Ext4UserFS::mount(rpc_context_t *ctx, mos_rpc_fs_mount_request *req, mos_rpc_fs_mount_response *resp)
{
    if (req->fs_name != "userfs.ext4"s)
    {
        resp->result.success = false;
        resp->result.error = strdup("Invalid filesystem name");
        return RPC_RESULT_OK;
    }

    auto state = get_data<ext4_context_state>(ctx);

    const auto dev = open_blockdev(req->device);
    if (!dev)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to open block device");
        return RPC_RESULT_OK;
    }

    state->blockdev = dev.value();

    const auto devsize = blockdev_size(req->device);

    state->ext4_dev_iface.open = no_op;
    state->ext4_dev_iface.close = no_op;
    state->ext4_dev_iface.lock = no_op;
    state->ext4_dev_iface.unlock = no_op;
    state->ext4_dev_iface.bread = blockdev_bread;
    state->ext4_dev_iface.bwrite = blockdev_bwrite;
    state->ext4_dev_iface.ph_bsize = 512;
    state->ext4_dev_iface.ph_bcnt = devsize / 512;
    state->ext4_dev_iface.ph_bbuf = state->ext4_buf;
    state->ext4_dev_iface.p_user = state;

    state->ext4_dev.bdif = &state->ext4_dev_iface;
    state->ext4_dev.part_offset = 0;
    state->ext4_dev.part_size = devsize;

    ext4_device_register(&state->ext4_dev, "dev");

    if (ext4_mount("dev", "/", true))
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to mount ext4 filesystem");
        return RPC_RESULT_OK;
    }

    state->fs = state->ext4_dev.fs;
    state->mp = ext4_get_mount("/");

    ext4_inode_ref *root = new ext4_inode_ref;
    ext4_fs_get_inode_ref(state->fs, EXT4_ROOT_INO, root);
    populate_pb_inode_info(resp->root_info, &state->fs->sb, root->inode, EXT4_ROOT_INO);
    resp->root_ref.data = (ptr_t) root;

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::readdir(rpc_context_t *ctx, mos_rpc_fs_readdir_request *req, mos_rpc_fs_readdir_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);

    ext4_inode_ref *dir = (ext4_inode_ref *) req->i_ref.data;

    ext4_dir_iter iter;
    if (ext4_dir_iterator_init(&iter, dir, 0) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to initialize directory iterator");
        return RPC_RESULT_OK;
    }

    while (iter.curr != NULL)
    {
        if (ext4_dir_en_get_inode(iter.curr) != 0)
        {
            // Found a non-empty directory entry
            const auto de = iter.curr;

            pb_dirent dirent;
            dirent.ino = de->inode;
            dirent.name = strndup((char *) de->name, de->name_len);
            dirent.type = ext4_get_file_type(&state->fs->sb, de);

            resp->entries_count++;
            resp->entries = (pb_dirent *) realloc(resp->entries, resp->entries_count * sizeof(pb_dirent));
            resp->entries[resp->entries_count - 1] = dirent;
        }

        if (ext4_dir_iterator_next(&iter) != EOK)
            break; // got some error
    }

    if (ext4_dir_iterator_fini(&iter) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to finalize directory iterator");
        return RPC_RESULT_OK;
    }

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::lookup(rpc_context_t *ctx, mos_rpc_fs_lookup_request *req, mos_rpc_fs_lookup_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);
    auto inode_ref = (ext4_inode_ref *) req->i_ref.data;

    ext4_dir_search_result result;
    if (ext4_dir_find_entry(&result, inode_ref, req->name, strlen(req->name)) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to find directory entry");
        return RPC_RESULT_OK;
    }

    ext4_inode_ref *sub_inode = new ext4_inode_ref;
    if (ext4_fs_get_inode_ref(state->fs, result.dentry->inode, sub_inode) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    resp->i_ref.data = (ptr_t) sub_inode;
    populate_pb_inode_info(resp->i_info, &state->fs->sb, sub_inode->inode, result.dentry->inode);

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::readlink(rpc_context_t *ctx, mos_rpc_fs_readlink_request *req, mos_rpc_fs_readlink_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);
    auto inode_ref = (ext4_inode_ref *) req->i_ref.data;

    size_t file_size = ext4_inode_get_size(&state->fs->sb, inode_ref->inode);
    if (file_size == 0)
    {
        resp->result.success = false;
        resp->result.error = strdup("File is empty");
        return RPC_RESULT_OK;
    }

    ext4_file file = {
        .mp = state->mp,
        .inode = inode_ref->index,
        .flags = O_RDONLY,
        .fsize = file_size,
        .fpos = 0,
    };

    char buf[MOS_PATH_MAX_LENGTH] = { 0 };
    size_t read_cnt;
    if (ext4_fread(&file, buf, MOS_PATH_MAX_LENGTH, &read_cnt) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to read file");
        return RPC_RESULT_OK;
    }

    resp->target = strdup(buf);

    resp->result.success = true;
    resp->result.error = nullptr;

    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::getpage(rpc_context_t *ctx, mos_rpc_fs_getpage_request *req, mos_rpc_fs_getpage_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);
    auto inode_ref = (ext4_inode_ref *) req->i_ref.data;

    ext4_inode *inode = inode_ref->inode;
    size_t file_size = ext4_inode_get_size(&state->fs->sb, inode);
    if (file_size == 0)
    {
        resp->result.success = false;
        resp->result.error = strdup("File is empty");
        return RPC_RESULT_OK;
    }

    ext4_file file = {
        .mp = state->mp,
        .inode = inode_ref->index,
        .flags = O_RDONLY,
        .fsize = file_size,
        .fpos = req->pgoff * MOS_PAGE_SIZE,
    };

    size_t read_size = std::min((size_t) MOS_PAGE_SIZE, file_size - file.fpos);

    resp->data = (pb_bytes_array_t *) malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(read_size));
    size_t read_cnt;
    if (ext4_fread(&file, resp->data->bytes, read_size, &read_cnt) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to read file");
        return RPC_RESULT_OK;
    }

    resp->data->size = read_cnt;

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}
