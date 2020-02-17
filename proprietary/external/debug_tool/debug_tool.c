#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <cutils/properties.h>
#include <string.h>

#define LOG_DIRECTORY "/data/local/tmp/logging/"
#define SELINUX "/sys/fs/selinux/enforce"


/* Define used variables */
char device[PROP_VALUE_MAX+1];

int show_help(void){
            printf("    USAGE:\n");
            printf("    -h Prints this help message.\n");
            printf("    -c Clean up the LOG directory.\n");
            printf("    -l Take logcat.\n");
            printf("    -d Take dmesg.\n");
            printf("    -b Take bugreport.\n");
            printf("    -g Get device properties.\n");
            printf("    -s Change SELinux status.\n\n");
            return 0;
}

/* Checks the current uid of the shell and if the su binary
   Is present in the device */
int check_root(void){
   FILE *fp;
   char whoami[1024];
   fp = popen("whoami", "r");
   if (fp == NULL) {
     printf("\n[-] Failed to open pipe.\n" );
     return 1;
     exit(1);
   }

   while (fgets(whoami, sizeof(whoami), fp) != NULL) {}

   printf("\n[?] Current id is %s", whoami);

   if (strstr(whoami, "shell") != NULL) {
      printf("[!] Not running from a root shell. Checking for su...\n");
      pclose(fp);
      FILE *fp;
      char su[1024];
      fp = popen("which su", "r");
      if (fp == NULL) {
         printf("[-] Failed to open pipe\n");
         return 1;
         exit(1);
      }
      while (fgets(su, sizeof(su), fp) != NULL) {}
      if(strcmp(su,"")== 0) {
         printf("[-] Cannot open su\n");
         return 1;
         exit(1);
         pclose(fp);
      }
      printf("[?] su binary is present at : %s \n[!] Run the tool from a root shell please\n", su);
      pclose(fp);
      return 1;
    }
    printf("[?] Running from a root shell (yay!)\n");
    pclose(fp);
    return 0; /* whoami is root and didn't checked su */
}


/* Writes an int value (float not allowed) to an specified file */
int write_int(char *file, int value){
   FILE *fptr;
   int fptr_close;
   printf("[?] Write %d to %s\n", value, file);
   fptr = fopen(file,"w");
   if (fptr == NULL){
      printf("[-] Failed to open %s!\n", file);
      return 1;
   }
   fprintf(fptr,"%d",value);
   fflush(fptr);
   fptr_close = fclose(fptr);
   printf("[?] fclose returns %d\n", fptr_close);
   if (fptr_close != 0){
      printf("[-] Failed to close %s!\n", file);
      return 1;
   }
   return 0;
}

int main(int argc, char *argv[])
{
        __system_property_get("ro.product.device", device);

        printf("\n            DEBUG TOOL for %s\n\n", device);

        /* Check if argc length is 2 */
        if (argc < 2){
           printf("[-] Expected more arguments.\n\n");
           show_help();
           return 1;
        }

        /* Check if user entered more than 1 argument */
        if (argc > 2){
           printf("[-] Only 1 argument is required.\n\n");
           show_help();
           return 1;
        }

        if (access(LOG_DIRECTORY, F_OK ) != -1 && (*argv[2] != '-c')) {}
        else {
            printf("[?] Logging directory is not present, regenerating...\n\n");
            system("mkdir -p /data/local/tmp/logging");
        }

        if(strcmp(argv[1],"-h")==0)
        {
           show_help();
        }

        if(strcmp(argv[1],"-c")==0)
        {
           printf("[?] Cleaning up...\n");
           system("rm -rf " LOG_DIRECTORY);
           printf("[+] All OK!\n\n");
        }

        if(strcmp(argv[1],"-l")==0)
        {
           printf("[?] Taking logcat\n");
           system("timeout 10 logcat > " LOG_DIRECTORY "logcat.log");
           printf("[+] All OK\n\n");
        }

        if(strcmp(argv[1],"-d")==0)
        {
        int response = check_root();
        if (response == 1) {
            printf("[-] Attempt to execute a privileged action being an unqualified user?\n\n");
            return 1;
            exit(1);
        }
        printf("[?] Taking dmesg...\n");
        system("dmesg > " LOG_DIRECTORY "dmesg.log");
        printf("[+] All OK!\n\n");
        }

        if(strcmp(argv[1],"-g")==0)
        {
           printf("[?] Getting device properties...\n");
           system("getprop > " LOG_DIRECTORY "properties.prop");
           printf("[+] All OK\n\n");
        }

        if(strcmp(argv[1],"-b")==0)
        {
           printf("[?] Taking bugreport...\n");
           system("timeout 15 bugreport > " LOG_DIRECTORY "bugreport.log");
           printf("[+] All OK!\n\n");
        }

        if(strcmp(argv[1],"-s")==0)
        {
           int response = check_root();
           if (response == 1) {
               printf("[-] Attempt to write to a secured file being an unqualified user?\n\n");
               return 1;
               exit(1);
           }
           int input;
           printf("[?] Switch SELinux to permissive (0) or enforcing (1)?\n");
           scanf("%d", &input);
           if (input != 0 && input != 1) {
               printf("[-] Invalid value: %d\n\n", input);
               return 1;
               exit(1);
           }
           if (input == 0) {
             printf("[?] User wants to switch SELinux to permissive...\n");
             printf("[?] Setting SELinux to permissive...\n");
             write_int("/sys/fs/selinux/enforce", 0);
           }
           if (input == 1) {
             printf("[?] User wants to switch SELinux to enforcing...\n");
             printf("[?] Setting SELinux to enforcing...\n");
             write_int("/sys/fs/selinux/enforce", 1);
           }
           printf("[+] Done\n\n");
        }
        return 0;
}
