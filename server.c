#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <poll.h>

int NUM_THREADS = 10000;

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

struct connection_info {
  int socket;
};

void read_bytes_from_socket(int socket) {
  char buf[16];
  int recvd;
  long *bufptr = (long *)buf;
  struct timespec tp;
  float latency;

  recvd = recv(socket, (void *)&buf, sizeof(buf)/sizeof(char), 0);
//    printf("%d bytes received on sock %d\n", recvd, socket);
  clock_gettime(CLOCK_REALTIME, &tp);
  latency = ((float)tp.tv_nsec-bufptr[1])/(float)1000000;
  if (tp.tv_sec-bufptr[0] <= 0) {
    if (latency > 10.0f) {
      printf("latency is %lf ms\n", ((float)tp.tv_nsec-bufptr[1])/(float)1000000);
    }
  } else {
    printf("latency too high - %ld s\n", tp.tv_sec - bufptr[0]);
  }
}
void* process_connection(void * arg) {
  struct connection_info *conn_info = (struct connection_info*)arg;

//  fcntl(conn_info->socket, F_SETFL, O_NONBLOCK);
  while (1) {
    read_bytes_from_socket(conn_info->socket);
  }

  return NULL;
}

void do_select_loop(struct connection_info *conn_info) {
  while(1) {
    fd_set readfds;
    int max_socket = -1, status, i;

    FD_ZERO(&readfds);
    for (i = 0; i < NUM_THREADS; i++) {
      if (conn_info[i].socket > max_socket) {
        max_socket = conn_info[i].socket;
      }
      FD_SET(conn_info[i].socket, &readfds);
    }
    max_socket++;

    status = select(max_socket, &readfds, NULL, NULL, NULL);
    if (status < 0) {
      printf("max_socket is - %d\n", max_socket);
      perror("select failed:");
      continue;
    }

    for (i = 0; i < max_socket; i++) {
      if (FD_ISSET(i, &readfds)) {
//          printf("Data present for socket %d\n", i);
        read_bytes_from_socket(i);
      }
    }
  }
}

void do_poll_loop(struct connection_info *conn_info) {
  while(1) {
    struct pollfd poll_fds[NUM_THREADS];
    int i;

    /* initialize the pollfds */
    for (i = 0; i < NUM_THREADS; i++) {
      poll_fds[i].fd = i;
      poll_fds[i].events = POLLIN;
      poll_fds[i].revents = 0;
    }

    poll(poll_fds, NUM_THREADS, -1);

    for (i = 0; i < NUM_THREADS; i++) {
      if (poll_fds[i].revents & POLLIN) {
//            printf("Data present for socket %d\n", i);
        read_bytes_from_socket(i);
      }
    }
  }
}

void do_epoll_loop(struct connection_info *conn_info) {

/*
typedef union epoll_data {
  void        *ptr;
  int          fd;
  uint32_t     u32;
  uint64_t     u64;
} epoll_data_t;

struct epoll_event {
  uint32_t     events;       //Epoll events
  epoll_data_t data;        //User data variable
};
*/
  int epoll_fd = epoll_create(1); // any number greater than 0 is fine.
  struct epoll_event epoll_events[NUM_THREADS];
  int i;

  for (i = 0; i < NUM_THREADS; i++) {
    epoll_events[i].events = EPOLLIN;
    epoll_events[i].data.fd = conn_info[i].socket;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_info[i].socket, &epoll_events[i]);
  }

  while(1) {
    int num_events;
    struct epoll_event returned_epoll_events[NUM_THREADS];

    num_events = epoll_wait(epoll_fd, &returned_epoll_events[0], NUM_THREADS, -1);

    for (i = 0; i < num_events; i++) {
//          printf("reading from socket %d\n", returned_epoll_events[i].data.fd);
      read_bytes_from_socket(returned_epoll_events[i].data.fd);
    }
  }
}

int main(void) {
// looks like everything in the network is either short ints or long ints with long being 32 bit.
  struct sockaddr_in address;
  struct sockaddr_in in;
  socklen_t in_len;
  int sock, newsock, status, i, do_sync = 0; // do_sync implies synchronous threading model.
  pthread_t thread[NUM_THREADS];
  struct connection_info conn_info[NUM_THREADS];

  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  printf("hello world - %d\n", sock);

  address.sin_family = AF_INET;
  address.sin_port = htons(1111);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 

  status = bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr));
  if (status < 0)
    perror("status returned from bind");
  
  status = listen(sock, 0);
  if (status < 0)
    perror("status returned from listen");

  for (i = 0; i < NUM_THREADS; i++) {
    newsock = accept(sock, (struct sockaddr *)&in, &in_len);
    if (newsock < 0) {
      perror("status returned from accept");
      continue;
    }

//    printf("socket returned from accept is %d\n", newsock);

    conn_info[i].socket = newsock;
    if (do_sync) {
      pthread_create(&thread[i], NULL, process_connection, &conn_info[i]);
    } else {
//       fcntl(newsock, F_SETFL, O_NONBLOCK);
    }
  }

  if (!do_sync) {
    int do_select = 0, do_poll = 0, do_epoll = 1;

    if (do_select) {
      do_select_loop(&conn_info[0]);
    } else if (do_poll) {
      do_poll_loop(&conn_info[0]);
    } else if (do_epoll) {
      do_epoll_loop(&conn_info[0]);
    }
  }

  if (do_sync) {
    for (i = 0; i < 1000; i++) {
      void *arg;
      pthread_join(thread[i], &arg);
    }
  }

  return 0;
}
