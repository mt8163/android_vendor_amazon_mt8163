#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>

#define LOG_DIRECTORY " /data/local/tmp/logging/"

char device[1024];

int main(int argc, char *argv[]){
        FILE *fp = popen("getprop ro.product.device", "r");
    static const struct option longopts[] = {
        {.name = "Help Message", .has_arg = no_argument, .val = 'h'},
        {.name = "Cleans up logging directories", .has_arg = no_argument, .val = 'c'},
        {.name = "Prepares the environment to log", .has_arg = no_argument, .val = 'p'},
        {.name = "Takes logcat", .has_arg = no_argument, .val = 'l'},
        {.name = "Takes dmesg", .has_arg = no_argument, .val = 'd'},
        {.name = "Takes bugreport", .has_arg = no_argument, .val = 'b'},
        {.name = "Get device properties", .has_arg = no_argument, .val = 'g'},
        {},
    };
    for (;;) {
        int opt = getopt_long(argc, argv, "hcpldbg", longopts, NULL);
        if (opt == -1) // If not enough arguments exit the program.
            break;
        switch (opt) {
        case 'h':
            if (!fp) {
		printf("Failed to open pipe \n");
		return 1;
	    }
        
            if(fgets(device, sizeof(device), fp) != 0) {
                printf("\nDevice: ");
		printf("%s", device);
	    }

            int status = pclose(fp);
            if (status == -1)
            {
		printf("Failed to close pipe \n");
		return 1;
            }
            printf("\n              DEBUG_TOOL\n    Usage:\n");
            printf("    -h Prints this help message.\n");
            printf("    -c Clean up the LOG directory.\n");
            printf("    -p Prepare the log environment.\n");
            printf("    -l Take logcat.\n");
            printf("    -d Take dmesg.\n");
            printf("    -b Take bugreport.\n");
            printf("    -g Get device properties.\n\n");
            break;
        case 'p':
            printf("Preparing environment...\n");
            system("mkdir -p" LOG_DIRECTORY);
            printf("All OK!\n");
            break;
        case 'c':
            printf("Cleaning up...\n");
            system("rm -rf" LOG_DIRECTORY);
            printf("All OK!\n");
            break;
        case 'l':
            printf("Taking logcat...\n");
            system("timeout 10 logcat >" LOG_DIRECTORY "logcat.log");
            printf("All OK!\n");
            break;
        case 'd':
            printf("Taking dmesg...\n");
            system("timeout 10 dmesg -w >" LOG_DIRECTORY "dmesg.log");
            printf("All OK!\n");
            break;
        case 'b':
            printf("Taking bugreport...\n");
            system("timeout 15 bugreport >" LOG_DIRECTORY "bugreport.log");
            printf("All OK!\n");
            break;
        case 'g':
            printf("Getting device props...\n");
            system("getprop >" LOG_DIRECTORY "properties.prop");
            printf("All OK!\n");
            break;
        default:
            // Return 1 if unexpected option
            return 1;
        }
    }
    return 0;
}
