/**
 * CS60038 - Advances in Operating Systems Design
 * Assignment 1 - Part B
 * 
 * Loadable Kernel Module for implementing a Priority-queue
 * 
 * Author: Utkarsh Patel (18EC35034)
 * 
 * This module is written to work on Ubuntu 20.04 operating system having 
 * kernel version 5.6.9
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_AUTHOR("Utkarsh Patel");
MODULE_DESCRIPTION("Loadable Kernel Module for implementing a Priority-queue");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");


/* ========================== MODULE INTERFACE ============================== */


static DEFINE_MUTEX(qlock);                /* mutex lock over `queues` */
#define PERMS 0666                         /* all users can read and write */
#define DEVICE_NAME "partb_1_17"

static ssize_t qwrite(struct file *, const char *, size_t, loff_t *);
static ssize_t qread (struct file *, char *      , size_t, loff_t *);

static int qopen   (struct inode *, struct file *);
static int qrelease(struct inode *, struct file *);

static struct proc_ops proc_ops = {
    .proc_open    = qopen,
    .proc_release = qrelease,
    .proc_read    = qread,
    .proc_write   = qwrite,
};

static int  _module_init(void);        /* routine to be passed to module_init */
static void _module_exit(void);        /* routine to be passed to module_exit */

module_init(_module_init);
module_exit(_module_exit);


/**
 * Wrapper for element in the priority queue
 */
struct item_t {
    int32_t value, priority;
};


/**
 * Wrapper for priority queue
 * 
 * Each priority queue is associated with a userspace process and each userspace
 * process can be associated with at most one priority queue.
 */
struct priority_queue {
    struct item_t   *items;        /* array of items */
    size_t          capacity;      /* maximum number of items possible */
    size_t          count;         /* current number of items */
};

#define MAX_PQ_CAPACITY 100        /* every queue's max_capacity should be less
                                      or equal to MAX_PQ_CAPACITY */

/**
 * Routines for handling priority queue
 */
#define LCHILD(x) (x) * 2 + 1
#define RCHILD(x) (x) * 2 + 2
#define PARENT(x) ((x) - 1) / 2

static struct priority_queue *create_queue(size_t);
static void                  free_queue   (struct priority_queue *);
static void                  swap_items   (struct item_t *, struct item_t *);
static int                   compare_items(struct item_t, struct item_t);
static int                   push         (struct priority_queue *, struct item_t);
static int32_t               pop          (struct priority_queue *);
static void                  heapify      (struct priority_queue *, size_t);


/* Linked list of priority queues */
struct queue_list {
    pid_t pid;
    struct priority_queue *queue;
    struct queue_list *next;

    int32_t item_value_cache;
    int is_item_value_cached;
};

static struct queue_list *head;

static struct queue_list *get_queue_list      (pid_t);  
static void              add_queue_list       (pid_t);
static void              delete_queue_list    (pid_t);
static void              free_queue_list      (struct queue_list *);

static void              init_list            (void);
static void              free_list            (void);
static void              print_list           (void);



/* ======================== MODULE IMPLEMENTATION =========================== */


/**
 * @brief Allocates a priority_queue structure in memory
 * 
 * @param capacity: Maximum number of items possible
 * @returns Pointer to a `priority_queue` structure (NULL in case of failure)
 */
static struct priority_queue *create_queue(size_t capacity) {
    struct priority_queue *queue = (struct priority_queue *) 
        kmalloc(sizeof(struct priority_queue), GFP_KERNEL);

    /* Check if priority queue was successfully allocated */
    if (queue == NULL) {
        printk(
            KERN_ALERT "<create_queue@%d>: Failed to allocate a priority queue!\n", 
            current->pid
        );
        return queue;
    }

    /* Initialize priority queue */
    *queue = (struct priority_queue) {
        .capacity = capacity,
        .count    = 0,
        .items    = (struct item_t *) 
                        kmalloc(sizeof(struct item_t) * capacity, GFP_KERNEL),
    };

    /* Check if the array of items was successfully allocated */
    if (queue->items == NULL) {
        printk(
            KERN_ALERT "<create_queue@%d>: Priority queue was allocated successfully, " \
            "but cannot allocate array of [%d] items!\n", current->pid, capacity
        );
        kfree(queue);
        return NULL;
    }

    printk(
        KERN_INFO "<create_queue@%d>: Successful allocation of priority queue with" \
        " capacity [%d].\n", current->pid, capacity
    );
    return queue;
}


/**
 * @brief Deallocates priority queue from memory
 * 
 * @param queue: Pointer to priority_queue structure
 */
static void free_queue(struct priority_queue *queue) {
    /* Null check */
    if (queue == NULL) {
        printk(
            KERN_ALERT "<free_queue@%d>: Attempt to deallocate null pointer.\n", 
            current->pid
        );
        return;
    }
    kfree(queue->items);
    kfree(queue);
    printk(KERN_INFO "<free_queue@%d>: Successful deallocation of queue.\n", current->pid);
}


/**
 * @brief Swap two items in priority queue
 * 
 * @param item1: Pointer to first item
 * @param item2: Pointer to second item
 */
static void swap_items(struct item_t *item1, struct item_t *item2) {
    struct item_t tmp = *item1;
    *item1            = *item2;
    *item2            = tmp;
}

/**
 * @brief Compare two items in the priority queue
 * 
 * @param item1: First item
 * @param item2: Second item
 * 
 * @returns  0, when item1 == item2
 *           1, when item1  > item2
 *          -1, when item1  < item2
 */
static int compare_items(struct item_t item1, struct item_t item2) {
    return (item1.priority > item2.priority) ? 1 :
        (item1.priority < item2.priority) ? -1 : 0;
}


/**
 * @brief Insert an element in a priority queue
 * 
 * @param queue: Pointer to the priority queue where insertion is to be performed
 * @param item: Item to be inserted
 * 
 * @returns 0 for success and -EACCES for failure
 */
static int push(struct priority_queue *queue, struct item_t item) {
    /* Check overflow */
    if (queue->count == queue->capacity) {
        printk(KERN_ALERT "<push@%d>: Overflow in the queue!\n", current->pid);
        return -EACCES;
    }

    /* Push the new item in the priority queue */
    queue->count++;
    int index = queue->count - 1;
    queue->items[index] = item;

    /* Fix priority queue property if it is violated */
    while (index != 0 && 
        compare_items(queue->items[PARENT(index)], queue->items[index]) > 0) {
        swap_items(&queue->items[PARENT(index)], &queue->items[index]);
        index = PARENT(index);
    }
    printk(KERN_INFO "<push@%d>: (%d, %d) pushed to queue.\n", current->pid, 
        item.value, item.priority);
    return 0;
}

/**
 * @brief Remove the highest priority item from priority queue and return it
 * 
 * @param queue: Pointer to priority queue structure
 * 
 * @returns item value for success, -EACCES for failure
 */
static int32_t pop(struct priority_queue *queue) {
    if (queue->count == 0) {
        printk(KERN_ALERT "<pop@%d>: No item to pop.\n", current->pid);
        return -EACCES;
    }

    if (queue->count == 1) {
        queue->count = 0;
        return queue->items[0].value;
    }

    int32_t value = queue->items[0].value;
    queue->items[0] = queue->items[queue->count - 1];
    queue->count--;
    heapify(queue, 0);

    return value;
}


/**
 * @brief A recursive method to heapify a subtree with the root at given index.
 * This method assumes that the subtrees are already heapified.
 * 
 * @param queue: Pointer to priority queue structure
 * @param index: Index to subtree to be heapified
 */
static void heapify(struct priority_queue *queue, size_t index) {
    size_t l_index = LCHILD(index);
    size_t r_index = RCHILD(index);

    /* `pos` points to item having maximum priority among `index`, `l_index` 
        and `r_index` */
    size_t pos = index;

    if (l_index < queue->count && 
        compare_items(queue->items[l_index], queue->items[pos]) < 0) {
        pos = l_index;
    }
    if (r_index < queue->count && 
        compare_items(queue->items[r_index], queue->items[pos]) < 0) {
        pos = r_index;
    }

    if (pos != index) {
        swap_items(&queue->items[index], &queue->items[pos]);
        heapify(queue, pos);
    }
}



/**
 * @brief Fetch the priority queue for given process
 * 
 * @param pid: pid of the process
 * 
 * @return `queue_list` instance holding priority queue
 */
static struct queue_list *get_queue_list(pid_t pid) {
    mutex_lock(&qlock);

    struct queue_list *queue_list = head->next;
    while (queue_list != NULL) {
        if (queue_list->pid == pid) {
            printk(KERN_INFO "<get_queue@%d>: Successfully found the queue.\n", pid);
            mutex_unlock(&qlock);
            return queue_list;
        }
        queue_list = queue_list->next;
    }
    printk(KERN_ALERT "<get_queue@%d>: No queue found!\n", pid);

    mutex_unlock(&qlock);
    return queue_list;
}


/**
 * @brief Allocate and add priority queue for given process in the linked list 
 * 
 * @param pid: pid of the process
 */
static void add_queue_list(pid_t pid) {
    mutex_lock(&qlock);

    struct queue_list *queue_list = (struct queue_list *) 
        kmalloc(sizeof(struct queue_list), GFP_KERNEL);
    *queue_list = (struct queue_list) {
        .pid                  = pid,
        .queue                = NULL,
        .next                 = NULL,
        .item_value_cache     = 0,
        .is_item_value_cached = 0,
    };

    queue_list->next = head->next;
    head->next = queue_list;
    printk(KERN_INFO "<add_queue@%d>: Successfully added the queue.\n", pid);

    mutex_unlock(&qlock);
}


/**
 * @brief Deallocate priority queue for given process
 * 
 * @param pid: pid of the process
 */
static void delete_queue_list(pid_t pid) {
    mutex_lock(&qlock);

    struct queue_list *prv = head;
    struct queue_list *cur = head->next;

    while (cur != NULL) {
        if (cur->pid == pid) {
            prv->next = cur->next;
            free_queue_list(cur);
            printk(KERN_INFO "<delete_queue@%d>: Successfully deleted the queue.\n", pid);
            mutex_unlock(&qlock);
            return;
        }
        prv = cur;
        cur = cur->next;
    }
    printk(KERN_ALERT "<delete_queue@%d>: No queue found!\n", pid);

    mutex_unlock(&qlock);
}


/**
 * @brief Internal helper subroutine for `delete_queue_list`.
 */
static void free_queue_list(struct queue_list *queue_list) {
    /* No mutex lock and unlock as this is an internal helper subroutine */
    if (queue_list == NULL) {
        printk(KERN_ALERT "<free_queue_list@%d>: Attempt to deallocate null pointer!\n", current->pid);
        return;
    }
    free_queue(queue_list->queue);
    printk(KERN_INFO "<free_queue_list@%d>: Deallocated the queue.\n", queue_list->pid);
    kfree(queue_list);
}


/**
 * @brief Allocates head of linked list containing priority queues. It is an
 * internal helper subroutine used by `module_init`.
 */
static void init_list(void) {
    head = (struct queue_list *) kmalloc(sizeof(struct queue_list), GFP_KERNEL);
    *head = (struct queue_list) {
        .pid                  = -1,
        .queue                = NULL,
        .next                 = NULL,
        .item_value_cache     = 0,
        .is_item_value_cached = 0,
    };
    printk(KERN_INFO "<init_list>: Head of queue list allocated.");
}


/**
 * @brief Deallocates entire linked list of priority queues. It is an 
 * internal helper subroutine used by `module_exit`.
 */
static void free_list(void) {
    struct queue_list *p, *q;
    q = head->next;
    free_queue_list(head);
    while (q != NULL) {
        p = q;
        q = q->next;
        free_queue_list(p);
    }
    printk(KERN_INFO "<free_list>: Deallocated all the queues.\n");
}


/**
 * @brief Prints pid of processes for which priority queue is stil alive.
 */
static void print_list(void) {
    mutex_lock(&qlock);

    struct queue_list *q = head->next;
    printk(KERN_INFO "<print_queue_list>: [");
    while (q != NULL) {
        printk("%d, ", q->pid);
        q = q->next;
    }
    printk("]\n");

    mutex_unlock(&qlock);
}



/**
 * @brief Write data to priority queue
 * 
 * @return Number of bytes wrote (if successful)
 *         -EACCES
 *             - when no queue_list is associated with given process
 *             - priority queue overflow
 *         -EINVAL
 *             - invalid input type for item value/priority
 *             - invalid buffer length
 *             - out of range queue size
 *         -ENOMEM
 *             - queue allocation failed
 */
static ssize_t qwrite(struct file *file, const char *buf, size_t count, loff_t *pos) {
    if (!buf || !count) return -EINVAL; /* check buf is not null and count is non-zero */

    /* Get the associated queue_list for current process */
    struct queue_list *queue_list = get_queue_list(current->pid);
    if (queue_list == NULL) {
        /* No queue_list associated with current process */
        printk(
            KERN_ALERT DEVICE_NAME " <write@%d>: No file associated with "
            "current process!\n", current->pid
        );
        return -EACCES;
    }

    int buf_len = count < 256 ? count : 256;

    if (queue_list->queue != NULL) {
        /**
         * `queue_list` is already initialized. Hence, need to write an integer
         * (4-bytes). It may be item's value or priority. We distinguish the two
         * cases using `item_value_cache` and `is_item_value_cached`.
         */

        /* Check if received an integer (4-bytes)? */
        if (buf_len != 4) {
            printk(
                KERN_ALERT DEVICE_NAME " <write@%d>: Invalid argument, "
                "expected an integer (4 bytes)!\n", current->pid
            );
            return -EINVAL;
        }

        int32_t num;
        memcpy(&num, buf, sizeof(char) * buf_len);
        printk(KERN_INFO DEVICE_NAME " <write@%d>: Received %d.\n", current->pid, num);

        if (queue_list->is_item_value_cached) {
            /* `num` will be treated as priority for cached item value */

            /* Check if `num` > 0 as priority is a positive integer */
            if (num <= 0) {
                printk(
                    KERN_ALERT DEVICE_NAME " <write@%d>: Invalid argument, "
                    "priority must be a positive integer!\n", current->pid
                );
                return -EINVAL;
            }

            /* Prepare item to push to priority queue */
            struct item_t new_item = (struct item_t) {
                .value    = queue_list->item_value_cache,
                .priority = num,
            };

            int status = push(queue_list->queue, new_item);
            if (status < 0) {
                /* Overflow in priority queue */
                return status;
            }

            printk(KERN_INFO DEVICE_NAME " <write@%d>: Item inserted in queue.\n", current->pid);
            queue_list->is_item_value_cached = 0;
        } else {
            /* `num` is treated as item value and will be cached for the process */
            queue_list->item_value_cache = num;
            queue_list->is_item_value_cached = 1;
            printk(
                KERN_INFO DEVICE_NAME " <write@%d>: Item value cached, waiting for "
                "item priority.\n", current->pid
            );
        }

        return sizeof(num);
    }

    /* `queue_list` for current process is not initialized */

    /* Check if received only one byte of data */
    if (buf_len != 1) {
        printk(
            KERN_ALERT DEVICE_NAME "<write@%d>: Expected one byte of data, "
            "got %d byte(s)!\n", current->pid, buf_len
        );
        return -EINVAL;
    }

    size_t queue_size = buf[0];
    /* Check if `queue_size` is in valid range */

    if (!(queue_size > 0 && queue_size <= MAX_PQ_CAPACITY)) {
        printk(
            KERN_ALERT DEVICE_NAME "<write@%d>: Priority-queue size "
            "should be in range [1, 100], got %d!", current->pid, queue_size
        );
        return -EINVAL;
    }

    /* Allocate priority queue for current process */
    queue_list->queue = create_queue(queue_size);
    if (queue_list->queue == NULL) {
        /* Error will be reported in `create_queue` method */
        return -ENOMEM;
    }
    
    return buf_len;
}


/**
 * @brief Read value of highest priority item from current process's queue and pop it
 * 
 * @return Number of bytes read (if successful)
 *         -EINVAL
 *             - when number of bytes requested for reading is not 4 (bytes)
 *         -EACCES
 *             - when no queue is associated with current process
 *             - when priority queue is not initialized for current process
 *             - priority queue underflow
 *             - unable to copy item value to buffer `buf`
 */
static ssize_t qread(struct file *file, char *buf, size_t count, loff_t *pos) {
    /* Check if count is non 4 (bytes) */
    if (count != 4) return -EINVAL;

    /* Fetch priority queue for current process */
    struct queue_list *queue_list = get_queue_list(current->pid);
    if (queue_list == NULL) {
        printk(
            KERN_ALERT DEVICE_NAME " <read@%d>: No queue associated with "
            "current process!\n", current->pid
        );
        return -EACCES;
    }

    if (queue_list->queue == NULL) {
        printk(
            KERN_ALERT DEVICE_NAME " <read@%d>: Priority queue is not "
            "initialized!\n", current->pid
        );
        return -EACCES;
    }

    if (queue_list->queue->count == 0) {
        printk(
            KERN_ALERT DEVICE_NAME " <read@%d>: No item present in priority "
            "queue!\n", current->pid
        );
        return -EACCES;
    }

    int32_t item_value = pop(queue_list->queue);
    int status = copy_to_user(buf, (int32_t *)(&item_value), count);

    if (status < 0) {
        /* `copy_to_user` failed */
        printk(
            KERN_ALERT DEVICE_NAME " <read@%d>: copy_to_user failed!\n", 
            current->pid
        );
        return -EACCES;
    }

    return sizeof(item_value);
}


/**
 * @brief Allocates a new `queue_list` instance for current process (if it 
 * doesn't exist)
 * 
 * @return 0 (if successful)
 *         -EACCES
 *             - when current process already has a queue
 */
static int qopen(struct inode *inode, struct file *file) {
    /**
     * If current process has already opened the file, don't allocate a new 
     * `queue_list` instance
     */
    if (get_queue_list(current->pid) != NULL) {
        printk(
            KERN_ALERT DEVICE_NAME " <open@%d>: Current process already "
            "has an associated queue!\n", current->pid
        );
        return -EACCES;
    }

    add_queue_list(current->pid);
    print_list();
    return 0;
}


/**
 * @brief Deallocates priority queue for current process
 */
static int qrelease(struct inode *inode, struct file *file) {
    delete_queue_list(current->pid);
    print_list();
    return 0;
}


/**
 * @brief Initiating module
 * 
 * @return 0 (if successful)
 *         -ENOENT 
 *             -unable to create proc_entry
 */
static int _module_init(void) {
    /* Create proc directory for the module */
    struct proc_dir_entry *entry = proc_create(DEVICE_NAME, PERMS, NULL, &proc_ops);
    if (entry == NULL) {
        return -ENOENT;
    }

    init_list();         /* Create header for linked list of `queue_list` */
    mutex_init(&qlock); 
    printk(KERN_INFO DEVICE_NAME " Module initiation completed.\n");
    return 0;
}


/**
 * @brief Exiting module
 */
static void _module_exit(void) {
    free_list();
    mutex_destroy(&qlock);
    remove_proc_entry(DEVICE_NAME, NULL);
    printk(KERN_INFO DEVICE_NAME " exiting module.\n");
}
