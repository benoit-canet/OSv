/*
 * Copyright (C) 2015 Scylla, Ltd.
 *
 * Based on nfs code Copyright (c) 2006-2007, Kohsuke Ohtani
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/prex.h>
#include <osv/vnode.h>
#include <osv/file.h>
#include <osv/mount.h>

#include <iostream>

#include "nfs.h"

static struct nfs_context *get_nfs_context(struct vnode *node)
{
    return get_mount_context(node->v_mount)->nfs.get();
}

static std::string get_nfs_server(struct vnode *node)
{
    return std::string(get_mount_context(node->v_mount)->url->server);
}

static std::string get_nfs_share(struct vnode *node)
{
    return std::string(get_mount_context(node->v_mount)->url->path);
}

static struct nfsfh *get_handle(struct vnode *node)
{
    return static_cast<struct nfsfh *>(node->v_data);
}

static struct nfsdir *get_dir_handle(struct vnode *node)
{
    return static_cast<struct nfsdir *>(node->v_data);
}

int nfs_open(struct file *fp)
{
    struct vnode *vp = file_dentry(fp)->d_vnode;
    std::string path(fp->f_dentry->d_path);
    auto nfs = get_nfs_context(vp);
    void *handle = nullptr;
    int flags = file_flags(fp);
    int ret = 0;

    if (path == "/") {
        return 0;
    }

    int type = vp->v_type;

    // It's a directory or a file.
    if (type == VDIR) {
        auto h = static_cast<struct nfsdir *>(handle);
        ret = nfs_opendir(nfs, path.c_str(), &h);
    } else if (type == VREG) {
        auto h = static_cast<struct nfsfh *>(handle);
        ret = nfs_open(nfs, path.c_str(), flags, &h);
    } else {
        return EIO;
    }

    if (ret) {
        return -ret;
    }

    vp->v_data = static_cast<void *>(handle);

    return 0;
}

int nfs_close(struct vnode *dvp, struct file *file)
{
    auto nfs = get_nfs_context(dvp);
    int type = dvp->v_type;
    int ret = 0;

    if (type == VDIR) {
        auto handle = get_dir_handle(dvp);
        nfs_closedir(nfs, handle);
    } else if (type == VREG) {
        auto handle = get_handle(dvp);
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

static int nfs_fsync(vnode *vp, struct file *fp)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);

    return -nfs_fsync(nfs, handle);
}

static int nfs_readdir(struct vnode *vp, struct file *fp, struct dirent *dir)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_dir_handle(vp);

    // query the NFS server about this directory entry.
    auto nfsdirent = nfs_readdir(nfs, handle);

    // We finished iterating on the directory.
    if (!nfsdirent) {
        return ENOENT;
    }

    // Fill dirent infos
    assert(sizeof(ino_t) == sizeof(nfsdirent->inode));
    dir->d_ino = nfsdirent->inode;
    // FIXME: not filling dir->d_off
    // FIXME: not filling dir->d_reclen
    dir->d_type = IFTODT(nfsdirent->mode & S_IFMT);;
    strlcpy((char *) &dir->d_name, nfsdirent->name, sizeof(dir->d_name));

    // iterate
    fp->f_offset++;

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
    if (S_ISCHR(type) || S_ISBLK(type) || S_ISFIFO(type) || S_ISSOCK(type)) {
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

static int nfs_create(struct vnode *dvp, char *name, mode_t mode)
{
    auto nfs = get_nfs_context(dvp);
    struct nfsfh *handle = nullptr;

    if (!S_ISREG(mode)) {
        return EINVAL;
    }

    int ret = nfs_creat(nfs, name, mode, &handle);

    if (ret) {
        return -ret;
    }

    dvp->v_data = static_cast<void *>(handle);

    return 0;
}

static int nfs_remove(struct vnode *dvp, struct vnode *vp, char *name)
{
    auto nfs = get_nfs_context(vp);

    return -nfs_unlink(nfs, name);
}

static int nfs_rename(struct vnode *dvp1, struct vnode *vp1, char *old_path,
                      struct vnode *dvp2, struct vnode *vp2, char *new_path)
{
    auto nfs = get_nfs_context(dvp1);

    return -nfs_rename(nfs, old_path, new_path);
}

// FIXME: Set permissions
static int nfs_mkdir(struct vnode *dvp, char *name, mode_t mode)
{
    auto nfs = get_nfs_context(dvp);

    std::cout << "nsf_mkdir " << name << std::endl;

    int ret = nfs_mkdir(nfs, name);

    std::cout << "ret = " << ret << nfs_get_error(nfs) << std::endl;

    return -ret;
}

static int nfs_rmdir(struct vnode *dvp, struct vnode *vp, char *name)
{
    auto nfs = get_nfs_context(vp);

    return -nfs_rmdir(nfs, name);
}


static struct timespec to_timespec(uint64_t sec, uint64_t nsec)
{
    struct timespec t;

    t.tv_sec = sec;
    t.tv_nsec = nsec;

    return std::move(t);
}

static int nfs_getattr(struct vnode *vp, struct vattr *attr)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);
    struct nfs_stat_64 st;

    // Get the file infos.
    int ret = nfs_fstat64(nfs, handle, &st);
    if (ret) {
        return -ret;
    }

    uint64_t type = st.nfs_mode & S_IFMT;
    uint64_t mode = st.nfs_mode & ~S_IFMT;

    // Copy the file infos.
    //attr->va_mask    =;
    attr->va_type    = IFTOVT(type);
    attr->va_mode    = IFTOVT(mode);;
    attr->va_nlink   = st.nfs_nlink;
    attr->va_uid     = st.nfs_uid;
    attr->va_gid     = st.nfs_gid;
    attr->va_fsid    = st.nfs_dev; // FIXME: not sure about this one
    attr->va_nodeid  = st.nfs_ino;
    attr->va_atime   = to_timespec(st.nfs_atime, st.nfs_atime_nsec);
    attr->va_mtime   = to_timespec(st.nfs_mtime, st.nfs_mtime_nsec);
    attr->va_ctime   = to_timespec(st.nfs_ctime, st.nfs_ctime_nsec);
    attr->va_rdev    = st.nfs_rdev;
    attr->va_nblocks = st.nfs_blocks;
    attr->va_size    = st.nfs_size;

    return 0;
}

#define nfs_setattr	((vnop_setattr_t)vop_eperm)
#define nfs_fallocate ((vnop_fallocate_t)vop_nullop)

static int nfs_truncate(struct vnode *vp, off_t length)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);

    return -nfs_ftruncate(nfs, handle, length);
}

static  int nfs_inactive(struct vnode *)
{
    return 0;
}

#define nfs_seek        ((vnop_seek_t)vop_nullop)   // no need to bound check
#define nfs_ioctl     ((vnop_ioctl_t)vop_einval)  // not a device
#define nfs_link	((vnop_link_t)vop_eperm)
#define nfs_readlink	((vnop_readlink_t)vop_nullop)
#define nfs_symlink	((vnop_symlink_t)vop_nullop)

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
