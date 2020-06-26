#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

void show_help() 
{
    printf("    USAGE:\n");
    printf("    -h Prints this help message.\n");
    printf("    -c Clean up the LOG directory.\n");
    printf("    -l Take logcat.\n");
    printf("    -d Take dmesg.\n");
    printf("    -b Take bugreport.\n");
    printf("    -g Get device properties.\n");
    printf("    -s Change SELinux status.\n\n");
}

int check_root()
{
   char whoami[1024];
   char su[1024];
   FILE *fp;

   fp = popen("whoami", "r");
   if (fp == NULL) 
   {
     printf("\n[-] Failed to open pipe.\n" );
     return -1;
   }

   while (fgets(whoami, sizeof(whoami), fp) != NULL) {} /* Loop to read whoami's output */

   printf("\n[?] Current id is %s", whoami);

   if (strstr(whoami, "shell") != NULL) 
   {
      printf("[!] Not running from a root shell. Checking for su...\n");
      pclose(fp);

      fp = popen("which su", "r");
      if (fp == NULL) 
      {
         printf("[-] Failed to open pipe\n");
         return -1;
      }

      while (fgets(su, sizeof(su), fp) != NULL) {} /* Loop to read su's output */

      if (strcmp(su, "") == 0) 
      {
         /* If the output is empty... */
         printf("[-] Cannot open su\n");
         pclose(fp);
         return -1;
      }

      strtok(su, "\n"); /* Remove the newlines */

      printf("[?] su binary is present at : %s \n", su);
      printf("[!] Run the tool from a root shell please\n");

      pclose(fp);

      return -1;
    }

    pclose(fp);

    return 0;
}

int write_int(char *file, int value)
{
    FILE *fp = fopen(file, "w");

    if (fp == NULL){
      printf("[-] Failed to open %s!\n", file);
      return 1;
   }

   printf("[?] Write %d to %s\n", value, file);
   fprintf(fp,"%d",value);

   /* Flush the data, otherwise won't actually write anything */
   fflush(fp);

   fclose(fp);

   return 0;
}
