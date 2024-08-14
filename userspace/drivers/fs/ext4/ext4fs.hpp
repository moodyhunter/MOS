// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "blockdev.h"
#include "ext4.h"
#include "ext4_blockdev.h"
#include "ext4_types.h"
#include "proto/blockdev.pb.h"
#include "proto/blockdev.services.h"
#include "proto/filesystem.pb.h"
#include "proto/filesystem.services.h"
#include "proto/userfs-manager.services.h"

#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <memory>
#include <mos/filesystem/fs_types.h>
#include <mos/proto/fs_server.h>
#include <pb_decode.h>

RPC_DECL_SERVER_INTERFACE_CLASS(IExt4Server, USERFS_SERVICE_X);
RPC_CLIENT_DEFINE_STUB_CLASS(BlockdevManager, BLOCKDEVMANAGER_SERVICE_X);
RPC_CLIENT_DEFINE_STUB_CLASS(UserFSManager, USERFSMANAGER_SERVICE_X);

using namespace std::string_literals;
using namespace mosrpc::blockdev;

extern std::unique_ptr<UserFSManager> userfs_manager;
extern std::unique_ptr<BlockdevManager> blockdev_manager;

struct ext4_context_state
{
    mosrpc_blockdev_blockdev blockdev;

    uint8_t ext4_buf[512] = { 0 };
    ext4_blockdev_iface ext4_dev_iface;
    ext4_blockdev ext4_dev;
    ext4_fs *fs;
    ext4_mountpoint *mp;
};

class Ext4UserFS : public IExt4Server
{
  public:
    explicit Ext4UserFS(const std::string &name);

  private:
    virtual void on_connect(rpc_context_t *ctx) override;

    virtual void on_disconnect(rpc_context_t *ctx) override;

  private:
    template<typename T>
    static file_type_t ext4_get_file_type(ext4_sblock *sb, T *dentry_or_inode)
    {
        if constexpr (std::is_same_v<T, ext4_dir_en>)
        {
            const auto type = ext4_dir_en_get_inode_type(sb, dentry_or_inode);
            switch (type)
            {
                case EXT4_DE_FIFO: return FILE_TYPE_NAMED_PIPE;
                case EXT4_DE_CHRDEV: return FILE_TYPE_CHAR_DEVICE;
                case EXT4_DE_DIR: return FILE_TYPE_DIRECTORY;
                case EXT4_DE_BLKDEV: return FILE_TYPE_BLOCK_DEVICE;
                case EXT4_DE_REG_FILE: return FILE_TYPE_REGULAR;
                case EXT4_DE_SYMLINK: return FILE_TYPE_SYMLINK;
                case EXT4_DE_SOCK: return FILE_TYPE_SOCKET;
                case EXT4_DE_UNKNOWN: return FILE_TYPE_UNKNOWN;
                default: return FILE_TYPE_UNKNOWN;
            }
        }
        else if constexpr (std::is_same_v<T, ext4_inode>)
        {
            const auto type = ext4_inode_type(sb, dentry_or_inode);
            switch (type)
            {
                case EXT4_INODE_MODE_FIFO: return FILE_TYPE_NAMED_PIPE;
                case EXT4_INODE_MODE_CHARDEV: return FILE_TYPE_CHAR_DEVICE;
                case EXT4_INODE_MODE_DIRECTORY: return FILE_TYPE_DIRECTORY;
                case EXT4_INODE_MODE_BLOCKDEV: return FILE_TYPE_BLOCK_DEVICE;
                case EXT4_INODE_MODE_FILE: return FILE_TYPE_REGULAR;
                case EXT4_INODE_MODE_SOFTLINK: return FILE_TYPE_SYMLINK;
                case EXT4_INODE_MODE_SOCKET: return FILE_TYPE_SOCKET;
                default: return FILE_TYPE_UNKNOWN;
            }
        }
        else
        {
            static_assert(false, "Invalid type");
        }
    };

    static void populate_mosrpc_fs_inode_info(mosrpc_fs_inode_info &info, ext4_sblock *sb, ext4_inode *inode, int ino);
    static void save_inode_info(ext4_sblock *sb, ext4_inode *inode, const mosrpc_fs_inode_info &info);

  private:
    virtual rpc_result_code_t mount(rpc_context_t *ctx, mosrpc_fs_mount_request *req, mosrpc_fs_mount_response *resp) override;

    virtual rpc_result_code_t readdir(rpc_context_t *ctx, mosrpc_fs_readdir_request *req, mosrpc_fs_readdir_response *resp) override;

    virtual rpc_result_code_t lookup(rpc_context_t *ctx, mosrpc_fs_lookup_request *req, mosrpc_fs_lookup_response *resp) override;

    virtual rpc_result_code_t readlink(rpc_context_t *ctx, mosrpc_fs_readlink_request *req, mosrpc_fs_readlink_response *resp) override;

    virtual rpc_result_code_t get_page(rpc_context_t *ctx, mosrpc_fs_getpage_request *req, mosrpc_fs_getpage_response *resp) override;

    virtual rpc_result_code_t create_file(rpc_context_t *ctx, mosrpc_fs_create_file_request *req, mosrpc_fs_create_file_response *resp) override;

    virtual rpc_result_code_t put_page(rpc_context_t *ctx, mosrpc_fs_putpage_request *req, mosrpc_fs_putpage_response *resp) override;

    virtual rpc_result_code_t sync_inode(rpc_context_t *ctx, mosrpc_fs_sync_inode_request *req, mosrpc_fs_sync_inode_response *resp) override;

    virtual rpc_result_code_t unlink(rpc_context_t *ctx, mosrpc_fs_unlink_request *req, mosrpc_fs_unlink_response *resp) override;
};
