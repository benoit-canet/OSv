/*
 * Copyright (C) 2015 Scylla, Ltd.
 *
 * Based on ramfs code Copyright (c) 2006-2007, Kohsuke Ohtani
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "nfs.hh"

struct mount_context *create_mount_context(std::string url, int &err_no)
{
    err_no = 0;

    /* Create the NFS context */
    std::unique_ptr<struct nfs_context,
                    void (*)(struct nfs_context *nfs_context)>
                        nfs(nfs_init_context(), nfs_destroy_context);
    if (!nfs) {
        debug("create_mount_context: failed to create NFS context\n");
        err_no = ENOMEM;
        return nullptr;
    }

    // parse the url while taking care of freeing it when needed
    std::unique_ptr<struct nfs_url, void (*)(struct nfs_url *url)>
               url(nfs_parse_url_dir(nfs, mp->m_special), nfs_destroy_url);
    if (!url) {
        debug(std::string("create_mount_context: ") + nfs_get_error(nfs) +
              "\n");
        err_no = EINVAL;
        return nullptr;
    }

    return mount_context(std::move(nfs), std::move(url);
}

struct mount_context *get_mount_context(struct mount *mp)
{
    return static_cast<struct mount_context *>(mp->m_root->d_vnode->v_data);
}
