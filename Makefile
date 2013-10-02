VERSION=0.1
PACKAGE=uvcstreamer

HEADERS=$(PACKAGE).h \
		input.h output.h utils.h \
		input_uvc.h v4l2uvc.h huffman.h jpeg_utils.h dynctrl.h \
		httpd.h       
		 		 
OBJECTS=$(PACKAGE).o utils.o \
		input_uvc.o v4l2uvc.o jpeg_utils.o dynctrl.o \
		httpd.o 


CROSS_COMPILE=
# arm-none-linux-gnueabi-
STRIP=$(CROSS_COMPILE)strip
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
ADDITIONAL_OBJECTS+=

CFLAGS=-Wall -O1 -DNDEBUG 
#-DDEBUG 

LDFLAGS=-ljpeg -lpthread 
#CFLAGS += -DUSE_LIBV4L2
#LDFLAGS += -lv4l2

all: $(PACKAGE) strip

$(PACKAGE)$(SUFFIX): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(ADDITIONAL_OBJECTS)

%$(SUFFIX).o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -o $@ -c $<

strip: $(PACKAGE)$(SUFFIX)
	$(STRIP) $^

clean:
	rm -f $(OBJECTS) $(PACKAGE)$(SUFFIX)

