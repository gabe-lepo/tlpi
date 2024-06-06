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

   //Need to do{} first before checking for -1 and EINTR
   do {
      r = ioctl(fd, request, arg);
   } while (r == -1 && errno == EINTR);

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

   //Get the device capabilities
   struct v4l2_capability cap;
   if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
      perror("Error querying capabilities");
      close(fd);
      return 1;
   }

   //Print device capabilities for debugging
   printf("\n-------------\n");
   printf("Driver: %s\n", cap.driver);
   printf("Card: %s\n", cap.card);
   printf("Bus info: %s\n", cap.bus_info);
   printf("-------------\n");

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
   fclose(out_fp);

   //Cleanup memory & fd
   munmap(buffer.start, buffer.length);
   close(fd);

   return 0;
}