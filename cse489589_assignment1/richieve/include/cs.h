//Functions specific to server and client
int server(char *port);
int client(char *port);


//Struct to store messages in order received
struct messages {
  char msg[256];
  struct messages *next;
};

//Struct to store messages in order received
struct bClient {
  char ip[16], host[30];
  int port;
  struct bClient *next;
};

//Struct for storing the list and stats and pending messages
struct Client {
  char host[30], ip[16];
  int msgs_sent, msgs_recv, online, csocket, port;
  struct messages *mlist;
  struct Client *next;
  struct bClient *blocked;
};
