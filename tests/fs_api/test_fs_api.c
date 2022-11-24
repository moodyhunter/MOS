// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/tree.h"
#include "mos/filesystem/filesystem.h"
#include "mos/filesystem/pathutils.h"
#include "test_engine.h"
#include "test_engine_impl.h"

MOS_TEST_CASE(test_fs_api_path_is_prefix)
{
    fsnode_t root = {
        .name = "/",
        .tree_node = {
            .parent = tree_node(&root),
            .n_children = 0,
            .children = NULL,
        },
    };

    // /bin
    fsnode_t bin = {
        .name = "bin",
        .tree_node = {
            .parent = tree_node(&root),
            .n_children = 0,
            .children = NULL,
        },
    };

    // /bin/tools
    fsnode_t bin_tools = {
        .name = "tools",
        .tree_node = {
            .parent = tree_node(&bin),
            .n_children = 0,
            .children = NULL,
        },
    };

    // /temp
    fsnode_t temp = {
        .name = "temp",
        .tree_node = {
            .parent = tree_node(&root),
            .n_children = 0,
            .children = NULL,
        },
    };

    bool result = false;

    result = path_verify_prefix(&temp, &bin_tools);
    MOS_TEST_CHECK(result, false);

    result = path_verify_prefix(&bin_tools, &bin_tools);
    MOS_TEST_CHECK(result, true);

    result = path_verify_prefix(&bin_tools, &temp);
    MOS_TEST_CHECK(result, false);

    result = path_verify_prefix(&bin_tools, &root);
    MOS_TEST_CHECK(result, true);

    result = path_verify_prefix(&root, &root);
    MOS_TEST_CHECK(result, true);

    result = path_verify_prefix(&root, &bin);
    MOS_TEST_CHECK(result, false);

    result = path_verify_prefix(&bin, &root);
    MOS_TEST_CHECK(result, true);

    result = path_verify_prefix(&bin_tools, &bin);
    MOS_TEST_CHECK(result, true);
}
