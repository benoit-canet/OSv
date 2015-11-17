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

#include "nfs.hh"

extern struct vnops nfs_vnops;

/*
 * Mount a file system.
 */
static int nfs_op_mount(struct mount *mp, const char *dev, int flags,
                        const void *data)
{
    assert(mp);

    debug("nfs_mount: " + std::string(dev) + "\n");

    auto ctx = new mount_context(mp->m_special);

    int err_no;
    if (!ctx->is_valid(err_no)) {
        delete ctx;
        return err_no;
    }

    // save the NFS context into the mount point
    mp->m_root->d_vnode->v_data = ctx;

    return 0;
}

/*
 * Unmount a file system.
 *
 * Note: There is no nfs_unmount in nfslib.
 *
 */
static int nfs_op_unmount(struct mount *mp, int flags)
{
    assert(mp);

    delete get_mount_context(mp);

    mp->m_root->d_vnode->v_data = nullptr;

    return 0;
}

int nfs_init(void)
{
    return 0;
}

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

