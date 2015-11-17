#include <osv/prex.h>
#include <osv/vnode.h>

int read_write_validate(struct vnode *vp, struct uio *uio)
{
    if (vp->v_type == VDIR) {
        return EISDIR;
    }
    if (vp->v_type != VREG) {
        return EINVAL;
    }
    if (uio->uio_offset < 0) {
        return EINVAL;
    }
    return 0;
}
