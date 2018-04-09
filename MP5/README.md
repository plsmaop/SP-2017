# Machine Problem 5 - Server moniter and Daemonlization
* Detailed [SPEC](https://systemprogrammingatntu.github.io/MP5)
* Brief Introduction
  * This is based on [MP2](https://github.com/plsmaop/SP-2017/tree/master/MP2). Now the program cae be daemonlized and can be monitored.
  * use `make` to compile the program under `./src`
  * Usage: 
   * Under `./script`, execute 
      ```
      ./csiebox_server.sh start
      ```
   * Under `./bin`, execute
      ```
      ./csiebox_client ../config/client.cfg
      ./csiebox_client ../config/client2.cfg
      ```
     or more `csiebox_client` if you want.
   * Start the server monitor
   * Under `./web`, execute
     ```
     node app.js
     ```
     (note that you have to install `node.js` first)
     then you will see
     ```
     Express server listening on port: [port]
     ```
     and you can open your browser and go to
     ```
     http://127.0.0.1:[port]
     ```
     to use the GUI monitor.
     The monitor will reveal how many threads of the server program are running.
