/* -*- mode: C; c-basic-offset: 4 -*- */
/* ex: set shiftwidth=4 tabstop=4 expandtab: */
/*
 * Copyright (c) 2011-2013, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Neil T. Dantam <ntd@gatech.edu>
 * Georgia Tech Humanoid Robotics Lab
 * Under Direction of Prof. Mike Stilman <mstilman@cc.gatech.edu>
 *
 *
 * This file is provided under the following "BSD-style" License:
 *
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef HAVE_CONFIG
#include "config.h"
#endif //HAVE_CONFIG

#include <ipcbench.h>
#include <ach.h>

static ach_channel_t *channel;

#define CHAN_NAME "ipcbench"

static void s_init(enum ach_map map)
{
    channel = (struct ach_channel*)calloc(ipcbench_cnt, sizeof(struct ach_channel));

    size_t i;
    enum ach_status r;
    for( i = 0; i < ipcbench_cnt; i ++ ) {
        char buf[64];
        snprintf( buf, sizeof(buf), CHAN_NAME "-%lu", i );

        r = ach_unlink(buf);               /* delete first */
        if( !( ACH_OK == r || ACH_ENOENT == r) ) {
            fprintf(stderr, "ach_unlink: %s\n", ach_result_to_string(r) );
            abort();
        }

        struct ach_create_attr attr;
        ach_create_attr_init(&attr);
        if( ACH_OK != (r = ach_create_attr_set_map(&attr,map)) ) {
            fprintf(stderr, "ach_create_attr_set_map: %s\n", ach_result_to_string(r));
            abort();
        }

        if( ACH_OK != (r = ach_create(buf, 10,
                                      sizeof(struct timespec), &attr)) ) {
            fprintf(stderr, "ach_create: %s\n", ach_result_to_string(r) );
            abort();
        }


        for(;;) {
            r = ach_open( &channel[i], buf, NULL );
            if( ACH_OK == r ) {
                break;
            } else if( ach_status_match(r, ACH_MASK_ENOENT | ACH_MASK_EACCES) ) {
                usleep(1000);     /* Race against udev */
            } else {
                fprintf(stderr, "ach_open: %s", ach_result_to_string(r) );
                abort();
            }
        }

        if( ipcbench_cnt > 1 ) {
            r = ach_channel_fd(&channel[i], &ipcbench_pfd[i].fd);
            if( ACH_OK != r ) {
                fprintf(stderr, "ach_channel_fd: %s\n", ach_result_to_string(r));
                abort();
            }

        }
    }
}


static void s_init_user()
{
    s_init(ACH_MAP_USER);
}

static void s_init_kernel()
{
    s_init(ACH_MAP_KERNEL);
}

static void s_destroy(void) {
    size_t i;
    for( i = 0; i < ipcbench_cnt; i ++ ) {
        char buf[64];
        snprintf( buf, sizeof(buf), CHAN_NAME "-%lu", i );

        enum ach_status r = ach_unlink(buf);
        if( ACH_OK != r ) {
            fprintf(stderr, "ach_unlink: %s\n", ach_result_to_string(r));
            abort();
        }
    }
}

static void s_send( const struct timespec *ts ) {
    size_t i = pubnext();
    ach_status_t r = ach_put( &channel[i], ts, sizeof(*ts) );
    if( ACH_OK != r ) {
        fprintf(stderr, "ach_put: %s\n", ach_result_to_string(r) );
        abort();
    }
}

static void s_recv( struct timespec *ts ) {
    size_t i = pollin();
    size_t fs;
    ach_status_t r = ach_get( &channel[i], ts, sizeof(*ts),
                              &fs, NULL, ACH_O_WAIT | ACH_O_LAST );
    if( ! (ACH_OK==r || ACH_MISSED_FRAME == r) ) {
        fprintf(stderr, "ach_get: %s\n", ach_result_to_string(r) );
        abort();
    }
    if( sizeof(*ts) != fs ) {
        abort();
    }
}

struct ipcbench_vtab ipc_bench_vtab_ach_user = {
    .init = s_init_user,
    .send = s_send,
    .recv = s_recv,
    .destroy = s_destroy,
};

struct ipcbench_vtab ipc_bench_vtab_ach_kernel = {
    .init = s_init_kernel,
    .send = s_send,
    .recv = s_recv,
    .destroy = s_destroy,
};
