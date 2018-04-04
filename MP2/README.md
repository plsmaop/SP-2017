# Machine Problem 2 - File Sychronization and Monitoring
* Detailed [SPEC](https://systemprogrammingatntu.github.io/MP2)
* Brief Introduction
  * This is a program to sychronize files between two directories and monitor the change of them.
  * use `make` to compile the program under `./src`
  * Usage: 
    1. Create directories `./cdir` and `./sdir`
    2. Under `./bin`, execute 
      ```
      ./csiebox_server ../config/server.cfg
      ```
      ```
      ./csiebox_client ../config/client.cfg
      ```
      
    3. If you change the file under `./cdir`, the program will detect the change and sychronize under `./sdir`
