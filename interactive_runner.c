/**
 * CS60038 - Advances in Operating Systems Design
 * Assignment 1 - Part B
 * 
 * Testing priority queue module
 * 
 * Author: Utkarsh Patel (18EC35034)
 * 
 * This module is written to work on Ubuntu 20.04 operating system having 
 * kernel version 5.6.9
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define DEVICE_NAME "partb_1_17"

#define RED         "\x1B[31m"
#define GRN         "\x1B[32m"
#define RESET       "\x1B[0m"


int main(int argc, const char *argv[]) {
    int n, fd, status;
    pid_t pid;
    char buf[256];
    int32_t value, priority;
    char proc_file[100] = "/proc/";

    strcat(proc_file, DEVICE_NAME);
    
    fd = open(proc_file, O_RDWR);
    if (fd < 0) {
        perror(RED "<main>: Could not open file!\n" RESET);
        exit(1);
    }

    printf("<main>: Enter queue size: ");
    scanf("%d", &n);
    buf[0] = n;

    /* Initialize priority queue */
    status = write(fd, buf, 1);
    if (status < 0) {
        perror(RED "<main>: Cannot write to file!\n" RESET);
        close(fd);
        exit(1);
    }
    printf("<main>: Wrote %d bytes.\n", status);

    while (1) {
        printf("<main>: Perform read/write or quit? [r/w/q]: ");
        scanf("%s", buf);
        if (strlen(buf) != 1 || !(buf[0] == 'r' || buf[0] == 'w' || buf[0] == 'q')) {
            printf("<main>: Invalid option!\n");
            continue;
        }
        if (buf[0] == 'r') {
            /* Reading from priority queue */
            status = read(fd, (void *) &value, sizeof(int32_t));
            if (status < 0) {
                perror(RED "<main>: Reading from priority queue failed!\n" RESET);
                close(fd);
                exit(1);
            }
            printf("<main>: Read %d bytes.\n", status);
            printf("<main>: Highest priority item: %d\n", value);
        }

        else if (buf[0] == 'w') {
            printf("<main>: Enter item value and priority: ");
            scanf("%d %d", &value, &priority);
            
            /* Write item value */
            status = write(fd, &value, sizeof(int32_t));
            if (status < 0) {
                perror(RED "<main>: Writing to priority queue failed!\n" RESET);
                close(fd);
                exit(1);
            }
            printf("<main>: Wrote %d bytes.\n", status);

            /* Write item priority */
            status = write(fd, &priority, sizeof(int32_t));
            if (status < 0) {
                perror(RED "<main>: Writing to priority queue failed!\n" RESET);
                close(fd);
                exit(1);
            }
            printf("<main>: Wrote %d bytes.\n", status);
        }
         
        else break;
    }

    close(fd);
    return 0;
}
