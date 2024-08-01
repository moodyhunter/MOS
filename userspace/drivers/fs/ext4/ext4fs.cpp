// SPDX-License-Identifier: GPL-3.0-or-later

#include "ext4fs.hpp"

#include "ext4.h"
#include "ext4_blockdev.h"
#include "ext4_dir.h"
#include "ext4_fs.h"
#include "ext4_inode.h"
#include "ext4_types.h"
#include "proto/filesystem.pb.h"

#include <cassert>
#include <iostream>
#include <librpc/rpc.h>
#include <optional>
#include <string>
#include <sys/stat.h>

// clang-format off
u64 inode_index_from_data(pb_inode_ref &ref) { return ref.data; }

pb_inode_ref make_inode_ref(ext4_inode_ref &ref) { return { .data = ref.index }; }

pb_inode_ref make_inode_ref(u64 index) { return { .data = index }; }
// clang-format on

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
        return EIO;
    }

    assert(resp.data->size == 512 * blk_cnt); // 512 bytes per block (hardcoded)

    memcpy(buf, resp.data->bytes, resp.data->size);

    pb_release(&mos_rpc_blockdev_read_block_request_msg, &req);
    pb_release(&mos_rpc_blockdev_read_block_response_msg, &resp);

    return EOK;
}

static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
    const auto state = static_cast<ext4_context_state *>(bdev->bdif->p_user);

    const auto data_size = 512 * blk_cnt; // 512 bytes per block (hardcoded)

    write_block::request req{
        .device = state->blockdev,
        .data = (pb_bytes_array_t *) malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(data_size)),
        .n_boffset = blk_id,
        .n_blocks = blk_cnt,
    };

    // std::cout << "Writing " << blk_cnt << " blocks at offset " << blk_id << std::endl;
    // std::cout << "  buffer: " << buf << std::endl;
    // std::cout << "  to: " << req.data << std::endl;
    // std::cout << std::flush;

    req.data->size = data_size;
    memcpy(req.data->bytes, buf, data_size);

    write_block::response resp;

    const auto result = blockdev_manager->write_block(&req, &resp);

    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cerr << "Failed to write block" << std::endl;
        if (resp.result.error)
            std::cerr << "Error: " << resp.result.error << std::endl;

        pb_release(&mos_rpc_blockdev_write_block_request_msg, &req);
        pb_release(&mos_rpc_blockdev_write_block_response_msg, &resp);
        return EIO;
    }

    pb_release(&mos_rpc_blockdev_write_block_request_msg, &req);
    pb_release(&mos_rpc_blockdev_write_block_response_msg, &resp);

    // std::cout << "  ok" << std::endl;
    return EOK;
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

void Ext4UserFS::save_inode_info(ext4_sblock *sb, ext4_inode *inode, const pb_inode_info &info)
{
    ext4_inode_set_size(inode, info.size);
    ext4_inode_set_mode(sb, inode, info.perm);
    ext4_inode_set_uid(inode, info.uid);
    ext4_inode_set_gid(inode, info.gid);
    ext4_inode_set_access_time(inode, info.accessed);
    ext4_inode_set_modif_time(inode, info.modified);
    ext4_inode_set_change_inode_time(inode, info.created);
    ext4_inode_set_links_cnt(inode, info.nlinks);
}

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

    if (int retval = ext4_mount("dev", "/", false); retval != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup(strerror(retval));
        return RPC_RESULT_OK;
    }

    state->fs = state->ext4_dev.fs;

    if (state->fs->read_only)
    {
        resp->result.success = false;
        resp->result.error = strdup("Filesystem is read-only");
        return RPC_RESULT_OK;
    }

    state->mp = ext4_get_mount("/");

    ext4_inode_ref root;
    ext4_fs_get_inode_ref(state->fs, EXT4_ROOT_INO, &root);
    populate_pb_inode_info(resp->root_info, &state->fs->sb, root.inode, EXT4_ROOT_INO);
    ext4_fs_put_inode_ref(&root);
    resp->root_ref = make_inode_ref(root);

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::readdir(rpc_context_t *ctx, mos_rpc_fs_readdir_request *req, mos_rpc_fs_readdir_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);

    ext4_inode_ref dir;
    if (ext4_fs_get_inode_ref(state->fs, inode_index_from_data(req->i_ref), &dir) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    ext4_dir_iter iter;
    if (ext4_dir_iterator_init(&iter, &dir, 0) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to initialize directory iterator");
        ext4_fs_put_inode_ref(&dir);
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
        ext4_fs_put_inode_ref(&dir);
        return RPC_RESULT_OK;
    }

    ext4_fs_put_inode_ref(&dir);

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::lookup(rpc_context_t *ctx, mos_rpc_fs_lookup_request *req, mos_rpc_fs_lookup_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);

    ext4_inode_ref parent_inode_ref;
    if (ext4_fs_get_inode_ref(state->fs, inode_index_from_data(req->i_ref), &parent_inode_ref) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    ext4_dir_search_result result;
    if (ext4_dir_find_entry(&result, &parent_inode_ref, req->name, strlen(req->name)) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to find directory entry");
        return RPC_RESULT_OK;
    }

    ext4_inode_ref sub_inode;
    if (ext4_fs_get_inode_ref(state->fs, result.dentry->inode, &sub_inode) != EOK)
    {
        ext4_dir_destroy_result(&parent_inode_ref, &result);
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    resp->i_ref = make_inode_ref(result.dentry->inode);
    populate_pb_inode_info(resp->i_info, &state->fs->sb, sub_inode.inode, result.dentry->inode);
    ext4_dir_destroy_result(&parent_inode_ref, &result);
    ext4_fs_put_inode_ref(&sub_inode);
    ext4_fs_put_inode_ref(&parent_inode_ref);

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::readlink(rpc_context_t *ctx, mos_rpc_fs_readlink_request *req, mos_rpc_fs_readlink_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);

    ext4_inode_ref inode_ref;
    if (ext4_fs_get_inode_ref(state->fs, inode_index_from_data(req->i_ref), &inode_ref) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    size_t file_size = ext4_inode_get_size(&state->fs->sb, inode_ref.inode);
    if (file_size == 0)
    {
        resp->result.success = false;
        resp->result.error = strdup("File is empty");
        ext4_fs_put_inode_ref(&inode_ref);
        return RPC_RESULT_OK;
    }

    ext4_file file = {
        .mp = state->mp,
        .inode = inode_ref.index,
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
        ext4_fs_put_inode_ref(&inode_ref);
        return RPC_RESULT_OK;
    }

    resp->target = strdup(buf);

    resp->result.success = true;
    resp->result.error = nullptr;

    ext4_fs_put_inode_ref(&inode_ref);
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::getpage(rpc_context_t *ctx, mos_rpc_fs_getpage_request *req, mos_rpc_fs_getpage_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);
    ext4_inode_ref inode_ref;
    if (ext4_fs_get_inode_ref(state->fs, inode_index_from_data(req->i_ref), &inode_ref) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    ext4_inode *inode = inode_ref.inode;
    const size_t file_size = ext4_inode_get_size(&state->fs->sb, inode);

    ext4_file file = {
        .mp = state->mp,
        .inode = inode_ref.index,
        .flags = O_RDONLY,
        .fsize = file_size,
        .fpos = req->pgoff * MOS_PAGE_SIZE,
    };

    const size_t read_size = std::min((size_t) MOS_PAGE_SIZE, file_size - file.fpos);

    resp->data = (pb_bytes_array_t *) malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(read_size));
    size_t read_cnt = 0; // should zero-initialize if read_size is zero
    if (ext4_fread(&file, resp->data->bytes, read_size, &read_cnt) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to read file");
        ext4_fs_put_inode_ref(&inode_ref);
        return RPC_RESULT_OK;
    }

    assert(read_cnt <= MOS_PAGE_SIZE);
    resp->data->size = read_cnt;

    resp->result.success = true;
    resp->result.error = nullptr;
    ext4_fs_put_inode_ref(&inode_ref);
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::create_file(rpc_context_t *ctx, mos_rpc_fs_create_file_request *req, mos_rpc_fs_create_file_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);
    ext4_inode_ref inode_ref;
    if (ext4_fs_get_inode_ref(state->fs, inode_index_from_data(req->i_ref), &inode_ref) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    const auto ext4_ftype = [&]()
    {
        switch ((file_type_t) req->type)
        {
            case FILE_TYPE_REGULAR: return EXT4_DE_REG_FILE;
            case FILE_TYPE_DIRECTORY: return EXT4_DE_DIR;
            case FILE_TYPE_SYMLINK: return EXT4_DE_SYMLINK;
            case FILE_TYPE_CHAR_DEVICE: return EXT4_DE_CHRDEV;
            case FILE_TYPE_BLOCK_DEVICE: return EXT4_DE_BLKDEV;
            case FILE_TYPE_NAMED_PIPE: return EXT4_DE_FIFO;
            case FILE_TYPE_SOCKET: return EXT4_DE_SOCK;
            case FILE_TYPE_UNKNOWN: return EXT4_DE_UNKNOWN;
            default: return EXT4_DE_UNKNOWN;
        }
    }();

    ext4_inode_ref child_ref;
    int ret = ext4_fs_alloc_inode(state->fs, &child_ref, (int) ext4_ftype);
    if (ret != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to allocate inode");
        ext4_fs_put_inode_ref(&inode_ref);
        return RPC_RESULT_OK;
    }

    ext4_fs_inode_blocks_init(state->fs, &child_ref);

    ret = ext4_link(state->mp, &inode_ref, &child_ref, req->name, strlen(req->name), false);
    if (ret != EOK)
    {
        /*Fail. Free new inode.*/
        ext4_fs_free_inode(&child_ref);
        /*We do not want to write new inode. But block has to be released.*/
        child_ref.dirty = false;
        ext4_fs_put_inode_ref(&child_ref);
        ext4_fs_put_inode_ref(&inode_ref);
        resp->result.success = false;
        resp->result.error = strdup("Failed to link new inode");
        return RPC_RESULT_OK;
    }

    resp->i_ref = make_inode_ref(child_ref);
    populate_pb_inode_info(resp->i_info, &state->fs->sb, child_ref.inode, child_ref.index);
    ext4_fs_put_inode_ref(&child_ref);
    ext4_block_cache_flush(state->mp->bc.bdev);
    ext4_fs_put_inode_ref(&inode_ref);

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::putpage(rpc_context_t *ctx, mos_rpc_fs_putpage_request *req, mos_rpc_fs_putpage_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);
    ext4_inode_ref inode_ref;
    if (ext4_fs_get_inode_ref(state->fs, inode_index_from_data(req->i_ref), &inode_ref) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    ext4_inode *inode = inode_ref.inode;
    ext4_file file = {
        .mp = state->mp,
        .inode = inode_ref.index,
        .flags = O_WRONLY,
        .fsize = ext4_inode_get_size(&state->fs->sb, inode),
        .fpos = 0,
    };

    ext4_fseek(&file, req->pgoff * MOS_PAGE_SIZE, SEEK_SET);

    const auto write_pos = req->pgoff * MOS_PAGE_SIZE;

    // if pos is beyond the file size, we need to extend the file
    if (write_pos > file.fsize)
    {
        size_t pad_size = write_pos - file.fsize;
        while (pad_size > 0)
        {
            char pad[512] = { 0 };
            size_t written = 0;
            if (int err = ext4_fwrite(&file, pad, std::min(pad_size, sizeof(pad)), &written); err != EOK)
            {
                resp->result.success = false;
                resp->result.error = strdup((std::string("Failed to pad file: ") + strerror(err)).data());
                ext4_fs_put_inode_ref(&inode_ref);
                return RPC_RESULT_OK;
            }

            pad_size -= written;
        }
    }

    size_t written = 0;

    if (int err = ext4_fwrite(&file, req->data->bytes, req->data->size, &written); err != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup((std::string("Failed to write file: ") + strerror(err)).data());
        ext4_fs_put_inode_ref(&inode_ref);
        return RPC_RESULT_OK;
    }

    if (written != req->data->size)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to write all data");
        ext4_fs_put_inode_ref(&inode_ref);
        return RPC_RESULT_OK;
    }

    ext4_fs_put_inode_ref(&inode_ref);
    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::sync_inode(rpc_context_t *ctx, mos_rpc_fs_sync_inode_request *req, mos_rpc_fs_sync_inode_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);

    ext4_inode_ref inode_ref;
    if (ext4_fs_get_inode_ref(state->fs, req->i_info.ino, &inode_ref) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get inode reference");
        return RPC_RESULT_OK;
    }

    save_inode_info(&state->fs->sb, inode_ref.inode, req->i_info);
    inode_ref.dirty = true;
    ext4_fs_put_inode_ref(&inode_ref);
    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t Ext4UserFS::unlink(rpc_context_t *ctx, mos_rpc_fs_unlink_request *req, mos_rpc_fs_unlink_response *resp)
{
    auto state = get_data<ext4_context_state>(ctx);

    ext4_inode_ref dir;
    if (ext4_fs_get_inode_ref(state->fs, inode_index_from_data(req->i_ref), &dir) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get directory inode reference");
        return RPC_RESULT_OK;
    }

    ext4_inode_ref child;
    if (ext4_fs_get_inode_ref(state->fs, req->dentry.inode_id, &child) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to get child inode reference");
        return RPC_RESULT_OK;
    }

    if (ext4_unlink(state->mp, &dir, &child, req->dentry.name, strlen(req->dentry.name)) != EOK)
    {
        resp->result.success = false;
        resp->result.error = strdup("Failed to unlink child");
        return RPC_RESULT_OK;
    }

    ext4_fs_put_inode_ref(&child);
    ext4_fs_put_inode_ref(&dir);

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}
