#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "uvcstreamer.h"

#include "input_uvc.h"
#include "httpd.h"

#include "utils.h"

struct _globals global={ .stop=0, .incnt=0, .outcnt=0 };

static int run=1;

void sighandler(int signum, siginfo_t *info, void *ptr)
{
    DBG("Received signal %d\n", signum);
    DBG("Signal originates from process %lu\n",
        (unsigned long)info->si_pid);

    switch(signum)
    {
    case SIGTERM:
    	DBG("exit by SIGTERM\n");
    	run=0;
    	break;
    default:
    	break;
    }

    run=0;
}

int sigaction_init(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));

    act.sa_sigaction = sighandler;
    act.sa_flags = SA_SIGINFO;

    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
	(void)signal(SIGPIPE, SIG_IGN);

    return 0;
}

static void help(void)
{
    int i;

    fprintf(stderr, " ---------------------------------------------------------------\n" \
    " [-h | --help ].........: this help message\n" \
    " [-d | --device ].......: video device to open (your camera)\n" \
    " [-r | --resolution ]...: the resolution of the video device,\n" \
    "                          can be one of the following strings:\n" \
    "                          ");

    for(i = 0; i < LENGTH_OF(resolutions); i++) {
        fprintf(stderr, "%s ", resolutions[i].string);
        if((i + 1) % 6 == 0)
            fprintf(stderr, "\n                          ");
    }
    fprintf(stderr, "\n                          or a custom value like the following" \
    "\n                          example: 640x480\n");

    fprintf(stderr, " [-f | --fps ]..........: frames per second\n" \
    " [-y | --yuv ]..........: enable YUYV format and disable MJPEG mode\n" \
    " [-q | --quality ]......: JPEG compression quality in percent \n" \
    "                          (activates YUYV format, disables MJPEG)\n" \
    " [-m | --minimum_size ].: drop frames smaller then this limit, useful\n" \
    "                          if the webcam produces small-sized garbage frames\n" \
    "                          may happen under low light conditions\n" \
    " [-n | --no_dynctrl ]...: do not initalize dynctrls of Linux-UVC driver\n" \
    " [-l | --led ]..........: switch the LED \"on\", \"off\", let it \"blink\" or leave\n" \
    "                          it up to the driver using the value \"auto\"\n" \
	" [-s | --stop ].........: stop camera when no active outputs\n" \
    " [-p | --port ].........: TCP port for this HTTP server\n" \
    " [-a | --auth ].........: ask for \"username:password\" on connect\n" \
    " [-w | --www ]..........: folder that contains webpages in \n" \
    "                           flat hierarchy (no subfolders)\n" \
    " [-c | --nocommands ]...: disable execution of commands\n"
    " ---------------------------------------------------------------\n\n");
}

static const char short_options[] = "hd:r:f:yq:m:nl:sp:a:w:c";

static const struct option long_options[] = {
    { "help",           no_argument,        NULL,   'h' },
    { "device",         required_argument,  NULL,   'd' },
    { "resolution",     required_argument,  NULL,   'r' },
    { "fps",            required_argument,  NULL,   'f' },
    { "yuv",            no_argument,        NULL,   'y' },
    { "quality",        required_argument,  NULL,   'q' },
    { "minimum_size",   required_argument,  NULL,   'm' },
    { "no_dynctrl",     no_argument,        NULL,   'n' },
    { "led",            required_argument,  NULL,   'l' },
    { "stop",           no_argument,        NULL,   's' },
    { "port",           required_argument,  NULL,   'p' },
    { "auth",           required_argument,  NULL,   'a' },
    { "www",            required_argument,  NULL,   'w' },
    { "nocommands",     no_argument,        NULL,   'c' },
    { 0, 0, 0, 0}
};

static int getopt_proc(int argc, char **argv)
{
	int i;
	char* s;

	/* show all parameters for DBG purposes */
    for(i = 0; i < argc; i++) {
        DBG("argv[%d]=%s\n", i, argv[i]);
    }

	reset_getopt();

	for (;;)
	{
		int index, c = 0;

		c = getopt_long(argc, argv, short_options, long_options, &index);

		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
            help();
            return -1;
            break;

        /* d, device */
        case 'd':
            DBG("case: d, device\n");
            input_uvc_cfg.dev = strdup(optarg);
            break;

        /* r, resolution */
        case 'r':
            DBG("case: r, resolution\n");
            input_uvc_cfg.width = -1;
            input_uvc_cfg.height = -1;

            /* try to find the resolution in lookup table "resolutions" */
            for(i = 0; i < LENGTH_OF(resolutions); i++) {
                if(strcmp(resolutions[i].string, optarg) == 0) {
                    input_uvc_cfg.width  = resolutions[i].width;
                    input_uvc_cfg.height = resolutions[i].height;
                }
            }
            /* done if width and height were set */
            if(input_uvc_cfg.width != -1 && input_uvc_cfg.height != -1)
                break;
            /* parse value as decimal value */
            input_uvc_cfg.width  = strtol(optarg, &s, 10);
            input_uvc_cfg.height = strtol(s + 1, NULL, 10);
            break;

        /* f, fps */
        case 'f':
            DBG("case: f, fps\n");
            input_uvc_cfg.fps = atoi(optarg);
            break;

        /* y, yuv */
        case 'y':
            DBG("case: y, yuv\n");
            input_uvc_cfg.format = V4L2_PIX_FMT_YUYV;
            break;

        /* q, quality */
        case 'q':
            DBG("case: q, quality\n");
            input_uvc_cfg.format = V4L2_PIX_FMT_YUYV;
            input_uvc_cfg.gquality = MIN(MAX(atoi(optarg), 0), 100);
            break;

        /* m, minimum_size */
        case 'm':
            DBG("case: m, minimum_size\n");
            input_uvc_cfg.minimum_size = MAX(atoi(optarg), 0);
            break;

        /* n, no_dynctrl */
        case 'n':
            DBG("case: n, no_dynctrl\n");
            input_uvc_cfg.dynctrls = false;
            break;

        /* l, led */
        case 'l':
            DBG("case: l, led\n");
            /*
            if ( strcmp("on", optarg) == 0 ) {
              led = IN_CMD_LED_ON;
            } else if ( strcmp("off", optarg) == 0 ) {
              led = IN_CMD_LED_OFF;
            } else if ( strcmp("auto", optarg) == 0 ) {
              led = IN_CMD_LED_AUTO;
            } else if ( strcmp("blink", optarg) == 0 ) {
              led = IN_CMD_LED_BLINK;
            }
            */
            break;

        /* s, stop */
        case 's':
			DBG("case: s, stop\n");
			input_uvc_cfg.stop_camera = 1;
            break;

        /* p, port */
        case 'p':
            DBG("case: p, port\n");
            server.conf.port = atoi(optarg);
            break;

        /* a, auth */
        case 'a':
            DBG("case: a, auth\n");
            server.conf.auth = strdup(optarg);
            break;

        /* w, www */
        case 'w':
            DBG("case: w, www\n");
            server.conf.www_folder = malloc(strlen(optarg) + 2);
            strcpy(server.conf.www_folder, optarg);
            if(optarg[strlen(optarg)-1] != '/')
                strcat(server.conf.www_folder, "/");
            break;

        /* c, commands */
        case 'c':
            DBG("case: c, commands\n");
            server.conf.control = 1;
            break;

        default:
            DBG("default case\n");
            help();
            return -1;
		}
	}

    return 0;
}

int main(int argc, char **argv)
{
    printf("%s v.%s\n",SOURCE_NAME, SOURCE_VERSION);
    if(getopt_proc(argc, argv))
        exit(1);

    sigaction_init();

    input_uvc_init();
    input_uvc_run();

    httpd_init();
    httpd_run();

    while(run)
    	sleep(3);

    httpd_stop();
    input_uvc_stop();

    return 0;
}
