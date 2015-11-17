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

#include "nfs.hh"

static inline struct nfs_context *get_nfs_context(struct vnode *node)
{
    return get_mount_context(node->v_mount)->nfs();
}

//static std::string get_nfs_server(struct vnode *node)
//{
//    return get_mount_context(node->v_mount)->server();
//}

//static std::string get_nfs_share(struct vnode *node)
//{
//    return get_mount_context(node->v_mount)->share();
//}

static inline struct nfsfh *get_handle(struct vnode *node)
{
    return static_cast<struct nfsfh *>(node->v_data);
}

static inline struct nfsdir *get_dir_handle(struct vnode *node)
{
    return static_cast<struct nfsdir *>(node->v_data);
}

static inline std::string mkpath(const char *name)
{
    std::string path("/");
    return path + name;
}

int nfs_open(struct file *fp)
{
    struct vnode *vp = file_dentry(fp)->d_vnode;
    std::string path(fp->f_dentry->d_path);
    auto nfs = get_nfs_context(vp);
    int flags = file_flags(fp);
    int ret = 0;

    std::cout << "nfsen " << path <<  std::endl;

    if (path == "/") {
        return 0;
    }

    int type = vp->v_type;

    // It's a directory or a file.
    if (type == VDIR) {
        std::cout << "VDIR" << std::endl;
        struct nfsdir *handle = nullptr;
        std::cout << handle << std::endl;
        ret = nfs_opendir(nfs, path.c_str(), &handle);
        std::cout << handle << std::endl;
        vp->v_data = handle;
    } else if (type == VREG) {
        std::cout << "VREG" << std::endl;
        struct nfsfh *handle = nullptr;
        std::cout << "nfs_open blib " << flags << std::endl;
        ret = nfs_open(nfs, path.c_str(), flags, &handle);
        vp->v_data = handle;
    } else {
        return EIO;
    }

    if (ret) {
        return -ret;
    }

    std::cout << "nfsen2" <<  vp->v_data << std::endl;
    return 0;
}

int nfs_close(struct vnode *dvp, struct file *file)
{
    auto nfs = get_nfs_context(dvp);
    int type = dvp->v_type;
    int ret = 0;

    std::cout << "nfs_close" << std::endl;

    if (type == VDIR) {
        auto handle = get_dir_handle(dvp);
        nfs_closedir(nfs, handle);
    } else if (type == VREG) {
        auto handle = get_handle(dvp);
        ret = nfs_close(nfs, handle);
    } else {
        return EIO;
    }

    return -ret;
}

static int nfs_read(struct vnode *vp, struct file *fp, struct uio *uio,
                    int ioflag)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);

    // From here to // END_PASTE comes from fs/ramfs/ramfs_vnops.cc
    if (vp->v_type == VDIR)
        return EISDIR;
    if (vp->v_type != VREG)
        return EINVAL;
    if (uio->uio_offset < 0)
        return EINVAL;

    if (uio->uio_resid == 0)
        return 0;

    if (uio->uio_offset >= (off_t)vp->v_size)
        return 0;

    size_t len;
    if (vp->v_size - uio->uio_offset < uio->uio_resid)
        len = vp->v_size - uio->uio_offset;
    else
        len = uio->uio_resid;
    // END_PASTE

    // FIXME: remove this temporary buffer
    auto buf = std::unique_ptr<char>(new char[len + 1]());

    printf("BUF read %s %i %i\n", buf.get(), len, uio->uio_offset);

    int ret = nfs_pread(nfs, handle, uio->uio_offset, len, buf.get());
    if (ret < 0) {
        return -ret;
    }
    printf("BUF read 2%s %i\n", buf.get(), ret);

    // Handle short read.
    return uiomove(buf.get(), ret, uio);
}

static int nfs_write(struct vnode *vp, struct uio *uio, int ioflag)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);

    std::cout << "nfs_write" << std::endl;

    // From here to // END_PASTE comes from fs/ramfs/ramfs_vnops.cc
    if (vp->v_type == VDIR)
        return EISDIR;
    if (vp->v_type != VREG)
        return EINVAL;
    if (uio->uio_offset < 0)
        return EINVAL;

    if (uio->uio_resid == 0)
        return 0;

    std::cout << "nfs_writei 2 " << uio->uio_resid << " " <<  uio->uio_offset << std::endl;
    int ret = 0;
    if (ioflag & IO_APPEND) {
        uio->uio_offset = vp->v_size;
        // NFS does not support happening to a file so let's truncate ourselve
        ret = nfs_ftruncate(nfs, handle, vp->v_size + uio->uio_resid);
    }

    if (ret) {
        return -ret;
    }

    std::cout << "nfs_writei 3 " << uio->uio_resid << " " <<  uio->uio_offset << std::endl;

    // make a copy of these since uimove will touch them.
    size_t size = uio->uio_resid;
    size_t offset = uio->uio_offset;

    // END_PASTE

    auto buf = std::unique_ptr<char>(new char[uio->uio_resid]());

    printf("BUF write%s\n", buf.get());

    ret = uiomove(buf.get(), size, uio);
    assert(!ret);

    std::cout << "nfs_write4 " << buf.get() << std::endl;
    printf("BUF write 2%s\n", buf.get());

    std::cout << "nfs_writei 5 " << uio->uio_resid << " " <<  uio->uio_offset << std::endl;

    auto buffp = buf.get();

    // Handle short writes.
    while (size > 0) {
        std::cout << "sz " << size << std::endl;
        ret = nfs_pwrite(nfs, handle, offset, size, buffp);
        if (ret < 0) {
            return -ret;
        }

        buffp += ret;
        offset += ret;
        size -= ret;
    }

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
    std::cout << "nfs_readdir" << std::endl;

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

// This function is called by the namei() family before nfsen()
static int nfs_lookup(struct vnode *dvp, char *p, struct vnode **vpp)
{
    auto nfs = get_nfs_context(dvp);
    struct nfs_stat_64 st;
    std::string path = mkpath(p);
    struct vnode *vp;

    std::cout << "nfs_lookup" << path << std::endl;

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
        std::cout << "nfs onx " << path << " " << -ret << std::endl;
        return -ret;
    }

    // Get the file type.
    uint64_t type = st.nfs_mode & S_IFMT;

    // Filter by inode type: only keep files, directories and symbolic links.
    if (S_ISCHR(type) || S_ISBLK(type) || S_ISFIFO(type) || S_ISSOCK(type)) {
        std::cout << "blaaaaaaah " << std::endl;
        // FIXME: Not sure it's the right error code.
        return EINVAL;
    }

    // Create the new vnode or get it from the cache.
    if (vget(dvp->v_mount, st.nfs_ino, &vp)) {
        // Present in the cache
        *vpp = vp;
        return 0;
    }

    if (!vp) {
        return ENOMEM;
    }

    uint64_t mode = st.nfs_mode & ~S_IFMT;

    // Fill in the new vnode informations.
    vp->v_type = IFTOVT(type);
    vp->v_mode = mode;
    vp->v_size = st.nfs_size;
    vp->v_mount = dvp->v_mount;

    *vpp = vp;

    switch(vp->v_type) {
    case VDIR:
        std::cout << "dir" << std::endl;
        break;
    case VREG:
        std::cout << "reg" << std::endl;
        break;
    case VLNK:
        std::cout << "link" << std::endl;
    }


    std::cout << "nfs_lookup 2 " << path << " " <<vp->v_type << " "<< std::endl;

    return 0;
}

static int nfs_create(struct vnode *dvp, char *name, mode_t mode)
{
    auto nfs = get_nfs_context(dvp);
    struct nfsfh *handle = nullptr;
    auto path = mkpath(name);

    std::cout << "nfs_create " << name << std::endl;

    if (!S_ISREG(mode)) {
        return EINVAL;
    }

    return -nfs_creat(nfs, path.c_str(), mode, &handle);
}

static int nfs_remove(struct vnode *dvp, struct vnode *vp, char *name)
{
    auto nfs = get_nfs_context(vp);
    auto path = mkpath(name);

    return -nfs_unlink(nfs, path.c_str());
}

static int nfs_rename(struct vnode *dvp1, struct vnode *vp1, char *old_path,
                      struct vnode *dvp2, struct vnode *vp2, char *new_path)
{
    auto nfs = get_nfs_context(dvp1);
    auto src = mkpath(old_path);
    auto dst = mkpath(new_path);

    return -nfs_rename(nfs, src.c_str(), dst.c_str());
}

// FIXME: Set permissions
static int nfs_mkdir(struct vnode *dvp, char *name, mode_t mode)
{
    auto nfs = get_nfs_context(dvp);
    auto path = mkpath(name);

    std::cout << "nfs_o_mkdir" << std::endl;
    return -nfs_mkdir(nfs, path.c_str());
}

static int nfs_rmdir(struct vnode *dvp, struct vnode *vp, char *name)
{
    auto nfs = get_nfs_context(vp);
    auto path = mkpath(name);

    std::cout << "rmdir " << path << std::endl;

    return -nfs_rmdir(nfs, path.c_str());
}


static inline struct timespec to_timespec(uint64_t sec, uint64_t nsec)
{
    struct timespec t;

    t.tv_sec = sec;
    t.tv_nsec = nsec;

    return std::move(t);
}

static const char *get_node_name(struct vnode *node)
{
    if (LIST_EMPTY(&node->v_names) == 1) {
        return nullptr;
    }

    return LIST_FIRST(&node->v_names)->d_path;
}

static int nfs_getattr(struct vnode *vp, struct vattr *attr)
{
    auto nfs = get_nfs_context(vp);
    struct nfs_stat_64 st;

    auto path = get_node_name(vp);
    if (!path) {
        return ENOENT;
    }

    // Get the file infos.
    int ret = nfs_stat64(nfs, path, &st);
    if (ret) {
        return -ret;
    }

    uint64_t type = st.nfs_mode & S_IFMT;
    uint64_t mode = st.nfs_mode & ~S_IFMT;

    // Copy the file infos.
    //attr->va_mask    =;
    attr->va_type    = IFTOVT(type);
    attr->va_mode    = mode;;
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

static int nfs_truncate(struct vnode *vp, off_t length)
{
    auto nfs = get_nfs_context(vp);
    auto handle = get_handle(vp);

    return -nfs_ftruncate(nfs, handle, length);
}

static int nfs_readlink(struct vnode *vp, struct uio *uio)
{
    auto nfs = get_nfs_context(vp);
    char buf[PATH_MAX + 1];

    auto path = get_node_name(vp);

    std::cout << "readlink" << path << std::endl;

    memset(buf, 0, sizeof(buf));
    int ret = nfs_readlink(nfs, path, buf, sizeof(buf));
    if (ret) {
        std::cout << "readlink error " << ret << std::endl;
        return -ret;
    }

    size_t sz = MIN(uio->uio_iov->iov_len, strlen(buf) + 1);

    return uiomove(buf, sz, uio);
}

static int nfs_symlink(struct vnode *dvp, char *l, char *t)
{
    auto nfs = get_nfs_context(dvp);
    auto target = mkpath(t);
    auto link = mkpath(l);

    return -nfs_symlink(nfs, target.c_str(), link.c_str());
}

static  int nfs_inactive(struct vnode *)
{
    return 0;
}

#define nfs_seek        ((vnop_seek_t)vop_nullop)       // no bound check
#define nfs_ioctl       ((vnop_ioctl_t)vop_einval)      // not a device
#define nfs_link        ((vnop_link_t)vop_eperm)        // not in NFS
#define nfs_setattr	((vnop_setattr_t)vop_eperm)
#define nfs_fallocate ((vnop_fallocate_t)vop_nullop)    // not in NFS


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
