#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <getopt.h>

#define WIDTH     640
#define HEIGHT    480

int list_controls_requested = 0;

struct buffer {
   void *start;
   size_t length;
};

struct v4l2_capability_info {
   __u32 capability;
   const char *name;
};

const struct v4l2_capability_info cap_info[] = {
   {V4L2_CAP_VIDEO_CAPTURE, "Video capture device"},
   {V4L2_CAP_VIDEO_OUTPUT, "Video output device"},
   {V4L2_CAP_VIDEO_OVERLAY, "Video overlay"},
   {V4L2_CAP_VBI_CAPTURE, "Raw VBI capture device"},
   {V4L2_CAP_VBI_OUTPUT, "Raw VBI output device"},
   {V4L2_CAP_SLICED_VBI_CAPTURE, "Sliced VBI capture device"},
   {V4L2_CAP_SLICED_VBI_OUTPUT, "Sliced VBI output device"},
   {V4L2_CAP_RDS_CAPTURE, "RDS data capture"},
   {V4L2_CAP_VIDEO_OUTPUT_OVERLAY, "Video output overlay"},
   {V4L2_CAP_HW_FREQ_SEEK, "Hardware frequency seeking"},
   {V4L2_CAP_RDS_OUTPUT, "RDS encoder"},
   {V4L2_CAP_VIDEO_CAPTURE_MPLANE, "Vid cap device supports multiplanar formats"},
   {V4L2_CAP_VIDEO_OUTPUT_MPLANE, "Vid out device supports multiplanar formats"},
   {V4L2_CAP_VIDEO_M2M_MPLANE, "mem-to-mem device supports multiplanar formats"},
   {V4L2_CAP_VIDEO_M2M, "mem-to-mem device"},
   {V4L2_CAP_TUNER, "Has a tuner"},
   {V4L2_CAP_AUDIO, "Audio support"},
   {V4L2_CAP_RADIO, "Radio device"},
   {V4L2_CAP_MODULATOR, "Has modulator"},
   {V4L2_CAP_SDR_CAPTURE, "SDR capture device"},
   {V4L2_CAP_EXT_PIX_FORMAT, "Supports extended pixel format"},
   {V4L2_CAP_SDR_OUTPUT, "SDR output device"},
   //{V4L2_CAP_META_CAPTURE, "Metadata capture device"}, /*NOT IN OUR KERNEL*/
   {V4L2_CAP_READWRITE, "R/W system calls"},
   {V4L2_CAP_ASYNCIO, "Asynchronus I/O"}, //deprecated in latest kernel version as of 2024-06-06 - remove if updated
   {V4L2_CAP_STREAMING, "Streaming I/O ioctls"},
   //{V4L2_CAP_META_OUTPUT, "Metadata output device"}, /*NOT IN OUR KERNEL*/
   {V4L2_CAP_TOUCH, "Touch capable"},
   //{V4L2_CAP_IO_MC, "I//O controlled by media controller"} /*NOT IN OUR KERNEL*/
   //{V4L2_CAP_DEVICE_CAPS, "Set device caps"} /*might break something, dont use it*/
};

void usage(char *program_name) {
   printf("Usage: %s [-lc]\n", program_name);
   printf("Options:\n");
   printf("\t-lc\tList available controls\n");
   exit(EXIT_FAILURE);
}

int xioctl(int fd, int request, void *arg) {
   int r;

   //Need to do{} first before checking for -1 and EINTR
   do {
      r = ioctl(fd, request, arg);
   } while (r == -1 && errno == EINTR);

   return r;
}

void print_capabilities(__u32 caps) {
   for (size_t i=0; i < sizeof(cap_info) / sizeof(cap_info[0]); ++i) {
      if (caps & cap_info[i].capability) {
         printf("\t%s\n", cap_info[i].name);
      }
   }
}

void print_control_value(int fd, __u32 controlId) {
   struct v4l2_control control;
   memset(&control, 0, sizeof(control));
   control.id = controlId;

   if (ioctl(fd, VIDIOC_G_CTRL, &control) == -1) {
      perror("VIDIOC_G_CTRL");
      return;
   }

   printf("\t- Current: %d\n\n", control.value);
}

void list_controls(int fd) {
   struct v4l2_queryctrl queryctrl;
   memset(&queryctrl, 0, sizeof(queryctrl));
   queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL; //Start with first control

   printf("Supported controls:\n");

   do {
      if (ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == -1) {
         return; //eventually we run out of next ctrls, just return and ignore errno
      }
      printf("\t%s\n", queryctrl.name);
      //printf("\t- ID: %d\n", queryctrl.id);
      printf("\t- Min: %d, Max: %d, Step: %d\n",
            queryctrl.minimum, queryctrl.maximum, queryctrl.step);
      print_control_value(fd, queryctrl.id);
      
      queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
   } while (queryctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL);
}

int main(int argc, char *argv[]) {
   //Parse cli args
   int opt;
   while ((opt = getopt(argc, argv, "lc")) != -1) {
      switch (opt) {
         case 'l':
         case 'c':
            list_controls_requested = 1;
            break;
         default:
            usage(argv[0]);
      }
   }

   //Open device 'file'
   const char *dev_name = "/dev/video0";
   int fd = open(dev_name, O_RDWR);
   if (fd == -1) {
      perror("Error opening video device");
      return 1;
   }

   //List available controls
   if (list_controls_requested) {
      //Get the device capabilities
      struct v4l2_capability cap;
      if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
         perror("Error querying capabilities");
         close(fd);
         return 1;
      }
      //Print device capabilities for debugging
      printf("-------------\n");
      printf("Driver: %s\n", cap.driver);
      printf("Card: %s\n", cap.card);
      printf("Bus info: %s\n", cap.bus_info);
      printf("Version: %u.%u.%u\n",
               (cap.version >> 16) & 0xFF,
               (cap.version >> 8) & 0xFF,
               cap.version & 0xFF);
      //Print capabilities list
      printf("Driver capabilities:\n");
      print_capabilities(cap.capabilities);
      printf("\n-------------\n");
      list_controls(fd);
      printf("-------------\n");
   }

   //Setup v4l2 pixel format
   printf("\n-------------\n");
   struct v4l2_format fmt;
   memset(&fmt, 0, sizeof(fmt));
   printf("Cleared [%zu] bytes from [%p] to [%p]\n", sizeof(fmt), (void *)&fmt, ((char *)&fmt + sizeof(fmt))-1);
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.width = WIDTH;
   fmt.fmt.pix.height = HEIGHT;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
   fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

   //Set pixel format
   if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
      perror("Error setting pixel format");
      close(fd);
      return 1;
   } else {
      printf("Pixel format set:\nwidth=%d, height=%d, type=%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.type);
   }
   printf("-------------\n");

   //Setup v4l2 request buffer
   printf("\n-------------\n");
   struct v4l2_requestbuffers request;
   memset(&request, 0, sizeof(request));
   printf("Cleared [%zu] bytes from [%p] to [%p]\n", sizeof(request), (void *)&request, ((char *)&request + sizeof(request))-1);
   request.count = 1;
   request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   request.memory = V4L2_MEMORY_MMAP;

   //Request buffer
   if (xioctl(fd, VIDIOC_REQBUFS, &request) == -1) {
      perror("Error requesting buffer");
      close(fd);
      return 1;
   } else {
      printf("Buffer requested:\ncount=%d\n", request.count);
   }
   printf("-------------\n");

   //Setup buffer query
   printf("\n-------------\n");
   struct v4l2_buffer buf;
   memset(&buf, 0, sizeof(buf));
   printf("Cleared [%zu] bytes from [%p] to [%p]\n", sizeof(buf), (void *)&buf, ((char *)&buf + sizeof(buf))-1);
   buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   buf.memory = V4L2_MEMORY_MMAP;
   buf.index = 0;

   //Query the buffer
   if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
      perror("Error querying buffer");
      close(fd);
      return 1;
   } else {
      printf("Buffer queried:\nlength=%u, offset=%u\n", buf.length, buf.m.offset);
   }
   if (buf.length == 0) {
      perror("Buffer length is 0");
      close(fd);
      return 1;
   }
   if (buf.m.offset == 0) {
      printf("Warning: Buffer offset is 0\n");
   }
   //Not sure if we should continue when buffer offset is 0? Lets try it
   printf("-------------\n");

   //Map the v4l2 buffer to memory
   printf("\n-------------\n");
   struct buffer buffer;
   buffer.length = buf.length;

   //Debugging
   printf("Buffer length: %zu\n", buf.length);
   printf("Buffer offset: %lld\n", (long long)buf.m.offset);

   //Memory map the buffer
   buffer.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
   if (buffer.start == MAP_FAILED) {
      perror("Error memory mapping buffer");
      close(fd);
      return 1;
   } else {
      printf("Buffer mapped:\nstart=%p, length=%zu\n", buffer.start, buffer.length);
   }
   printf("-------------\n");
   
   //Prepare to queue the buffer
   printf("\n-------------\n");
   memset(&buf, 0, sizeof(buf)); //need to clear the buffer again
   printf("Cleared [%zu] bytes from [%p] to [%p]\n", sizeof(buf), (void *)&buf, ((char *)&buf + sizeof(buf))-1);
   buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   buf.memory = V4L2_MEMORY_MMAP;
   buf.index = 0;

   //Queue it (NOT query | VIDIOC_QUERYBUF)
   if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
      perror("Error queuing buffer");
      munmap(buffer.start, buffer.length);
      close(fd);
      return 1;
   } else {
      printf("Buffer queued\n");
   }
   printf("-------------\n");

   // Start capturing video
   printf("\n-------------\n");
   int type = buf.type;
   if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
      perror("Error starting capture");
      munmap(buffer.start, buffer.length);
      close(fd);
      return 1;
   } else {
      printf("Stream started\n");
   }
   printf("-------------\n");

   //De-queue the buffer to get frame
   printf("\n-------------\n");
   if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
      perror("Error retrieving frame");
      munmap(buffer.start, buffer.length);
      close(fd);
      return 1;
   } else {
      printf("Frame dequeued:\nbytesused=%u\n", buf.bytesused);
   }
   printf("-------------\n");

   //Ensure bytesused set properly, maybe not needed?
   buf.bytesused = buf.length;

   /* This doesn't work for now
   //We have the frame, process it to find ratio of black pixels
   unsigned char *yuyv = (unsigned char *)buffer.start;
   int black_pixels = 0;
   int total_pixels = WIDTH * HEIGHT;

   for (int i = 0; i < total_pixels * 2; i += 2) {
      unsigned char y = yuyv[i];
      if (y < 16)
         black_pixels++;
   }

   double black_px_ratio = (double)black_pixels / total_pixels;
   printf("Black pixel ratio: %.2f%%\n", black_px_ratio * 100);
   */

   //Save raw frame for MJPG conversion later
   FILE *out_fp = fopen("frame.raw", "wb");
   if (out_fp == NULL) {
      perror("Error opening output file");
      munmap(buffer.start, buffer.length);
      close(fd);
      return 1;
   }
   fwrite(buffer.start, 1, buf.bytesused, out_fp);

   //Cleanup memory & files
   fclose(out_fp);
   munmap(buffer.start, buffer.length);
   close(fd);

   return 0;
}