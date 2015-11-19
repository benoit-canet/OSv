/*
 * Copyright (c) 2006-2007, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * rmafs_vnops.c - vnode operations for RAM file system.
 */

#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <osv/prex.h>
#include <osv/vnode.h>
#include <osv/file.h>
#include <osv/mount.h>

#include <iostream>

#include "../../external/fs/libnfs/include/nfsc/libnfs.h"

static struct nfs_context *get_nfs_context(struct vnode *node)
{
    return static_cast<struct nfs_context *>(node->v_mount->m_root->d_vnode->v_data);
}

static struct nfsfh *get_handle(struct vnode *node)
{
    return static_cast<struct nfsfh *>(node->v_data);
}

int nfs_open(struct file *fp)
{
    struct vnode *vp = file_dentry(fp)->d_vnode;
    std::string path(fp->f_dentry->d_path);
    auto nfs = get_nfs_context(vp);
    struct void *handle = nullptr;
    int flags = file_flags(fp);
    int ret = 0;

    if (path == "/") {
        return 0;
    }

    int type = vp->v_type;

    // It's a directory or a file.
    if (type == VDIR) {
        auto h = static_cast<struct nfsdir *handle>(handle);
        ret = nfs_opendir(nfs, path, &h);
    } else if (type == VREG) {
        auto h = static_cast<struct nfsfh *>(handle);
        ret = nfs_open(nfs, path, flags, &h);
    } else {
        return EIO;
    }

    if (ret) {
        return -ret;
    }

    vp->v_data = handle;

    return 0;
}

int nfs_close(struct vnode *dvp, struct file *file)
{
    auto nfs = get_nfs_context(dvp);
    auto handle = get_handle(dvp);
    int ret = 0;

    int type = vp->v_type;

    if (type == VDIR) {
        ret = nfs_closedir(nfs, handle);
    } else if (type == VREG) {
        ret = nfs_close(nfs, handle);
    } else {
        return EIO;
    }

    if (ret) {
        return -ret;
    }

    dvp->v_data = nullptr;

    return 0;
}

static int nfs_read(struct vnode *vp, struct file *fp, struct uio *uio, int ioflag)
{
    return 0;
}

static int nfs_write(struct vnode *vp, struct uio *uio, int ioflag)
{
    return 0;
}

static int zfs_fsync(vnode_t *vp, struct file *fp)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);

    return -nfs_fsync(nfs, handle);
}

static int nfs_readdir(struct vnode *vp, struct file *fp, struct dirent *dir)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);
    struct nfsdirent *nfsdirent;
    int ret;

    while((nfsdirent = nfs_readdir(nfs, nfsdir)) != NULL) {
    char path[1024];

    if (!strcmp(nfsdirent->name, ".") || !strcmp(nfsdirent->name, "..")) {
    continue;
    }
    snprintf(path, 1024, "%s/%s", dir, nfsdirent->name);

    switch (nfsdirent->mode & S_IFMT) {
    case S_IFLNK:
    printf("l");
    break;
    case S_IFREG:
    printf("-");
    break;
    case S_IFDIR:
    printf("d");
    break;
    case S_IFCHR:
    printf("c");
    break;
    case S_IFBLK:
    printf("b");
    break;
    }
    printf("%c%c%c",
    "-r"[!!(nfsdirent->mode & S_IRUSR)],
    "-w"[!!(nfsdirent->mode & S_IWUSR)],
    "-x"[!!(nfsdirent->mode & S_IXUSR)]
    );
    printf("%c%c%c",
    "-r"[!!(nfsdirent->mode & S_IRGRP)],
    "-w"[!!(nfsdirent->mode & S_IWGRP)],
    "-x"[!!(nfsdirent->mode & S_IXGRP)]
    );
    printf("%c%c%c",
    "-r"[!!(nfsdirent->mode & S_IROTH)],
    "-w"[!!(nfsdirent->mode & S_IWOTH)],
    "-x"[!!(nfsdirent->mode & S_IXOTH)]
    );
    printf(" %2d", (int)nfsdirent->nlink);
    printf(" %5d", (int)nfsdirent->uid);
    printf(" %5d", (int)nfsdirent->gid);
    printf(" %12" PRId64, nfsdirent->size);

    printf(" %s\n", path + 1);

    if (recursive && (nfsdirent->mode & S_IFMT) == S_IFDIR) {
    process_dir(nfs, path, level - 1);
    }
    }
    nfs_closedir(nfs, nfsdir);
}
    return 0;
}

// This function is called by the namei() family before nfs_open()
static int nfs_lookup(struct vnode *dvp, char *p, struct vnode **vpp)
{
    auto nfs = get_nfs_context(dvp);
    struct nfs_stat_64 st;
    std::string path(p);
    struct vnode *vp;

    // Make sure we don't accidentally return garbage.
    *vpp = nullptr;

    // Following 4 checks inspired by ZFS code
    if (!path.size())
        return ENOENT;

    if (dvp->v_type != VDIR)
        return ENOTDIR;

    assert(path != ".");
    assert(path != "..");

    // We must get the inode number so we query the NFS server.
    int ret = nfs_stat64(nfs, path.c_str(), &st);
    if (ret) {
        return -ret;
    }

    // Get the file type.
    uint64_t type = st.nfs_mode & S_IFMT;

    // Filter by inode type: only keep files, directories and symbolic links.
    if (S_ISCHR(type) || S_ISBLK(type) || S_ISFIFO(type) || S_ISSOCK(type)) {
        // FIXME: Not sure it's the right error code.
        return EINVAL;
    }

    // Create the new vnode or get it from the cache.
    if (vget(dvp->v_mount, st.nfs_ino, &vp)) {
        // Present in the cache
        *vpp = vp;
        return 0;
    }

    uint64_t mode = st.nfs_mode & ~S_IFMT;

    // Fill in the new vnode informations.
    vp->v_type = IFTOVT(type);
    vp->v_mode = IFTOVT(mode);
    vp->v_size = st.nfs_size;
    vp->v_data = nullptr;

    *vpp = vp;

    return 0;
}

// FIXME: Set permissions
static int  nfs_mkdir(struct vnode *dvp, char *name, mode_t mode)
{
    auto nfs = get_nfs_context(dvp);

    return -nfs_mkdir(nfs, name);
}

static int nfs_rmdir(struct vnode *dvp, struct vnode *vp, char *name)
{
    auto nfs = get_nfs_context(vp);

    return -nfs_rmdir(nfs, name);
}

static int nfs_remove(struct vnode *dvp, struct vnode *vp, char *name)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);

    return -nfs_unlink(nfs, handle, vp->v_path);
}

static int nfs_truncate(struct vnode *vp, off_t length)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);

    return -nfs_ftruncate(nfs, handle, length);
}

static int nfs_create(struct vnode *dvp, char *name, mode_t mode)
{
    auto nfs = get_nfs_context(dvp);
    struct nfsdir *nfsdir = nullptr;
    struct nfsfh *handle = nullptr;

    ret = nfs_opendir(nfs, dir, &nfsdir);
    if (ret) {
        return -ret;
    }
    int ret = nfs_creat(nfs, name, mode, &handle);

    if (ret) {
        return -ret;
    }

    dvp->v_data = handle;
    return 0;
}
static int  nfs_rename(struct vnode *dvp1, struct vnode *vp1, char *name1,
                         struct vnode *dvp2, struct vnode *vp2, char *name2)
{
    return 0;
}

static int nfs_getattr(struct vnode *vnode, struct vattr *attr)
{
    return 0;
}

static  int nfs_inactive(struct vnode *)
{
    return 0;
}

#define nfs_seek        ((vnop_seek_t)vop_nullop)
#define ramfs_ioctl     ((vnop_ioctl_t)vop_einval)
/*
 * vnode operations
 */
struct vnops nfs_vnops = {
    nfs_open,             /* open */
    nfs_close,            /* close */
    nfs_read,             /* read */
    nfs_write,            /* write */
    nfs_seek,             /* seek */
    nfs_ioctl,            /* ioctl */
    nfs_fsync,            /* fsync */
    nfs_readdir,          /* readdir */
    nfs_lookup,           /* lookup */
    nfs_create,           /* create */
    nfs_remove,           /* remove */
    nfs_rename,           /* remame */
    nfs_mkdir,            /* mkdir */
    nfs_rmdir,            /* rmdir */
    nfs_getattr,          /* getattr */
    nfs_setattr,          /* setattr */
    nfs_inactive,         /* inactive */
    nfs_truncate,         /* truncate */
    nfs_link,             /* link */
    (vnop_cache_t) nullptr, /* arc */
    nfs_fallocate,        /* fallocate */
    nfs_readlink,         /* read link */
    nfs_symlink,          /* symbolic link */
};
