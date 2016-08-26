#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "edify/expr.h"
#include "bootloader_ar6mx.h"

Value* WriteBootloaderFn(const char* name, State* state, int argc, Expr* argv[]) {
   char *srcPath = NULL, *dstPath = NULL;
   unsigned long seek = 0, skip = 0;
   const char *btLdrFrRoNode = "/sys/block/mmcblk3boot0/force_ro";
   const char *btLdrEnableBoot = "/sys/block/mmcblk3/device/boot_config";
   int result = -1;

   Value* srcPathVal;
   Value* dstPathVal;
   Value* seekVal;
   Value* skipVal;

   // sanity check inputs and extract
   if (argc != 4) {
      return ErrorAbort(state, "%s() expects 4 args, got %d", name, argc);
   }

   if (ReadValueArgs(state, argv, 4, 
                     &srcPathVal, &dstPathVal, &seekVal, &skipVal) < 0) {
      return NULL;
   }

   // extract data from Value structures
   srcPath = malloc(sizeof(char) * strlen(srcPathVal->data));
   strcpy(srcPath, srcPathVal->data);
   dstPath = malloc(sizeof(char) * strlen(dstPathVal->data));
   strcpy(dstPath, dstPathVal->data);
   seek = atol(seekVal->data);
   skip = atol(skipVal->data);

   // write the new bootloader, unlock to write, write the file to emmc, and relock
   result = write_sysfs(0, btLdrFrRoNode);
   if (result < 0) {
      return ErrorAbort(state, "Unlocking MMC boot partition failed");
   }
   result = update_bootloader(srcPath, dstPath, seek, skip);
   if (result < 0) {
      return ErrorAbort(state, "Writing the bootloader from %s to %s with seek=%d and skip=%d failed, your unit may now be bricked result=%d", 
                        srcPath, dstPath, seek, skip, result);
   }
   result = write_sysfs(1, btLdrFrRoNode);
   if (result < 0) {
      return ErrorAbort(state, "Relocking MMC boot partition failed");
   }   
  
   // make sure bootloader is bootable
   result = write_sysfs(8, btLdrEnableBoot);
   if (result < 0) {
      return ErrorAbort(state, "Relocking MMC boot partition failed");
   }   

   // be a good citizen, free your resources
   FreeValue(srcPathVal);
   FreeValue(dstPathVal);
   FreeValue(seekVal);
   FreeValue(skipVal);
 
   // return a good result here
   return StringValue(strdup(result == 0 ? "t" : ""));
}

void Register_librecovery_updater_ar6mx() {

    fprintf(stderr, "installing PDi(R) ar6mx updater extensions\n");

    RegisterFunction("ar6mx.write_bootloader", WriteBootloaderFn);
}

