/**
 *
 * Copyright (C) 2011  Kyan He <kyan.ql.he@gmail.com>
 *
 * Last modified:
 *      full function completed, 
 *      Kyan He <kyan.ql.he@gmail.com> @ Sat Feb 26 01:32:11 CST 2011
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2.0 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/netlink.h>

#ifndef ANDROID

/* because android use bionic, i don't know the following things in GNU libc */
#define SCM_CREDENTIALS     0x02

struct ucred {
    __u32 pid;
    __u32 uid;
    __u32 gid;
};

#endif /* ANDROID */

/* verbose output switch */
static int verbose = 0;

static void
I(const char *msg, ...)
{
    if (!verbose) return;

    va_list ap;

    va_start (ap, msg);
    vfprintf(stderr, msg, ap);
    va_end (ap);
}

static void
E(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n    errno: %s\n", strerror(errno));
    va_end(ap);
}

#ifdef VERSION_2
/* maybe use this in future */
struct uevent {
    const char *action;
    const char *path;
    const char *subsystem;
    const char *firmware;
    const char *partition_name;
    int partition_num;
    int major;
    int minor;
};
#endif

static int open_uevent_socket(void)
{
    struct sockaddr_nl addr;
    int sz = 128*1024;
    int on = 1;
    int s;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(s < 0)
        return -1;

    setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));
    setsockopt(s, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

    /* bind is a privalege syscall */
    if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }

    return s;
}

static void dump_uevent(char *msg, size_t sz)
{
    //TODO: change arg1 to const char *
    const char *s;
#define UDEV_OLY
#ifdef UDEV_ONLY
    msg = strchr(msg, 0) + 1;
#endif

    s = msg;

    while(*msg) {
        /* advance to after the next \0 */
        msg += strlen(msg);
        *msg++ = '\n';
    }
    printf("%s\n", s);
}

#define UEVENT_MSG_LEN  1024
void handle_io(int device_fd)
{
    for(;;) {
        char msg[UEVENT_MSG_LEN+2];
        char cred_msg[CMSG_SPACE(sizeof(struct ucred))];
        struct iovec iov = {msg, sizeof(msg)};
        struct sockaddr_nl snl;
        struct msghdr hdr = {&snl, sizeof(snl), &iov, 1, cred_msg, sizeof(cred_msg), 0};

        ssize_t n = recvmsg(device_fd, &hdr, 0);
        if (n <= 0) {
            E("recvmsg");
            continue;
        }

        I("sock, groups: %d pid:%hd\n",
                    snl.nl_groups, snl.nl_pid);
        if ((snl.nl_groups != 1) || (snl.nl_pid != 0)) {
            /* ignoring non-kernel netlink multicast message */
            if (!verbose) continue;
        }

        struct cmsghdr * cmsg = CMSG_FIRSTHDR(&hdr);
        I("MSG_TYPE %d\n",cmsg->cmsg_type);
        if (cmsg == NULL || cmsg->cmsg_type != SCM_CREDENTIALS) {
            /* no sender credentials received, ignore message */
            if (!verbose) continue;
        }

        struct ucred * cred = (struct ucred *)CMSG_DATA(cmsg);
        I("=> send from pid:%d, uid:%d, gid:%d\n",
                    cred->pid, cred->uid, cred->gid);
        if (cred->uid != 0) {
            /* message from non-root user, ignore */
            if (!verbose) continue;
        }

        if(n >= UEVENT_MSG_LEN)   /* overflow -- discard */
            continue;

        msg[n] = '\0';
        msg[n+1] = '\0'; /* for what */

        dump_uevent(msg, n);
    }
}

int main(int argc, char **argv)
{
    int device_fd;

    if (argc > 1) {
        if(!strcmp(argv[1], "-v")
                || !strcmp(argv[1], "--verbose")) {
            verbose = 1;
        } else {
            printf("Usage: dumpuevent [-v]\n"
                   "    -v for verbose output\n");
        }
    }

    device_fd = open_uevent_socket();
    if (device_fd < 0) {
        E("open_uevent_socket");
        goto oops;
    }

    fcntl(device_fd, F_SETFD, FD_CLOEXEC);

    /* main loop */
    handle_io(device_fd);

    exit(EXIT_SUCCESS);
oops:
    exit(EXIT_FAILURE);
}

