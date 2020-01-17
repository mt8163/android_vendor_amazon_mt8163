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

    static const struct option longopts[] = {
        {.name = "Help Message", .has_arg = no_argument, .val = 'h'},
        {.name = "Cleans up logging directories", .has_arg = no_argument, .val = 'c'},
        {.name = "Takes logcat", .has_arg = no_argument, .val = 'l'},
        {.name = "Takes dmesg", .has_arg = no_argument, .val = 'd'},
        {.name = "Takes bugreport", .has_arg = no_argument, .val = 'b'},
        {.name = "Get device properties", .has_arg = no_argument, .val = 'g'},
        {},
    };
    for (;;) {
        int opt = getopt_long(argc, argv, "hcldbg", longopts, NULL);
        if (opt == -1) // If not enough arguments exit the program.
            break;
        switch (opt) {
        case 'h':
            show_help();
            break;
        case 'c':
            printf("[?] Cleaning up...\n");
            system("rm -rf " LOG_DIRECTORY);
            printf("[+] All OK!\n");
            break;
        case 'l':
            printf("[?] Taking logcat...\n");
            system("timeout 10 logcat > " LOG_DIRECTORY "logcat.log");
            printf("[+] All OK!\n");
            break;
        case 'd':
            printf("[?] Taking dmesg...\n");
            system("timeout 10 dmesg -w > " LOG_DIRECTORY "dmesg.log");
            printf("[+] All OK!\n");
            break;
        case 'b':
            printf("[?] Taking bugreport...\n");
            system("timeout 15 bugreport > " LOG_DIRECTORY "bugreport.log");
            printf("[+] All OK!\n");
            break;
        case 'g':
            printf("[?] Getting device props...\n");
            system("getprop > " LOG_DIRECTORY "properties.prop");
            printf("[+] All OK!\n");
        default:
            // Return 1 if unexpected option
            return 1;
        }
    }
    return 0;
}
