# Machine Problem 3 - MD5 Miner 
* Detailed [SPEC](https://systemprogrammingatntu.github.io/MP3)
* Brief Introduction
  * This is a program to assign tasks to other programs to find the specific MD5 strings.
  * use `make` to compile the program under `./server` and `./client` respectively.
  * Usage: 
    * Create directories `./cdir` and `./sdir`
    * Under `./server`, execute 
      ```
      ./boss [config file]
      ```
      where the config file lists out the information for `boss` to assign tasks.<br>
    * Under `./client`, execute several
      ```
      ./miner [name] [fifo_in] [fifo_out]
      ```
      according to the `[config file]`.<br>
    * The format of `[config file]`:
      ```
      MINE: /tmp/mine.bin\n
      MINER: /tmp/ada_in /tmp/ada_out\n
      MINER: /tmp/margaret_in /tmp/margaret_out\n
      MINER: /tmp/hopper_in /tmp/hopper_out\n
      ```
      where `MINE` is the file `boss` output the result.<br>
    * Server Commands Line
      * The server should support commands from `stdin`, the `status`, and `dump`, and `quit`.
        * `status` will show the current highest-scored MD5 string
        * `dump` will output the original string corresponding the highest-scored MD5 string to the `MINE PATH` in `[config file]`
        * `quit` will terminate the `boss`
    
