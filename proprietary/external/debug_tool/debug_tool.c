#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#define LOG_DIRECTORY "/data/local/tmp/logging/"

/* Define used variables */
char device[20];

int show_help(void){
            printf("\n  USAGE:\n");
            printf("    -h Prints this help message.\n");
            printf("    -c Clean up the LOG directory.\n");
            printf("    -l Take logcat.\n");
            printf("    -d Take dmesg.\n");
            printf("    -b Take bugreport.\n");
            printf("    -g Get device properties.\n\n");
            return 0;
}

int main(int argc, char *argv[])
{
        FILE *fp = popen("getprop ro.product.device", "r");
        if (!fp) {
           printf("[-] Failed to open pipe.\n");
           exit(1);
        }

        if (fgets(device, sizeof(device), fp) != 0) {}

        int status = pclose(fp);
        if (status == -1)
        {
	     printf("[-] Failed to close pipe. \n");
	     return 1;
        }

        printf("\n            DEBUG TOOL for %s\n", device);

        if (argc < 2){
           printf("[-] Expected more arguments.\n");
           show_help();
           return 1;
        }

        if (access(LOG_DIRECTORY, F_OK ) != -1 && (*argv[2] != '-c')) {}
        else {
            printf("[?] Logging directory is not present, regenerating...\n");
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
           printf("[+] All OK!\n");
        }

        if(strcmp(argv[1],"-l")==0)
        {
           printf("[?] Taking logcat\n");
           system("timeout 10 logcat > " LOG_DIRECTORY "logcat.log");
           printf("[+] All OK\n");
        }

        if(strcmp(argv[1],"-d")==0)
        {
           /* Check for root */
           if(system("su")==0
           {
               printf("[?] Taking dmesg...\n");
               system("su -c dmesg > " LOG_DIRECTORY "dmesg.log");
               printf("[+] All OK!\n");
           }
           else
           {
              printf("[-] Failed to open su\n");
              exit(1);
           }
        }

        if(strcmp(argv[1],"-g")==0)
        {
           printf("[?] Getting device properties...\n");
           system("getprop > " LOG_DIRECTORY "properties.prop");
           printf("[+] All OK\n");
        }

        if(strcmp(argv[1],"-b")==0)
        {
           printf("[?] Taking bugreport...\n");
           system("timeout 15 bugreport > " LOG_DIRECTORY "bugreport.log");
           printf("[+] All OK!\n");
        }

        else
        {
           printf("[?] Invalid option: %s\n", argv[1]);
           show_help();
           exit(1);
        }
        return 0;
}
