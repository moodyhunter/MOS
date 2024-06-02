// SPDX-License-Identifier: GPL-3.0-or-later

#include "bootstrapper.h"
#include "cpiofs.h"
#include "proto/filesystem.pb.h"

#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server.h>
#include <mos/filesystem/fs_types.h>
#include <mos/mos_global.h>
#include <mos/proto/fs_server.h>
#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <sys/param.h>
#include <unistd.h>

#if !MOS_CONFIG(MOS_MAP_INITRD_TO_INIT)
#error "MOS_MAP_INITRD_TO_INIT must be enabled to use bootstrapper"
#endif

#define CPIOFS_NAME            "cpiofs"
#define CPIOFS_RPC_SERVER_NAME "fs.cpiofs"

RPC_CLIENT_DEFINE_SIMPLECALL(fs_manager, USERFS_MANAGER_X)

RPC_DECL_SERVER_PROTOTYPES(cpiofs, USERFS_IMPL_X)

static rpc_server_t *cpiofs = NULL;
static rpc_server_stub_t *fs_manager = NULL;

typedef struct
{
    pb_inode_info pb_i;
    size_t header_offset;
    size_t name_offset, name_length;
    size_t data_offset;
    cpio_header_t header;
} cpio_inode_t;

static file_type_t cpio_modebits_to_filetype(u32 modebits)
{
    file_type_t type = FILE_TYPE_UNKNOWN;
    switch (modebits & CPIO_MODE_FILE_TYPE)
    {
        case CPIO_MODE_FILE: type = FILE_TYPE_REGULAR; break;
        case CPIO_MODE_DIR: type = FILE_TYPE_DIRECTORY; break;
        case CPIO_MODE_SYMLINK: type = FILE_TYPE_SYMLINK; break;
        case CPIO_MODE_CHARDEV: type = FILE_TYPE_CHAR_DEVICE; break;
        case CPIO_MODE_BLOCKDEV: type = FILE_TYPE_BLOCK_DEVICE; break;
        case CPIO_MODE_FIFO: type = FILE_TYPE_NAMED_PIPE; break;
        case CPIO_MODE_SOCKET: type = FILE_TYPE_SOCKET; break;
        default: puts("invalid cpio file mode"); break;
    }

    return type;
}

static cpio_inode_t *cpio_trycreate_i(const char *path)
{
    cpio_header_t header;
    cpio_metadata_t metadata;
    const bool found = cpio_read_metadata(path, &header, &metadata);
    if (!found)
        return NULL;

    cpio_inode_t *cpio_inode = malloc(sizeof(cpio_inode_t));
    cpio_inode->header = header;
    cpio_inode->header_offset = metadata.header_offset;
    cpio_inode->name_offset = metadata.name_offset;
    cpio_inode->name_length = metadata.name_length;
    cpio_inode->data_offset = metadata.data_offset;

    const u32 modebits = strntoll(cpio_inode->header.mode, NULL, 16, sizeof(cpio_inode->header.mode) / sizeof(char));
    const u64 ino = strntoll(cpio_inode->header.ino, NULL, 16, sizeof(cpio_inode->header.ino) / sizeof(char));
    const file_type_t file_type = cpio_modebits_to_filetype(modebits & CPIO_MODE_FILE_TYPE);

    pb_inode_info *const i = &cpio_inode->pb_i;

    i->type = file_type;
    i->ino = ino;

    // 0000777 - The lower 9 bits specify read/write/execute permissions for world, group, and user following standard POSIX conventions.
    i->perm = modebits & 0777;
    i->size = metadata.data_length;
    i->uid = strntoll(cpio_inode->header.uid, NULL, 16, sizeof(cpio_inode->header.uid) / sizeof(char));
    i->gid = strntoll(cpio_inode->header.gid, NULL, 16, sizeof(cpio_inode->header.gid) / sizeof(char));
    i->sticky = modebits & CPIO_MODE_STICKY;
    i->suid = modebits & CPIO_MODE_SUID;
    i->sgid = modebits & CPIO_MODE_SGID;
    i->nlinks = strntoll(cpio_inode->header.nlink, NULL, 16, sizeof(cpio_inode->header.nlink) / sizeof(char));
    return cpio_inode;
}

static rpc_result_code_t cpiofs_mount(rpc_context_t *, mos_rpc_fs_mount_request *req, mos_rpc_fs_mount_response *resp)
{
    if (req->options && strlen(req->options) > 0 && strcmp(req->options, "defaults") != 0)
        printf("cpio: mount option '%s' is not supported\n", req->options);

    if (req->device && strlen(req->device) > 0 && strcmp(req->device, "none") != 0)
        printf("cpio: mount: device name '%s' is not supported\n", req->device);

    cpio_inode_t *cpio_i = cpio_trycreate_i(".");
    if (!cpio_i)
    {
        puts("cpio: failed to mount");
        resp->result.error = strdup("unable to find root inode");
        resp->result.success = false;
        return RPC_RESULT_OK;
    }

    resp->result.success = true;
    resp->root_info = cpio_i->pb_i;
    resp->root_ref.data = (ptr_t) cpio_i;
    return RPC_RESULT_OK;
}

static rpc_result_code_t cpiofs_readdir(rpc_context_t *, mos_rpc_fs_readdir_request *req, mos_rpc_fs_readdir_response *resp)
{
    cpio_inode_t *inode = (cpio_inode_t *) req->i_ref.data;

    char path_prefix[inode->name_length + 1]; // +1 for null terminator
    read_initrd(path_prefix, inode->name_length, inode->name_offset);
    path_prefix[inode->name_length] = '\0';

    const size_t prefix_len = statement_expr(size_t, {
        retval = strlen(path_prefix);
        if (strcmp(path_prefix, ".") == 0)
            path_prefix[0] = '\0', retval = 0; // root directory
    });

    // find all children of this directory, that starts with 'path' and doesn't have any more slashes

    size_t n_written = 0;
    resp->entries_count = 1;
    resp->entries = malloc(sizeof(pb_dirent) * resp->entries_count);

    size_t offset = 0;

    while (true)
    {
        cpio_header_t header;
        read_initrd(&header, sizeof(cpio_header_t), offset);
        offset += sizeof(cpio_header_t);

        if (strncmp(header.magic, "07070", 5) != 0 || (header.magic[5] != '1' && header.magic[5] != '2'))
            puts("invalid cpio header magic, possibly corrupt archive");

        const size_t fpath_len = strntoll(header.namesize, NULL, 16, sizeof(header.namesize) / sizeof(char));

        char fpath[fpath_len + 1]; // +1 for null terminator
        read_initrd(fpath, fpath_len, offset);
        fpath[fpath_len] = '\0';

        const bool found = strncmp(path_prefix, fpath, prefix_len) == 0 &&  // make sure the path starts with the parent path
                           (prefix_len == 0 || fpath[prefix_len] == '/') && // make sure it's a child, not a sibling (/path/to vs /path/toooo)
                           strchr(fpath + prefix_len + 1, '/') == NULL;     // make sure it's a direct child, not a grandchild (/path/to vs /path/to/ooo)

        const bool is_TRAILER = strcmp(fpath, "TRAILER!!!") == 0;
        const bool is_root_dot = strcmp(fpath, ".") == 0;

        if (found && !is_TRAILER && !is_root_dot)
        {
            const s64 ino = strntoll(header.ino, NULL, 16, sizeof(header.ino) / sizeof(char));
            const u32 modebits = strntoll(header.mode, NULL, 16, sizeof(header.mode) / sizeof(char));
            const file_type_t type = cpio_modebits_to_filetype(modebits & CPIO_MODE_FILE_TYPE);

            const char *fname = fpath + prefix_len + (prefix_len != 0);          // +1 for the slash if it's not the root
            const size_t fname_len = fpath_len - prefix_len - (prefix_len != 0); // -1 for the slash if it's not the root

            // write ino, name, name_len, type
            if (n_written >= resp->entries_count)
            {
                resp->entries_count *= 2;
                resp->entries = realloc(resp->entries, sizeof(pb_dirent) * resp->entries_count);
            }

            pb_dirent *const de = &resp->entries[n_written++];
            de->ino = ino;
            de->name = strndup(fname, fname_len);
            de->type = type;
        }

        if (unlikely(is_TRAILER))
            break; // coming to the end of the archive

        offset += fpath_len;
        offset = ALIGN_UP(offset, 4);

        const size_t data_len = strntoll(header.filesize, NULL, 16, sizeof(header.filesize) / sizeof(char));
        offset += data_len;
        offset = ALIGN_UP(offset, 4);
    }

    resp->entries_count = n_written; // not the same as resp->entries_count, because we might have allocated more than we needed
    return RPC_RESULT_OK;
}

static rpc_result_code_t cpiofs_lookup(rpc_context_t *, mos_rpc_fs_lookup_request *req, mos_rpc_fs_lookup_response *resp)
{
    char pathbuf[PATH_MAX] = { 0 };
    cpio_inode_t *parent_diri = (cpio_inode_t *) req->i_ref.data;
    read_initrd(pathbuf, parent_diri->name_length, parent_diri->name_offset);

    // append the filename (req->name)
    const size_t pathbuf_len = strlen(pathbuf);
    const size_t name_len = strlen(req->name);

    if (unlikely(pathbuf_len + name_len + 1 >= MOS_PATH_MAX_LENGTH))
    {
        puts("cpiofs_lookup: path too long");
        return false;
    }

    pathbuf[pathbuf_len] = '/';
    memcpy(pathbuf + pathbuf_len + 1, req->name, name_len);
    pathbuf[pathbuf_len + 1 + name_len] = '\0';

    const char *path = statement_expr(char *, {
        retval = pathbuf;
        if (pathbuf[0] == '.' && pathbuf[1] == '/')
            retval += 2; // skip the leading ./ (relative path)
    });

    cpio_inode_t *const cpio_i = cpio_trycreate_i(path);
    if (!cpio_i)
    {
        resp->result.success = false;
        resp->result.error = strdup("unable to find inode");
        return RPC_RESULT_OK;
    }

    resp->result.success = true;
    resp->i_info = cpio_i->pb_i;
    resp->i_ref.data = (ptr_t) cpio_i;
    return RPC_RESULT_OK;
}

static rpc_result_code_t cpiofs_readlink(rpc_context_t *, mos_rpc_fs_readlink_request *req, mos_rpc_fs_readlink_response *resp)
{
    cpio_inode_t *cpio_i = (cpio_inode_t *) req->i_ref.data;
    char path[cpio_i->data_offset + cpio_i->pb_i.size + 1];
    read_initrd(path, cpio_i->pb_i.size, cpio_i->data_offset);
    path[cpio_i->pb_i.size] = '\0';

    resp->result.success = true;
    resp->target = strdup(path);
    return RPC_RESULT_OK;
}

static rpc_result_code_t cpiofs_getpage(rpc_context_t *, mos_rpc_fs_getpage_request *req, mos_rpc_fs_getpage_response *resp)
{
    cpio_inode_t *cpio_i = (cpio_inode_t *) req->i_ref.data;

    if (req->pgoff * MOS_PAGE_SIZE >= cpio_i->pb_i.size)
    {
        resp->data = malloc(sizeof(pb_bytes_array_t));
        resp->data->size = 0;
        resp->result.success = true;
        return RPC_RESULT_OK;
    }

    const size_t bytes_to_read = MIN((size_t) MOS_PAGE_SIZE, cpio_i->pb_i.size - req->pgoff * MOS_PAGE_SIZE);

    resp->data = malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(bytes_to_read));
    resp->data->size = bytes_to_read;

    const size_t read = read_initrd(resp->data->bytes, bytes_to_read, cpio_i->data_offset + req->pgoff * MOS_PAGE_SIZE);
    if (read != bytes_to_read)
    {
        puts("cpiofs_getpage: failed to read page");
        resp->result.success = false;
        resp->result.error = strdup("failed to read page");

        free(resp->data);
        return RPC_RESULT_OK;
    }

    resp->result.success = true;
    return RPC_RESULT_OK;
}

void init_start_cpiofs_server(fd_t notifier)
{
    cpiofs = rpc_server_create(CPIOFS_RPC_SERVER_NAME, NULL);
    if (!cpiofs)
    {
        puts("cpiofs: failed to create cpiofs server");
        goto bad;
    }

    rpc_server_register_functions(cpiofs, cpiofs_functions, MOS_ARRAY_SIZE(cpiofs_functions));

    fs_manager = rpc_client_create(USERFS_SERVER_RPC_NAME);
    if (!fs_manager)
    {
        puts("cpiofs: failed to connect to the userfs manager");
        goto bad;
    }

    mos_rpc_fs_register_request req = mos_rpc_fs_register_request_init_zero;
    req.fs.name = CPIOFS_NAME;
    req.rpc_server_name = CPIOFS_RPC_SERVER_NAME;

    mos_rpc_fs_register_response resp = mos_rpc_fs_register_response_init_zero;
    const rpc_result_code_t result = fs_manager_register_fs(fs_manager, &req, &resp);
    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        puts("cpiofs: failed to register cpiofs with filesystem server");
        goto bad;
    }

    pb_release(mos_rpc_fs_register_response_fields, &resp);

    if (write(notifier, &(char){ 'v' }, 1) != 1)
    {
        puts("cpiofs: failed to notify init");
        goto bad2;
    }

    rpc_server_exec(cpiofs);
    rpc_server_destroy(cpiofs);
    return;

bad:
    if (write(notifier, &(char){ 'x' }, 1) != 1)
        puts("cpiofs: failed to notify init");
bad2:
    if (fs_manager)
        rpc_client_destroy(fs_manager);
    if (cpiofs)
        rpc_server_destroy(cpiofs);
}
