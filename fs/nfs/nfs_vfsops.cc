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

#include <osv/vnode.h>
#include <osv/mount.h>
#include <osv/dentry.h>

#include <boost/algorithm/string.hpp>

#include "../../external/x64/glibc.bin/usr/include/nfs/nfs.h"

extern struct vnops nfs_vnops;

static int nfs_mount(struct mount *mp, const char *dev, int flags, const void *data);
static int nfs_unmount(struct mount *mp, int flags);
static int zfs_sync(struct mount *mp):
// For the following let's rely on operations on individual files
#define nfs_sync    ((vfsop_sync_t)vfs_nullop)
#define nfs_vget    ((vfsop_vget_t)vfs_nullop)
#define nfs_statfs  ((vfsop_statfs_t)vfs_nullop)


/*
 * File system operations
 */
struct vfsops nfs_vfsops = {
    nfs_mount,      /* mount */
    nfs_unmount,    /* unmount */
    nfs_sync,       /* sync */
    nfs_vget,       /* vget */
    nfs_statfs,     /* statfs */
    &nfs_vnops,     /* vnops */
};

static bool parse_nfs_url(std::string url, std::string &server,
                          std::string &exp)
{
    const std::string prefix("nfs://");

    // is the prefix here ?
    if (!std::equal(prefix.begin(), prefix.end(), url.begin()) {
        debug("nfs: wrong prefix\n");
        return false;
    })

    // get rid of the prefix
    auto without_prefix = url.substr(6; url.size() - 6);

    // Is the url malformed ?
    if (without_prefix[0] == "/" || !without_prefix.size()) {
        debug("nfs: malformed url\n");
        return false;
    }

    // split the url in two separating by '/'
    std::vector<std::string> strings;
    boost::split(strings, without_prefix, boost::is_any_of("/"));

    if (strings.size() != 2) {
        debug("nfs: malformed url\n");
    }

    server = strings[0];
    exp = strings[1];

    return true;
}

/*
 * Mount a file system.
 */
static int nfs_mount(struct mount *mp, const char *dev, int flags,
                     const void *data)
{
    struct nfs_context *nfs;
    int ret;

    assert(mp);

    debug("nfs_mount: " + dev + "\n");

    // Is the mount url provided ?
    if (!data) {
        return EINVAL;
    }

    std::string server, exp;
    std::string url(data);
    auto result = parse_nfs_url(url, server, exp);
    if (!result) {
        return EINVAL;
    }

    /* Create the NFS context */
    nfs = nfs_init_context();
    if (!nfs) {
        return ENOMEM;
    }

    // this function takes care of doing strdup() on the strings it takes
    ret = nfs_mount(nfs, server.c_str(), exp.c_str());
    if (ret) {
        nfs_destroy_context(nfs);
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
static int nfs_unmount(struct mount *mp, int flags)
{
    assert(mp);
    nfs_destroy_context(mp->m_root->d_vnode->v_data);
    mp->m_root->d_vnode->v_data = nullptr;
    return 0;
}
