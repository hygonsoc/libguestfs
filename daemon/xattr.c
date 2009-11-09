/* libguestfs - the guestfsd daemon
 * Copyright (C) 2009 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <stdio.h>
#include <unistd.h>

#include "../src/guestfs_protocol.h"
#include "daemon.h"
#include "actions.h"

#if defined(HAVE_ATTR_XATTR_H) || defined(HAVE_SYS_XATTR_H)

#ifdef HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#else
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
#endif

static guestfs_int_xattr_list *getxattrs (const char *path, ssize_t (*listxattr) (const char *path, char *list, size_t size), ssize_t (*getxattr) (const char *path, const char *name, void *value, size_t size));
static int _setxattr (const char *xattr, const char *val, int vallen, const char *path, int (*setxattr) (const char *path, const char *name, const void *value, size_t size, int flags));
static int _removexattr (const char *xattr, const char *path, int (*removexattr) (const char *path, const char *name));

guestfs_int_xattr_list *
do_getxattrs (const char *path)
{
#if defined(HAVE_LISTXATTR) && defined(HAVE_GETXATTR)
  return getxattrs (path, listxattr, getxattr);
#else
  reply_with_error ("getxattrs: no support for listxattr and getxattr");
  return NULL;
#endif
}

guestfs_int_xattr_list *
do_lgetxattrs (const char *path)
{
#if defined(HAVE_LLISTXATTR) && defined(HAVE_LGETXATTR)
  return getxattrs (path, llistxattr, lgetxattr);
#else
  reply_with_error ("lgetxattrs: no support for llistxattr and lgetxattr");
  return NULL;
#endif
}

int
do_setxattr (const char *xattr, const char *val, int vallen, const char *path)
{
#if defined(HAVE_SETXATTR)
  return _setxattr (xattr, val, vallen, path, setxattr);
#else
  reply_with_error ("setxattr: no support for setxattr");
  return -1;
#endif
}

int
do_lsetxattr (const char *xattr, const char *val, int vallen, const char *path)
{
#if defined(HAVE_LSETXATTR)
  return _setxattr (xattr, val, vallen, path, lsetxattr);
#else
  reply_with_error ("lsetxattr: no support for lsetxattr");
  return -1;
#endif
}

int
do_removexattr (const char *xattr, const char *path)
{
#if defined(HAVE_REMOVEXATTR)
  return _removexattr (xattr, path, removexattr);
#else
  reply_with_error ("removexattr: no support for removexattr");
  return -1;
#endif
}

int
do_lremovexattr (const char *xattr, const char *path)
{
#if defined(HAVE_LREMOVEXATTR)
  return _removexattr (xattr, path, lremovexattr);
#else
  reply_with_error ("lremovexattr: no support for lremovexattr");
  return -1;
#endif
}

static guestfs_int_xattr_list *
getxattrs (const char *path,
           ssize_t (*listxattr) (const char *path, char *list, size_t size),
           ssize_t (*getxattr) (const char *path, const char *name,
                                void *value, size_t size))
{
  ssize_t len, vlen;
  char *buf = NULL;
  int i, j;
  guestfs_int_xattr_list *r = NULL;

  CHROOT_IN;
  len = listxattr (path, NULL, 0);
  CHROOT_OUT;
  if (len == -1) {
    reply_with_perror ("listxattr");
    goto error;
  }

  buf = malloc (len);
  if (buf == NULL) {
    reply_with_perror ("malloc");
    goto error;
  }

  CHROOT_IN;
  len = listxattr (path, buf, len);
  CHROOT_OUT;
  if (len == -1) {
    reply_with_perror ("listxattr");
    goto error;
  }

  r = calloc (1, sizeof (*r));
  if (r == NULL) {
    reply_with_perror ("malloc");
    goto error;
  }

  /* What we get from the kernel is a string "foo\0bar\0baz" of length
   * len.  First count the strings.
   */
  r->guestfs_int_xattr_list_len = 0;
  for (i = 0; i < len; i += strlen (&buf[i]) + 1)
    r->guestfs_int_xattr_list_len++;

  r->guestfs_int_xattr_list_val =
    calloc (r->guestfs_int_xattr_list_len, sizeof (guestfs_int_xattr));
  if (r->guestfs_int_xattr_list_val == NULL) {
    reply_with_perror ("calloc");
    goto error;
  }

  for (i = 0, j = 0; i < len; i += strlen (&buf[i]) + 1, ++j) {
    CHROOT_IN;
    vlen = getxattr (path, &buf[i], NULL, 0);
    CHROOT_OUT;
    if (vlen == -1) {
      reply_with_perror ("getxattr");
      goto error;
    }

    r->guestfs_int_xattr_list_val[j].attrname = strdup (&buf[i]);
    r->guestfs_int_xattr_list_val[j].attrval.attrval_val = malloc (vlen);
    r->guestfs_int_xattr_list_val[j].attrval.attrval_len = vlen;

    if (r->guestfs_int_xattr_list_val[j].attrname == NULL ||
        r->guestfs_int_xattr_list_val[j].attrval.attrval_val == NULL) {
      reply_with_perror ("malloc");
      goto error;
    }

    CHROOT_IN;
    vlen = getxattr (path, &buf[i],
                     r->guestfs_int_xattr_list_val[j].attrval.attrval_val,
                     vlen);
    CHROOT_OUT;
    if (vlen == -1) {
      reply_with_perror ("getxattr");
      goto error;
    }
  }

  free (buf);

  return r;

 error:
  free (buf);
  if (r) {
    if (r->guestfs_int_xattr_list_val) {
      unsigned int k;
      for (k = 0; k < r->guestfs_int_xattr_list_len; ++k) {
        free (r->guestfs_int_xattr_list_val[k].attrname);
        free (r->guestfs_int_xattr_list_val[k].attrval.attrval_val);
      }
    }
    free (r->guestfs_int_xattr_list_val);
  }
  free (r);
  return NULL;
}

static int
_setxattr (const char *xattr, const char *val, int vallen, const char *path,
           int (*setxattr) (const char *path, const char *name,
                            const void *value, size_t size, int flags))
{
  int r;

  CHROOT_IN;
  r = setxattr (path, xattr, val, vallen, 0);
  CHROOT_OUT;
  if (r == -1) {
    reply_with_perror ("setxattr");
    return -1;
  }

  return 0;
}

static int
_removexattr (const char *xattr, const char *path,
              int (*removexattr) (const char *path, const char *name))
{
  int r;

  CHROOT_IN;
  r = removexattr (path, xattr);
  CHROOT_OUT;
  if (r == -1) {
    reply_with_perror ("removexattr");
    return -1;
  }

  return 0;
}

guestfs_int_xattr_list *
do_lxattrlist (const char *path, char *const *names)
{
#if defined(HAVE_LLISTXATTR) && defined(HAVE_LGETXATTR)
  /* XXX This would be easier if the kernel had lgetxattrat.  In the
   * meantime we use this buffer to store the whole path name.
   */
  char pathname[PATH_MAX];
  size_t path_len = strlen (path);
  guestfs_int_xattr_list *ret = NULL;
  int i, j;
  size_t k, m, nr_attrs;
  ssize_t len, vlen;
  char *buf = NULL;

  if (path_len >= PATH_MAX) {
    reply_with_perror ("lxattrlist: path longer than PATH_MAX");
    goto error;
  }

  strcpy (pathname, path);

  ret = malloc (sizeof (*ret));
  if (ret == NULL) {
    reply_with_perror ("malloc");
    goto error;
  }

  ret->guestfs_int_xattr_list_len = 0;
  ret->guestfs_int_xattr_list_val = NULL;

  for (k = 0; names[k] != NULL; ++k) {
    /* Be careful in here about which errors cause the whole call
     * to abort, and which errors allow us to continue processing
     * the call, recording a special "error attribute" in the
     * outgoing struct list.
     */
    if (path_len + strlen (names[k]) + 2 > PATH_MAX) {
      reply_with_perror ("lxattrlist: path and name longer than PATH_MAX");
      goto error;
    }
    pathname[path_len] = '/';
    strcpy (&pathname[path_len+1], names[k]);

    /* Reserve space for the special attribute. */
    void *newptr =
      realloc (ret->guestfs_int_xattr_list_val,
               (ret->guestfs_int_xattr_list_len+1)*sizeof (guestfs_int_xattr));
    if (newptr == NULL) {
      reply_with_perror ("realloc");
      goto error;
    }
    ret->guestfs_int_xattr_list_val = newptr;
    ret->guestfs_int_xattr_list_len++;

    guestfs_int_xattr *entry =
      &ret->guestfs_int_xattr_list_val[ret->guestfs_int_xattr_list_len-1];
    entry->attrname = NULL;
    entry->attrval.attrval_len = 0;
    entry->attrval.attrval_val = NULL;

    entry->attrname = strdup ("");
    if (entry->attrname == NULL) {
      reply_with_perror ("strdup");
      goto error;
    }

    CHROOT_IN;
    len = llistxattr (pathname, NULL, 0);
    CHROOT_OUT;
    if (len == -1)
      continue; /* not fatal */

    buf = malloc (len);
    if (buf == NULL) {
      reply_with_perror ("malloc");
      goto error;
    }

    CHROOT_IN;
    len = llistxattr (pathname, buf, len);
    CHROOT_OUT;
    if (len == -1)
      continue; /* not fatal */

    /* What we get from the kernel is a string "foo\0bar\0baz" of length
     * len.  First count the strings.
     */
    nr_attrs = 0;
    for (i = 0; i < len; i += strlen (&buf[i]) + 1)
      nr_attrs++;

    newptr =
      realloc (ret->guestfs_int_xattr_list_val,
               (ret->guestfs_int_xattr_list_len+nr_attrs) *
               sizeof (guestfs_int_xattr));
    if (newptr == NULL) {
      reply_with_perror ("realloc");
      goto error;
    }
    ret->guestfs_int_xattr_list_val = newptr;
    ret->guestfs_int_xattr_list_len += nr_attrs;

    /* entry[0] is the special attribute,
     * entry[1..nr_attrs] are the attributes.
     */
    entry = &ret->guestfs_int_xattr_list_val[ret->guestfs_int_xattr_list_len-nr_attrs-1];
    for (m = 1; m <= nr_attrs; ++m) {
      entry[m].attrname = NULL;
      entry[m].attrval.attrval_len = 0;
      entry[m].attrval.attrval_val = NULL;
    }

    for (i = 0, j = 0; i < len; i += strlen (&buf[i]) + 1, ++j) {
      CHROOT_IN;
      vlen = lgetxattr (pathname, &buf[i], NULL, 0);
      CHROOT_OUT;
      if (vlen == -1) {
        reply_with_perror ("getxattr");
        goto error;
      }

      entry[j+1].attrname = strdup (&buf[i]);
      entry[j+1].attrval.attrval_val = malloc (vlen);
      entry[j+1].attrval.attrval_len = vlen;

      if (entry[j+1].attrname == NULL ||
          entry[j+1].attrval.attrval_val == NULL) {
        reply_with_perror ("malloc");
        goto error;
      }

      CHROOT_IN;
      vlen = lgetxattr (pathname, &buf[i],
                        entry[j+1].attrval.attrval_val, vlen);
      CHROOT_OUT;
      if (vlen == -1) {
        reply_with_perror ("getxattr");
        goto error;
      }
    }

    free (buf); buf = NULL;

    char num[16];
    snprintf (num, sizeof num, "%zu", nr_attrs);
    entry[0].attrval.attrval_len = strlen (num) + 1;
    entry[0].attrval.attrval_val = strdup (num);

    if (entry[0].attrval.attrval_val == NULL) {
      reply_with_perror ("strdup");
      goto error;
    }
  }

  /* If verbose, debug what we're about to send back. */
  if (verbose) {
    fprintf (stderr, "lxattrlist: returning: [\n");
    for (k = 0; k < ret->guestfs_int_xattr_list_len; ++k) {
      const guestfs_int_xattr *entry = &ret->guestfs_int_xattr_list_val[k];
      if (STRNEQ (entry[0].attrname, "")) {
        fprintf (stderr, "ERROR: expecting empty attrname at k = %zu\n", k);
        break;
      }
      fprintf (stderr, "  %zu: special attrval = %s\n",
               k, entry[0].attrval.attrval_val);
      for (i = 1; k+i < ret->guestfs_int_xattr_list_len; ++i) {
        if (STREQ (entry[i].attrname, ""))
          break;
        fprintf (stderr, "    name %s, value length %d\n",
                 entry[i].attrname, entry[i].attrval.attrval_len);
      }
      k += i-1;
    }
    fprintf (stderr, "]\n");
  }

  return ret;

 error:
  free (buf);
  if (ret) {
    if (ret->guestfs_int_xattr_list_val) {
      for (k = 0; k < ret->guestfs_int_xattr_list_len; ++k) {
        free (ret->guestfs_int_xattr_list_val[k].attrname);
        free (ret->guestfs_int_xattr_list_val[k].attrval.attrval_val);
      }
      free (ret->guestfs_int_xattr_list_val);
    }
    free (ret);
  }
  return NULL;
#else
  reply_with_error ("lxattrlist: no support for llistxattr and lgetxattr");
  return NULL;
#endif
}

#else /* no xattr.h */

guestfs_int_xattr_list *
do_getxattrs (const char *path)
{
  reply_with_error ("getxattrs: no support for xattrs");
  return NULL;
}

guestfs_int_xattr_list *
do_lgetxattrs (const char *path)
{
  reply_with_error ("lgetxattrs: no support for xattrs");
  return NULL;
}

int
do_setxattr (const char *xattr, const char *val, int vallen, const char *path)
{
  reply_with_error ("setxattr: no support for xattrs");
  return -1;
}

int
do_lsetxattr (const char *xattr, const char *val, int vallen, const char *path)
{
  reply_with_error ("lsetxattr: no support for xattrs");
  return -1;
}

int
do_removexattr (const char *xattr, const char *path)
{
  reply_with_error ("removexattr: no support for xattrs");
  return -1;
}

int
do_lremovexattr (const char *xattr, const char *path)
{
  reply_with_error ("lremovexattr: no support for xattrs");
  return -1;
}

guestfs_int_xattr_list *
do_lxattrlist (const char *path, char *const *names)
{
  reply_with_error ("lxattrlist: no support for xattrs");
  return NULL;
}

#endif /* no xattr.h */
