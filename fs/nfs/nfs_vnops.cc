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

#include "../../external/fs/libnfs/include/nfsc/libnfs.h"

static int nfs_lookup(struct vnode *dvp, char *name, struct vnode **vpp)
{
    return 0;
}

static int  nfs_mkdir(struct vnode *dvp, char *name, mode_t mode)
{
    return 0;
}

static int nfs_rmdir(struct vnode *dvp, struct vnode *vp, char *name)
{
    return 0;
}

static int nfs_remove(struct vnode *dvp, struct vnode *vp, char *name)
{
    return 0;
}

static int nfs_truncate(struct vnode *vp, off_t length)
{
    return 0;
}

static int nfs_create(struct vnode *dvp, char *name, mode_t mode)
{
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

static int  nfs_rename(struct vnode *dvp1, struct vnode *vp1, char *name1,
                         struct vnode *dvp2, struct vnode *vp2, char *name2)
{
    return 0;
}

static int nfs_readdir(struct vnode *vp, struct file *fp, struct dirent *dir)
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

#define nfs_open	((vnop_open_t)vop_nullop)
#define nfs_close	((vnop_close_t)vop_nullop)
#define nfs_seek	((vnop_seek_t)vop_nullop)
#define nfs_ioctl	((vnop_ioctl_t)vop_einval)
#define nfs_fsync	((vnop_fsync_t)vop_nullop)
#define nfs_setattr	((vnop_setattr_t)vop_eperm)
#define nfs_link	((vnop_link_t)vop_eperm)
#define nfs_fallocate ((vnop_fallocate_t)vop_nullop)
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
