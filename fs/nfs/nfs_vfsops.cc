/*
 * Copyright (C) 2015 Scylla, Ltd.
 *
 * Based on ramfs code Copyright (c) 2006-2007, Kohsuke Ohtani
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <cassert>
#include <cerrno>

#include <osv/debug.hh>
#include <osv/vnode.h>
#include <osv/mount.h>
#include <osv/dentry.h>

#include <string>
#include <memory>

#include "../../external/fs/libnfs/include/nfsc/libnfs.h"

extern struct vnops nfs_vnops;

static int nfs_op_mount(struct mount *mp, const char *dev, int flags, const void *data);
static int nfs_op_unmount(struct mount *mp, int flags);
// For the following let's rely on operations on individual files
#define nfs_op_sync    ((vfsop_sync_t)vfs_nullop)
#define nfs_op_vget    ((vfsop_vget_t)vfs_nullop)
#define nfs_op_statfs  ((vfsop_statfs_t)vfs_nullop)

/*
 * File system operations
 */
struct vfsops nfs_vfsops = {
    nfs_op_mount,      /* mount */
    nfs_op_unmount,    /* unmount */
    nfs_op_sync,       /* sync */
    nfs_op_vget,       /* vget */
    nfs_op_statfs,     /* statfs */
    &nfs_vnops,        /* vnops */
};

static void nfs_mount_error(struct nfs_context *nfs)
{
    debug(std::string("nfs_mount: ") + nfs_get_error(nfs) + "\n");
    nfs_destroy_context(nfs);
}

static struct nfs_context *get_nfs_context(struct mount *mp)
{
    return static_cast<struct nfs_context *>(mp->m_root->d_vnode->v_data);
}

/*
 * Mount a file system.
 */
static int nfs_op_mount(struct mount *mp, const char *dev, int flags,
                        const void *data)
{
    struct nfs_context *nfs;
    int ret;

    assert(mp);

    debug("nfs_mount: " + std::string(dev) + "\n");

    /* Create the NFS context */
    nfs = nfs_init_context();
    if (!nfs) {
        debug("nfs_mount: failed to create NFS context\n");
        return ENOMEM;
    }

    // parse the url while taking care of freeing it when needed
    std::unique_ptr<struct nfs_url, void (*)(struct nfs_url *url)>
               url(nfs_parse_url_dir(nfs, mp->m_special), nfs_destroy_url);
    if (!url) {
        nfs_mount_error(nfs);
        return EINVAL;
    }

    // Do the mount
    ret = nfs_mount(nfs, url->server, url->path);
    if (ret) {
        nfs_mount_error(nfs);
        return -ret;
    }

    // save the NFS context into the mount point
    mp->m_root->d_vnode->v_data = nfs;
    return 0;
}

/*
 * Unmount a file system.
 *
 */
static int nfs_op_unmount(struct mount *mp, int flags)
{
    assert(mp);
    auto nfs = get_nfs_context(mp);
    nfs_destroy_context(nfs);
    mp->m_root->d_vnode->v_data = nullptr;
    return 0;
}

int nfs_init(void)
{
    return 0;
}
