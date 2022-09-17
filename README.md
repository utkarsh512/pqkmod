# pqkmod

A loadable kernel module for implementing priority queue. It is written for Ubuntu `20.04` with kernel `5.6.9`.

This repository is part of *assignment-1 part-b* of the couse *CS60038 - Advances in Operating System Design*.

## Install, build and run!

* Clone the repository

    ```shell
    $ git clone https://github.com/utkarsh512/pqkmod
    ```

* Build the kernel module and interactive runner

    ```shell
    $ cd pqkmod
    $ make all
    $ gcc interactive_runner.c -o run
    ```

* Load the module in kernel

    ```shell
    $ sudo insmod pqkmod.ko
    ```

* Run the interactive runner (for doing concurrency check, do this on multiple shell window)

    ```shell
    $ ./run
    ```

* For verbose, open a new shell window to view kernel logs as 

    ```shell
    $ cat /dev/kmsg
    ```

## Removing module from kernel

```shell
$ sudo rmmod pqkmod.ko
$ make clean
```
