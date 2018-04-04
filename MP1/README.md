# Machine Problem 1 - Version Control with MD5
* Detailed [SPEC](https://systemprogrammingatntu.github.io/MP1)
* Brief Introduction
  * use `make` to compile the program
  * Usage:
    ```
    ./loser status [dir]
    ```
    will show the difference between the current status of the directory and the last commit of directory.
    ```
    ./loser commit [dir]
    ```
    will commit the directory.
    ```
    ./loser log [number] [dir]
    ```
    will log the last `number` commits of directory.