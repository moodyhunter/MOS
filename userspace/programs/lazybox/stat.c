// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <path>...\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        file_stat_t statbuf;
        if (!lstatat(FD_CWD, argv[i], &statbuf))
        {
            fprintf(stderr, "%s: No such file or directory\n", argv[i]);
            return 1;
        }

        printf("File: %s\n", argv[i]);
        printf("File size: %zd bytes\n", statbuf.size);
        char link_target[MOS_PATH_MAX_LENGTH] = { 0 };
        switch (statbuf.type)
        {
            case FILE_TYPE_REGULAR: puts("Type: Regular file"); break;
            case FILE_TYPE_DIRECTORY: puts("Type: Directory"); break;
            case FILE_TYPE_CHAR_DEVICE: puts("Type: Character device"); break;
            case FILE_TYPE_BLOCK_DEVICE: puts("Type: Block device"); break;
            case FILE_TYPE_NAMED_PIPE: puts("Type: Pipe"); break;
            case FILE_TYPE_SOCKET: puts("Type: Socket"); break;
            case FILE_TYPE_SYMLINK:
            {
                puts("Type: Symbolic link");
                const size_t size = syscall_vfs_readlinkat(FD_CWD, argv[i], link_target, sizeof(link_target));
                if (size == (size_t) -1)
                {
                    puts("readlink failed");
                    return 1;
                }
                printf("Link target: %s\n", link_target);
                break;
            }
            default: puts("Type: Unknown"); break;
        }

        printf("Owner: %u:%u\n", statbuf.uid, statbuf.gid);

        char buf[16] = { 0 };
        file_format_perm(statbuf.perm, buf);
        printf("Permissions: %.9s", buf);

        if (statbuf.suid)
            printf("[SUID]");
        if (statbuf.sgid)
            printf("[SGID]");
        if (statbuf.sticky)
            printf("[STICKY]");

        printf("\n");

        printf("Inode: %llu\n", statbuf.ino);
        printf("Links: %ld\n", statbuf.nlinks);
        printf("\n");
    }

    return 0;
}
