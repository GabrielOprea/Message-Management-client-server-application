# Message-Management-client-server-application
TCP and UDP clients subscribe to particular topics to the server, and the server notifies each client when a new entry is added to that topic.
The server keeps the data as an array of topics, where each topic has a singly linked list of clients subscribed to that topic.

The data is persistent. That is, when a subscriber disconnects and reconnects, all the messages he should have received in that period will
be received as soon as he reconnects. To do this, I used an array of backup structures, each one having a list. That is, I kept a structure
for each disconnected client. When receiving a message, instead of sending it directly via TCP, the server caches it the corresponding list
from the array. When the client recconects, I empty his corresponding list and send him all the messages.

The messages sent can be of different types: integers, floats, large numbers, strings. The client receives the message and is able to
parse it accordingly. All the communication is done with I/O multiplexing and socket programming. The server allows many simultaneous
connections, and the clients use commands from stdin to communicate with the server. At the end of the program, all the sockets are closed.
