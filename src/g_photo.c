#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#define WIDTH     640
#define HEIGHT    480

struct buffer {
   void *start;
   size_t length;
};

int xioctl(int fd, int request, void *arg) {
   int r;

   while (r == -1 && errno == EINTR)
      r = ioctl(fd, request, arg);

   return r;
}

int main() {
   //Open device 'file'
   const char *dev_name = "/dev/video0";
   int fd = open(dev_name, O_RDWR);
   if (fd == -1) {
      perror("Error opening video device");
      return 1;
   }

   //Setup v4l2 pixel format
   struct v4l2_format fmt;
   memset(&fmt, 0, sizeof(fmt));
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.width = WIDTH;
   fmt.fmt.pix.height = HEIGHT;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
   fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
   if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
      perror("Error setting pixel format");
      close(fd);
      return 1;
   }

   //Setup v4l2 request buffer
   struct v4l2_requestbuffers request;
   memset(&request, 0, sizeof(request));
   request.count = 1;
   request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   request.memory = V4L2_MEMORY_MMAP;
   if (xioctl(fd, VIDIOC_REQBUFS, &request) == -1) {
      perror("Error requesting buffer");
      return 1;
   }

   //Query v4l2 buffer
   struct v4l2_buffer buf;
   memset(&buf, 0, sizeof(buf));
   buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   buf.memory = V4L2_MEMORY_MMAP;
   buf.index = 0;
   if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
      perror("Error querying buffer");
      close(fd);
      return 1;
   }
   if (buf.length == 0 || buf.m.offset == 0) {
      perror("Buffer length or offset is 0");
      close(fd);
      return 1;
   }

   //Map the v4l2 buffer to memory
   struct buffer buffer;
   buffer.length = buf.length;

   //Debugging
   printf("Buffer length: %zu\n", buf.length);
   printf("Buffer offset: %lld\n", (long long)buf.m.offset);

   buffer.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
   if (buffer.start == MAP_FAILED) {
      perror("Error memory mapping buffer");
      return 1;
   }
   
   //Is this necessary?
   memset(&buf, 0, sizeof(buf));
   buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   buf.memory = V4L2_MEMORY_MMAP;
   buf.index = 0;
   if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
      perror("Error querying buffer");
      return 1;
   }

   int type = buf.type;
   if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
      perror("Error starting capture");
      return 1;
   }

   if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
      perror("Error retrieving frame");
      munmap(buffer.start, buffer.length);
      close(fd);
      return 1;
   }

   buf.bytesused = buf.length;

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

   //Save raw frame for MJPG conversion later
   FILE *out_fp = fopen("frame.raw", "wb");
   if (out_fp == NULL) {
      perror("Error opening output file");
      return 1;
   }
   fwrite(buffer.start, 1, buf.bytesused, out_fp);
   fclose(out_fp);

   //Cleanup memory
   munmap(buffer.start, buffer.length);
   close(fd);

   return 0;
}