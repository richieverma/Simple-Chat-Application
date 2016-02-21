//Functions specific to server and client
int server(char *port);
int client(char *port);


//Struct to store messages in order received
struct messages {
  char msg[256];
  char sender[16];
  struct messages *next;
};

//Struct for storing the list and stats and pending messages
struct Client {
  char host[30], ip[16];
  int msgs_sent, msgs_recv, online, csocket, port;
  struct messages *mlist;
  struct Client *next, *blocked;
};
