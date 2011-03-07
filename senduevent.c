/**
 *
 * Copyright (C) 2010  Kyan He <kyan.ql.he@gmail.com>
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
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

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
    int on = 0;
    int s;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    /* addr.nl_groups = 0xffffffff; */

    s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(s < 0)
        return -1;

    //setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));
    //setsockopt(s, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

    /*
    if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }
    */

    return s;
}

#define UEVENT_MSG_LEN  1024
void send_uevent(int device_fd, const char *msg)
{
        char cred_msg[CMSG_SPACE(sizeof(struct ucred))];
        struct iovec iov = {msg, sizeof(msg)};
        struct sockaddr_nl snl;
        struct msghdr hdr = {&snl, sizeof(snl), &iov, 1, /*cred_msg, sizeof(cred_msg)*/0,0, 0};


        snl.nl_family = AF_NETLINK;
        snl.nl_groups = 0;
        snl.nl_pid = getpid();
        ssize_t n = sendmsg(device_fd, &hdr, 0);
        if (n <= 0) {
            perror("sendmsg");
        }
}

int main(int argc, char *argv[])
{
    int device_fd;

    device_fd = open_uevent_socket();
    if (device_fd < 0) {
        fprintf(stderr, "open_uevent_socket");
        goto oops;
    }

    //char *msg = strdup(argv[1]);
    char *msg =argv[1];
    /*
    for (msg = strchr(msg, ' '); msg != NULL; ) {
        *msg = 0;
    }
    */
    send_uevent(device_fd, msg);

    exit(EXIT_SUCCESS);
oops:
    exit(EXIT_FAILURE);
}

