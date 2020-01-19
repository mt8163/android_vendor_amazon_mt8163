#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <cutils/properties.h>

#define LOG_DIRECTORY "/data/local/tmp/logging/"

/* Define used variables */
char device[PROP_VALUE_MAX+1];

int show_help(void){
            printf("    USAGE:\n");
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
        __system_property_get("ro.product.device", device);        

        printf("\n            DEBUG TOOL for %s\n", device);

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
           if(system("su")==0)
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
        return 0;
}
