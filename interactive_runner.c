#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>

#define PERMS 0666                         /* all users can read and write */
#define DEVICE_NAME "cs60038_a2_17"

#define PB2_SET_CAPACITY _IOW(0x10, 0x31, int32_t *)
#define PB2_INSERT_INT   _IOW(0x10, 0x32, int32_t *)
#define PB2_INSERT_PRIO  _IOW(0x10, 0x33, int32_t *) 
#define PB2_GET_INFO     _IOW(0x10, 0x34, int32_t *)
#define PB2_GET_MIN      _IOW(0x10, 0x35, int32_t *)
#define PB2_GET_MAX      _IOW(0x10, 0x36, int32_t *)

#define RED         "\x1B[31m"
#define GRN         "\x1B[32m"
#define RESET       "\x1B[0m"

struct obj_info {
	int32_t prio_que_size; 	/* current number of elements in priority-queue */
	int32_t capacity;		/* maximum capacity of priority-queue */
};


int main(int argc, const char *argv[]) {
    int fd, status;
    char proc_file[100] = "/proc/";

    strcat(proc_file, DEVICE_NAME);
    
    fd = open(proc_file, O_RDWR);
    if (fd < 0) {
        perror(RED "<main>: Could not open file!\n" RESET);
        exit(1);
    }

    int ops, flag = 1;
    int32_t num;
    struct obj_info obj_info;

    while (flag) {
        /* Print menu */
        printf("\n\n--------------------\n\n");
        printf("[1] SET_CAPACITY\n");
        printf("[2] INSERT_INT\n");
        printf("[3] INSERT_PRIO\n");
        printf("[4] GET_INFO\n");
        printf("[5] GET_MIN\n");
        printf("[6] GET_MAX\n");
        printf("[7] Exit\n");
        printf("\n[*] Enter your choice [1..7]: ");
        scanf("%d", &ops);
    
        switch (ops) {
            case 1:
                printf("[*] Enter priority queue size: ");
                scanf("%d", &num);
                status = ioctl(fd, PB2_SET_CAPACITY, &num);
                if (status) {
                    perror(RED "[-] Error while initializing queue!\n" RESET);
                    close(fd);
                    exit(1);
                }
                printf("[+] Initialized queue with size %d.\n", num);
                break;
            
            case 2:
                printf("[*] Enter item value: ");
                scanf("%d", &num);
                status = ioctl(fd, PB2_INSERT_INT, &num);
                if (status) {
                    perror(RED "[-] Error while caching item value!\n" RESET);
                    close(fd);
                    exit(1);
                }
                printf("[+] Item value %d successfully cached.\n", num);
                break;

            case 3:
                printf("[*] Enter item priority: ");
                scanf("%d", &num);
                status = ioctl(fd, PB2_INSERT_PRIO, &num);
                if (status) {
                    perror(RED "[-] Error while pushing item to queue!\n" RESET);
                    close(fd);
                    exit(1);
                }
                printf("[+] Item with priority %d successfully pushed.\n", num);
                break;
            
            case 4:
                status = ioctl(fd, PB2_GET_INFO, &obj_info);
                if (status) {
                    perror(RED "[-] Error while extracting queue metadata!\n" RESET);
                    close(fd);
                    exit(1);
                }
                printf("[+] Queue size: %d, Queue capacity: %d\n", obj_info.prio_que_size, obj_info.capacity);
                break;

            case 5:
                status = ioctl(fd, PB2_GET_MIN, &num);
                if (status) {
                    perror(RED "[-] Error while extracting minimum!\n" RESET);
                    close(fd);
                    exit(1);
                }
                printf("[+] Minimum priority item: %d\n", num);
                break;
            
            case 6:
                status = ioctl(fd, PB2_GET_MAX, &num);
                if (status) {
                    perror(RED "[-] Error while extracting maximum!\n" RESET);
                    close(fd);
                    exit(1);
                }
                printf("[+] Maximum priority item: %d\n", num);
                break;
            
            case 7:
                flag = 0;
                break;
            
            default:
                printf("[-] Invalid choice!\n");
        }
    }

    close(fd);
    return 0;
}
