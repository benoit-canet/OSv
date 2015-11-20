/*
 * Copyright (C) 2015 Scylla, Ltd.
 *
 * Based on ramfs code Copyright (c) 2006-2007, Kohsuke Ohtani
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <memory>
#include <string>

#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "../../external/fs/libnfs/include/nfsc/libnfs.h"

// Declared as a struct so every fields are public
struct mount_context {
    mount_context(std::unique_ptr<struct nfs_context,
                                  void (*)(struct nfs_context *nfs_context)>
                                        nfs_context,
                  std::unique_ptr<struct nfs_url, void (*)(struct nfs_url *url)>
                                        nfs_url): nfs(nfs_context), url(nfs_url)
    {
    };
    std::unique_ptr<struct nfs_context,
                    void (*)(struct nfs_context *nfs_context)> nfs;
    std::unique_ptr<struct nfs_url, void (*)(struct nfs_url *url)> url;
};

struct mount_context *create_mount_context(std::string url, int &err_no);

struct mount_context *get_mount_context(struct mount *mp);
