#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#include "bootloader_ar6mx.h"

int write_sysfs(unsigned long value, const char* path) {
   int node_fd = 0;
   int result = 0, conv_result = 0;

   // Convert value to write to string 
   char* buf = malloc(sizeof(unsigned long));
   conv_result = snprintf(buf, sizeof(buf), "%lu", value);
   if (conv_result < 0) {
      fprintf(stderr, "Conversion problem for value %lu on node %s", 
              value, path);
      result = -1; 
      return result;
   }   

   // open node
   node_fd = open(path, O_SYNC | O_WRONLY);
 
   // if open was opened successfully, write to node
   if (node_fd > 0) {
      fprintf(stdout, "Writing to %s (%d) the value %s from buffer of size %d\n", 
              path, node_fd, buf, sizeof(buf));
      result = write(node_fd, buf, sizeof(buf));
   
      // sanity check write to node
      if (result < 0) {
         fprintf(stderr, "Could not write sysfs node %s (%d)\n", 
         strerror(errno), errno);
         result = errno;
      }   
      else {
	 fprintf(stdout, "Successfully wrote sysfs node %s with value %lu\n",
	         path, value);
      }

      // free your resources
      close(node_fd);
   }   
   // problem opening node, report and return
   else {
      fprintf(stderr, "Could not open sysfs node %s\n", 
              strerror(errno));
      result = errno;
      return result;   
   }   
   free(buf);
   return result;
}

int update_bootloader(char* srcPath, char* dstPath, unsigned long seek, 
                       unsigned long skip) {
   FILE *srcFile = NULL, *dstLocation = NULL;
   int bytesRead, bytesWritten, bytesTotal=0;
   char buf[1024];
   int result;

   // open the source file at srcPath
   srcFile = fopen(srcPath, "r");
   if (srcFile == NULL) {
      fprintf(stderr, "Source File %s not found\n", srcPath);
      fprintf(stderr, "error given: %s\n", strerror(errno));
      return -1;
   }
   else {
       fprintf(stdout, "Successfully open source file %s\n",
               srcPath);
   }  

   // open the destination at dstPath
   dstLocation = fopen(dstPath, "w");
   if (dstLocation == NULL)  {
      fprintf(stderr, "Destination path %s not found\n", dstPath);
      fprintf(stderr, "error given: %s\n", strerror(errno));
      fclose(srcFile);
      return -2;
   }
   else {
       fprintf(stdout, "Successfully opened destination path %s\n",
               dstPath);
   }  

   // adjust starting location of bootloader write
   result = fseek(dstLocation, seek, 0);
   if (result != 0) {
      fprintf(stderr, "Error with destination seek op %s\n", strerror(errno));
   }
   else {
      fprintf(stdout, "Destination seek op returned okay\n");
   }

   // adjust starting location of bootloader file read
   result = fseek(srcFile, skip, 0);
   if (result != 0) {
      fprintf(stderr, "Error with source seek op %s\n", strerror(errno));
   }
   else {
       fprintf(stdout, "Source seek op returned okay\n");
   }

   fprintf(stdout, "Will now write %s bootloader image to %s with passed offsets %lu %lu\n",
	   srcPath, dstPath, seek, skip); 
   // copy in 1k chucks (or 1024 1 byte ites) from source to destination
   while(!feof(srcFile)) {
      bytesRead = fread(buf, 1, sizeof(buf), srcFile);
      bytesWritten = fwrite(buf, 1, bytesRead, dstLocation);
      if (bytesRead != bytesWritten) {
         fprintf(stderr, "Mismatch in copying %s to %s, read=%d written=%d \n",
                 srcPath, dstPath, bytesRead, bytesWritten);
      }
      else {
	 bytesTotal += bytesWritten;
         fprintf(stdout, "written=%d, total=%d\n", bytesWritten, bytesTotal);
      }
   }

   // free resources
   fclose(srcFile);
   fclose(dstLocation);
   fprintf(stdout, "update_bootloader call complete\n");
   return 0;
}
