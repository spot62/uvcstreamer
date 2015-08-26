/*******************************************************************************
# Linux-UVC streaming input-plugin for MJPG-streamer                           #
#                                                                              #
# This package work with the Logitech UVC based webcams with the mjpeg feature #
#                                                                              #
# Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard                   #
#                    2007 Lucas van Staden                                     #
#                    2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <syslog.h>

#include "utils.h"
#include "v4l2uvc.h" // this header will includes the ../../mjpg_streamer.h
#include "huffman.h"
#include "jpeg_utils.h"
#include "dynctrl.h"

#include "input_uvc.h"

#define INPUT_PLUGIN_NAME "UVC webcam grabber"

struct input_uvc_config input_uvc_cfg = {
    .dev = "/dev/video0",
    .width = 640, //1280, //320,
    .height = 480, //960, //240,
    .fps = 25,//1,
    .format = V4L2_PIX_FMT_MJPEG,
    .dynctrls = true,
    .gquality = 80,
    .minimum_size = 0,
    .stop_camera = 0
};

/* private functions and variables to this plugin */
extern struct _globals global;
static globals *pglobal=&global;

void *cam_thread(void *);
void cam_cleanup(void *);
int input_cmd(int plugin, unsigned int control, unsigned int group, int value);
context cam;

/******************************************************************************
Description.: init function
Input Value.: -
Return Value: 0 - succes, else - error
******************************************************************************/
int input_uvc_init(void)
{
    /* initialize the mutes variable */
    if(pthread_mutex_init(&cam.controls_mutex, NULL) != 0) {
        IPRINT("could not initialize mutex variable\n");
        exit(EXIT_FAILURE);
    }

    //getopt_proc(argc, argv);

    cam.id = pglobal->incnt;
    cam.pglobal = pglobal;

    pglobal->in[pglobal->incnt].plugin=INPUT_PLUGIN_NAME;
    pglobal->in[pglobal->incnt].cmd=input_uvc_cmd;
    pglobal->incnt++;

    /* allocate webcam datastructure */
    cam.videoIn = malloc(sizeof(struct vdIn));
    if(cam.videoIn == NULL) {
        IPRINT("not enough memory for videoIn\n");
        exit(EXIT_FAILURE);
    }
    memset(cam.videoIn, 0, sizeof(struct vdIn));

    /* display the parsed values */
    IPRINT("Using V4L2 device.: %s\n", input_uvc_cfg.dev);
    IPRINT("Desired Resolution: %lu x %lu\n", input_uvc_cfg.width, input_uvc_cfg.height);
    IPRINT("Frames Per Second.: %lu\n", input_uvc_cfg.fps);
    IPRINT("Format............: %s\n", (input_uvc_cfg.format == V4L2_PIX_FMT_YUYV) ? "YUV" : "MJPEG");
    if(input_uvc_cfg.format == V4L2_PIX_FMT_YUYV)
        IPRINT("JPEG Quality......: %lu\n", input_uvc_cfg.gquality);
    IPRINT("Stop camera feat..: %s\n", (!input_uvc_cfg.stop_camera) ? "disabled" : "enabled");
    IPRINT("Dynctrls feat.....: %s\n", (!input_uvc_cfg.dynctrls) ? "disabled" : "enabled");

    DBG("vdIn pn: %d\n", cam.id);
    /* open video device and prepare data structure */
    if(init_videoIn(cam.videoIn, input_uvc_cfg.dev,
                    input_uvc_cfg.width, input_uvc_cfg.height, input_uvc_cfg.fps,
                    input_uvc_cfg.format, 1, cam.pglobal, cam.id) < 0)
    {
        IPRINT("init_VideoIn failed\n");
        closelog();
        exit(EXIT_FAILURE);
    }

    /*
     * recent linux-uvc driver (revision > ~#125) requires to use dynctrls
     * for pan/tilt/focus/...
     * dynctrls must get initialized
     */
    if(input_uvc_cfg.dynctrls)
    {
        initDynCtrls(cam.videoIn->fd);
        // enumerate V4L2 controls after UVC extended mapping
        enumerateControls(cam.videoIn, cam.pglobal, cam.id);
    }

    return 0;
}

/******************************************************************************
Description.: spins of a worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_uvc_run(void)
{
    cam.pglobal->in[0].buf = malloc(cam.videoIn->framesizeIn);
    if(cam.pglobal->in[0].buf == NULL) {
        fprintf(stderr, "could not allocate memory\n");
        exit(EXIT_FAILURE);
    }

    DBG("launching camera thread\n");
    /* create thread and pass context to thread function */
    pthread_create(&(cam.threadID), NULL, cam_thread, &(cam));
    pthread_detach(cam.threadID);
    return 0;
}

/******************************************************************************
Description.: Stops the execution of worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_uvc_stop(void)
{
    DBG("will cancel camera thread\n");
    pthread_cancel(cam.threadID);
    return 0;
}

/******************************************************************************
Description.: this thread worker grabs a frame and copies it to the global buffer
Input Value.: unused
Return Value: unused, always NULL
******************************************************************************/
void *cam_thread(void *arg)
{

    context *pcontext = arg;
    pglobal = pcontext->pglobal;

    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(cam_cleanup, pcontext);

    while(!pglobal->stop) {
        while(pcontext->videoIn->streamingState == STREAMING_PAUSED) {
            usleep(1); // maybe not the best way so FIXME
        }

        if(input_uvc_cfg.stop_camera == 1)
        {
			/* check active outputs */
			pthread_mutex_lock(&pglobal->in[pcontext->id].out);
			if(pglobal->in[pcontext->id].num_outs == 0)
			{
				/* stop camera */
				uvcStopGrab(pcontext->videoIn);
				/* wait for active outputs */
				pthread_cond_wait(&pglobal->in[pcontext->id].out_update, &pglobal->in[pcontext->id].out);
			}
			/* allow others to access the global buffer again */
			pthread_mutex_unlock(&pglobal->in[pcontext->id].out);
		}

        /* grab a frame */
        if(uvcGrab(pcontext->videoIn) < 0) {
            IPRINT("Error grabbing frames\n");
            exit(EXIT_FAILURE);
        }

        DBG("received frame of size: %lu from plugin: %d\n", pcontext->videoIn->framebuffer_sz, pcontext->id);

        /*
         * Workaround for broken, corrupted frames:
         * Under low light conditions corrupted frames may get captured.
         * The good thing is such frames are quite small compared to the regular pictures.
         * For example a VGA (640x480) webcam picture is normally >= 8kByte large,
         * corrupted frames are smaller.
         */
        if(pcontext->videoIn->framebuffer_sz < input_uvc_cfg.minimum_size) {
            DBG("dropping too small frame, assuming it as broken\n");
            continue;
        }

        /* copy JPG picture to global buffer */
        pthread_mutex_lock(&pglobal->in[pcontext->id].db);

        /*
         * If capturing in YUV mode convert to JPEG now.
         * This compression requires many CPU cycles, so try to avoid YUV format.
         * Getting JPEGs straight from the webcam, is one of the major advantages of
         * Linux-UVC compatible devices.
         */
        if(pcontext->videoIn->formatIn == V4L2_PIX_FMT_YUYV) {
            DBG("compressing frame from input: %d\n", (int)pcontext->id);
            pglobal->in[pcontext->id].size = compress_yuyv_to_jpeg(pcontext->videoIn, pglobal->in[pcontext->id].buf, pcontext->videoIn->framebuffer_sz, input_uvc_cfg.gquality);
        } else {
            DBG("copying frame from input: %d\n", (int)pcontext->id);
            pglobal->in[pcontext->id].size = memcpy_picture(pglobal->in[pcontext->id].buf, pcontext->videoIn->framebuffer, pcontext->videoIn->framebuffer_sz);
        }

#if 0
        /* motion detection can be done just by comparing the picture size, but it is not very accurate!! */
        if((prev_size - global->size)*(prev_size - global->size) > 4 * 1024 * 1024) {
            DBG("motion detected (delta: %d kB)\n", (prev_size - global->size) / 1024);
        }
        prev_size = global->size;
#endif

        /* copy this frame's timestamp to user space */
        pglobal->in[pcontext->id].timestamp = pcontext->videoIn->timestamp;

        /* signal fresh_frame */
        pthread_cond_broadcast(&pglobal->in[pcontext->id].db_update);
        pthread_mutex_unlock(&pglobal->in[pcontext->id].db);


        /* only use usleep if the fps is below 5, otherwise the overhead is too long */
        if(pcontext->videoIn->fps < 5) {
            DBG("waiting for next frame for %d us\n", 1000 * 1000 / pcontext->videoIn->fps);
            usleep(1000 * 1000 / pcontext->videoIn->fps);
        } else {
            DBG("waiting for next frame\n");
        }
    }

    DBG("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);

    return NULL;
}

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
void cam_cleanup(void *arg)
{
    static unsigned char first_run = 1;
    context *pcontext = arg;
    pglobal = pcontext->pglobal;
    if(!first_run) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;
    IPRINT("cleaning up ressources allocated by input thread\n");

    close_v4l2(pcontext->videoIn);
    if(pcontext->videoIn != NULL) free(pcontext->videoIn);
    if(pglobal->in[pcontext->id].buf != NULL)
        free(pglobal->in[pcontext->id].buf);
}

/******************************************************************************
Description.: process commands, allows to set v4l2 controls
Input Value.: * control specifies the selected v4l2 control's id
                see struct v4l2_queryctr in the videodev2.h
              * value is used for control that make use of a parameter.
Return Value: depends in the command, for most cases 0 means no errors and
              -1 signals an error. This is just rule of thumb, not more!
******************************************************************************/
int input_uvc_cmd(int plugin_number, unsigned int control_id, unsigned int group, int value)
{
    int ret = -1;
    int i = 0;
    DBG("Requested cmd (id: %d) for the %d plugin. Group: %d value: %d\n", control_id, plugin_number, group, value);
    switch(group) {
    case IN_CMD_GENERIC: {
            int i;
            for (i = 0; i<pglobal->in[plugin_number].parametercount; i++) {
                if ((pglobal->in[plugin_number].in_parameters[i].ctrl.id == control_id) &&
                    (pglobal->in[plugin_number].in_parameters[i].group == IN_CMD_GENERIC)){
                    DBG("Generic control found (id: %d): %s\n", control_id, pglobal->in[plugin_number].in_parameters[i].ctrl.name);
                    DBG("New %s value: %d\n", pglobal->in[plugin_number].in_parameters[i].ctrl.name, value);
                    return 0;
                }
            }
            DBG("Requested generic control (%d) did not found\n", control_id);
            return -1;
        } break;
    case IN_CMD_V4L2: {
            ret = v4l2SetControl(cam.videoIn, control_id, value, plugin_number, pglobal);
            if(ret == 0) {
                pglobal->in[plugin_number].in_parameters[i].value = value;
            } else {
                DBG("v4l2SetControl failed: %d\n", ret);
            }
            return ret;
        } break;
    case IN_CMD_RESOLUTION: {
        // the value points to the current formats nth resolution
        if(value > (pglobal->in[plugin_number].in_formats[pglobal->in[plugin_number].currentFormat].resolutionCount - 1)) {
            DBG("The value is out of range");
            return -1;
        }
        int height = pglobal->in[plugin_number].in_formats[pglobal->in[plugin_number].currentFormat].supportedResolutions[value].height;
        int width = pglobal->in[plugin_number].in_formats[pglobal->in[plugin_number].currentFormat].supportedResolutions[value].width;
        ret = setResolution(cam.videoIn, width, height);
        if(ret == 0) {
            pglobal->in[plugin_number].in_formats[pglobal->in[plugin_number].currentFormat].currentResolution = value;
        }
        return ret;
    } break;
    case IN_CMD_JPEG_QUALITY:
        if((value >= 0) && (value < 101)) {
            pglobal->in[plugin_number].jpegcomp.quality = value;
            if(IOCTL_VIDEO(cam.videoIn->fd, VIDIOC_S_JPEGCOMP, &pglobal->in[plugin_number].jpegcomp) != EINVAL) {
                DBG("JPEG quality is set to %d\n", value);
                ret = 0;
            } else {
                DBG("Setting the JPEG quality is not supported\n");
            }
        } else {
            DBG("Quality is out of range\n");
        }
        break;
    }
    return ret;
}

