/*
 * Copyright (C) 2015 Scylla, Ltd.
 *
 * Based on ramfs code Copyright (c) 2006-2007, Kohsuke Ohtani
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/debug.hh>

#include <osv/mount.h>

#include "nfs.hh"

#include <iostream>

mount_context::mount_context(const char *url)
    : _valid(false)
    , _errno(0)
    , _nfs(nullptr, nfs_destroy_context)
    , _url(nullptr, nfs_destroy_url)
{
    /* Create the NFS context */
    _nfs.reset(nfs_init_context());
    if (!_nfs) {
        debug("mount_context(): failed to create NFS context\n");
        _errno = ENOMEM;
        return;
    }

    // parse the url while taking care of freeing it when needed
    _url.reset(nfs_parse_url_dir(_nfs.get(), url));
    if (!_url) {
        debug(std::string("mount_context():g: ") +
              nfs_get_error(_nfs.get()) + "\n");
        _errno = EINVAL;
        return;
    }

    _errno = -nfs_mount(_nfs.get(), _url->server, _url->path);
    if (_errno) {
        return;
    }

    _valid = true;
}

struct mount_context *get_mount_context(struct mount *mp)
{
    return static_cast<struct mount_context *>(mp->m_root->d_vnode->v_data);
}
