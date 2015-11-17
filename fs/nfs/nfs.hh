/*
 * Copyright (C) 2015 Scylla, Ltd.
 *
 * Based on ramfs code Copyright (c) 2006-2007, Kohsuke Ohtani
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef FS_NFS_NFS_HH
#define FS_NFS_NFS_HH

#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "../../external/fs/libnfs/include/nfsc/libnfs.h"

#ifdef    __cplusplus

#include <memory>
#include <string>

#include <atomic>
#include <memory>
#include <thread>
#include <unordered_map>

#include <osv/mutex.h>

class mount_context {
public:
    mount_context(const char *url);
    struct nfs_context *nfs() {
        return _nfs.get();
    };
    bool is_valid(int &err_no) {
        err_no = _errno;
        return _valid;
    };
    std::string server() {
        return std::string(_url->server);
    }
    std::string share() {
        return std::string(_url->path);
    }
    bool same_url(std::string url) {
        return url == _raw_url;
    }
private:
    bool _valid;
    int _errno;
    std::string _raw_url;
    std::unique_ptr<struct nfs_context,
                    void (*)(struct nfs_context *)> _nfs;
    std::unique_ptr<struct nfs_url, void (*)(struct nfs_url *)> _url;
};

struct mount_point *get_mount_point(struct mount *mp);

struct mount_context *get_mount_context(struct mount *mp, int &err_no);

#endif

#endif
