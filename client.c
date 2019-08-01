#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

int NUM_THREADS = 10000;

struct connection_info {
  int socket;
};

void* process_connection(void * arg) {
  struct connection_info *conn_info = (struct connection_info*)arg;
  char buf[16];
  int sent;

  while (1) {
    struct timespec tp;
    long *bufptr = (long *)buf;

    clock_gettime(CLOCK_REALTIME, &tp);
    bufptr[0] = tp.tv_sec;
    bufptr[1] = tp.tv_nsec;
    sent = send(conn_info->socket, (void *)&buf, sizeof(buf)/sizeof(char), 0);
    printf("%d bytes sent on sock %d\n", sent, conn_info->socket);
    sleep(5);
  }

  return NULL;
}

int main(void) {
/*

// All pointers to socket address structures are often cast to pointers
// to this type before use in various functions and system calls:

struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};


// IPv4 AF_INET sockets:

struct sockaddr_in {
    short            sin_family;   // e.g. AF_INET, AF_INET6
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;     // see struct in_addr, below
    char             sin_zero[8];  // zero this if you want to
};

struct in_addr {
    unsigned long s_addr;          // load with inet_pton()
};

*/
// looks like everything in the network is either short ints or long ints with long being 32 bit.

  struct sockaddr_in address, srv_address;
  socklen_t in_len;
  int reuse = 1, i, sock, status;
  pthread_t thread[NUM_THREADS];
  struct connection_info conn_info[NUM_THREADS];

  address.sin_family = AF_INET;
  address.sin_port = htons(1122);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  srv_address.sin_family = AF_INET;
  srv_address.sin_port = htons(1111);
  srv_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  for (i = 0; i < NUM_THREADS; i++) {
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    printf("new client socket - %d\n", sock);
    //	status = bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr));
        //if (status < 0)
    //  perror("status returned from bind is");
    status = connect(sock, (struct sockaddr *)&srv_address, sizeof(struct sockaddr));
    if (status < 0) {
      perror("status returned from connect is");
      continue;
    }
    conn_info[i].socket = sock;
    pthread_create(&thread[i], NULL, process_connection, &conn_info[i]);
  }

  for (i = 0; i < NUM_THREADS; i++) {
    void *arg;
    pthread_join(thread[i], &arg);
  }

  return 0;
}

