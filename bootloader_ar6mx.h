#ifndef __BOOTLOADER_AR6MX_H__
#define __BOOTLOADER_AR6MX_H__

int write_sysfs(unsigned long value, const char* path);
int update_bootloader(char* srcPath, char* dstPath, unsigned long seek, 
                       unsigned long skip);

#endif
