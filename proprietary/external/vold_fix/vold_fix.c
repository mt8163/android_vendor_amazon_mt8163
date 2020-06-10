#define LOG_TAG "vold_fix"

#include <cutils/properties.h>
#include <errno.h>
#include <log/log.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define INTERNAL_STORAGE "/storage/emulated/0"

int main() {
  struct stat s;
  int ret = stat(INTERNAL_STORAGE, &s);
  ALOGD("Initalizing Vold_fix 1.0 \n");
  sleep(2);
  if (ret == -1) {
    if (ENOENT == errno) {
      ALOGD("Executing Vold \n");
      ALOGD("Linking /storage/emulated/0 to storage/self/primary");
      execl("ln", "-sf", "/storage/emulated/0", "storage/self/primary", NULL);
      system("/system/bin/vold --blkid_context=u:r:blkid:s0 --blkid_untrusted_context=u:r:blkid_untrusted:s0 --fsck_context=u:r:fsck:s0 --fsck_untrusted_context=u:r:fsck_untrusted:s0");
      ALOGD("Mounted \n");
      return 0;
    } else {
      perror("stat");
      ALOGE("Failed to mount sdcard! Try rebooting... \n");
      return -1;
    }
  }
  return -2;
}
