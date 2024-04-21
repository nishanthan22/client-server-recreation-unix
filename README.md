# client-server-recreation-unix
This is an Unix application, in which I created a server that could receive connections from n clients and, in the event that a threshold was met, would also divert client connections to the mirror servers, thus illustrating the general idea of load balancing.

#Follow these steps to make this application work in your system
- First clone the repo in your local
- And, compile all the four files using the gcc compiler
  "gcc -o <runnable> <file_name.c>"
- After compiling succcessfully, run the files in the following order
  1. serverw24
  2. mirror1
  3. mirror2
  4. clienw24 localhost (localhost should be passed as an argument to the clientw24 program)
- Once all the files have been started, the servers will pop the message that it has been initialised successfully and ready to accept client requests.
- Finally, you can request the following commands from the server
  1. dirlist -a [ex: clientw24$ dirlist -a]
  2. dirlist -t [ex: clientw24$ dirlist -t]
  3. w24fn <filename> [ex: client24$ w24fn sample.txt]
  4. w24fz size1 size2 [ex: client24$ w24fz 1240 12450]
  5. w24ft <extension_list> //up to 3 different file types [ex: client24$ w24ft c txt]
  6. w24fdb date [ex: client24$ w24fdb 2023-01-01]
  7. w24fda date [ex: client24$ w24fda 2023-03-31]
