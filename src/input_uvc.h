#ifndef INPUT_UVC_H
#define INPUT_UVC_H

#include <stdbool.h>
/*
 * UVC resolutions mentioned at: (at least for some webcams)
 * http://www.quickcamteam.net/hcl/frame-format-matrix/
 */
static const struct {
    const char *string;
    const int width, height;
} resolutions[] = {
    { "QSIF", 160,  120  },
    { "QCIF", 176,  144  },
    { "CGA",  320,  200  },
    { "QVGA", 320,  240  },
    { "CIF",  352,  288  },
    { "VGA",  640,  480  },
    { "SVGA", 800,  600  },
    { "XGA",  1024, 768  },
    { "SXGA", 1280, 1024 }
};

struct input_uvc_config {
    char *dev;
    size_t width;
    size_t height;
    size_t fps;
    size_t format;
    bool dynctrls;
    size_t gquality;
    size_t minimum_size;
    int stop_camera;
};

extern struct input_uvc_config input_uvc_cfg;

int input_uvc_init(void);
int input_uvc_run(void);
int input_uvc_stop(void);
int input_uvc_cmd(int plugin_number, unsigned int control_id, unsigned int group, int value);

#endif // INPUT_UVC_H
