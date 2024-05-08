/* 
 * dictclient.c - A simple multi-threaded dictionary client
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h> 
#include <ctype.h>
#include <pthread.h>

#define BUFSIZE 1024
#define NUMTHREADS 25

void *thread_func_single_word (void *argsp);

/* error - wrapper for perror */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/*
 * This function returns a pointer to a substring of 
 * the original string str with white leading and trailing
 * white space removed.  It is used by find_synonym().
 */
char *trim_whitespace(char *str) {
  char *end;

  if (str == NULL) return NULL;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  // Is string all spaces?
  if(*str == 0) return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator at end of string
  *(end+1) = 0;

  return str;
}

/*
 * Parse synonym from dictionary server.  Store synonym in result.
 */
void find_synonym(char *word, char *result, int fd) {
  char *syn="", *trimsyn="", *save="";
  int n = 0;
  char fromserver[BUFSIZE], buf[BUFSIZE];

  memset(buf, 0, BUFSIZE);
  memset(fromserver, 0, BUFSIZE);

  if(strstr(word, " ") != NULL) error("String cannot contain spaces.");
  
  // Build line that will be sent to server 
  strcat(buf, "define moby-thesaurus ");
  strcat(buf, word);
  strcat(buf, "\n");
  
  // Send line to server 
  while (n += write(fd, buf, strlen(buf)) < strlen(buf)) ;
  if (n < 0) error("ERROR writing to socket");

  // Read the server's reply.  Continue reading until 
  // line beginning with 151 or 552 is found.
  while ((n = read(fd, fromserver, BUFSIZE)) !=0){
    if (n < 0) error("ERROR reading from socket");
    
    // Found a synonym!
    if(strstr(fromserver, "151 ") != NULL) break;
    
    // No synonym found, return input word
    if(strstr(fromserver, "552 ") != NULL) {
      trimsyn = trim_whitespace(word);
      memcpy(result, trimsyn, strlen(trimsyn));
      return;
    }
    memset(fromserver, 0, BUFSIZE);
  }
  
  // Parse result. Find first synonym
  syn = strtok_r(fromserver, ",", &save);
  syn = strtok_r(NULL, ":", &save);
  syn = strtok_r(NULL, ",", &save);
  
  // Remove leading and trailing white space
  trimsyn = trim_whitespace(syn);
  memset(result, 0, BUFSIZE);
  memcpy(result, trimsyn, strlen(trimsyn));

  // Close socket 
  close(fd);    
}

/*
 * Read input phrase and create threads for Part B.
 */
void part_b () {
  int threadid=0, i=0;
  char *res, *next_word, *save;
  char input[BUFSIZE];
  char *words[NUMTHREADS];
  pthread_t tids[NUMTHREADS];
  size_t wordlen;

  // get phrase from stdin 
  printf("Enter a phrase: ");

  // read input from stdin 
  res = fgets(input, sizeof input, stdin);
  if (res == NULL) error ("ERROR reading from stdin");

  // split input up into words (break at white space and line breaks)
  next_word = strtok_r (input," \r\n", &save);

  while (next_word != NULL) {
    wordlen = strlen(next_word);
    
    // malloc space on heap for each word (and later the synonym), 
    // store pointers in words[]
    words[threadid] = (char*) malloc(BUFSIZE);  
    memset(words[threadid], 0, BUFSIZE);
    
    // copy next_word to allocated heap space using memcpy
    memcpy(words[threadid], next_word, wordlen);

    // create thread, pass in address of next_word on heap as argsp 
    pthread_create(&tids[threadid], NULL, thread_func_single_word, words[threadid]);

    // increment threadid
    threadid++;

    // find next word
    next_word = strtok_r (NULL, " \r\n", &save);
  }

  // call join on threads in order to ensure proper ordering of output
  printf("New phrase: ");
  for (i=0; i<threadid; i++) {
    pthread_join(tids[i], NULL);

    // print result 
    printf("%s ", words[i]);

    // free malloced space for word
    free(words[i]);
  }
  printf("\n");
}


/*************Code above this line is provided as helper code*************/

/*
 * Part A. Handle socket setup.  Return socket fd (an int).
 * FILL IN THIS FUNCTION!
 */

int setup_socket() {
  int sockfd, status;
  struct addrinfo hints;
  struct addrinfo *servinfo;  // will point to the getaddrinfo results 
  char *hostname, *portno;

  hostname = "www.dict.org";
  portno = "2628";

  /* set up struct for getaddrinfo */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        /* use IPv4  */
  hints.ai_socktype = SOCK_STREAM;  /* use TCP */
  hints.ai_flags = AI_PASSIVE;      /* fill in my IP addr */

  /* populate servinfo with server name and port */
  status = getaddrinfo(hostname, portno, &hints, &servinfo);
  /* make a socket */
  sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (sockfd < 0) {
    error("ERROR creating socket.");
  }
  /* connect! */
  if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
    error("ERROR connecting.");
  }
  /* freeaddrinfo and return sockfd */
  freeaddrinfo(servinfo);
  return sockfd;
}

/*
 * Part A. Read input from stdin for a single word. 
 * Print output to stdout.
 * FILL IN THIS FUNCTION!
 */

void part_a() {
  int sockfd=0;
  char *res;
  char input[BUFSIZE], result[BUFSIZE];
  
  memset(input, 0, BUFSIZE);
  memset(result, 0, BUFSIZE);
  
  /* Part A */
  printf("Enter a word: ");

  /* read from stdin */
  res = fgets(input, sizeof(input), stdin);
  if (res == NULL) {
    error("ERROR reading from stdin.");
  }
  /* create socket, connect to server, get fd */
  sockfd = setup_socket();
  /* read from socket to find synonym */
  find_synonym(input, result, sockfd);
  printf("Synonym: %s\n", result);
}

/*
 * Part B. Thread code.  Have each thread separately process *individual word*.
 * Pass in input word as argsp.  Return synonym in word buffer as well. 
 * Hint: Much of this code you wrote for Part A in part_a().
 * FILL IN THIS FUNCTION!
 */

void *thread_func_single_word (void *argsp) {
  char *word = "";
  int sockfd = 0;
  char result[BUFSIZE];
  
  memset(result, 0, BUFSIZE);

  /* input word is passed in as argsp */
  word = (char *) argsp;  

  /* create socket, connect to server */
  sockfd = setup_socket();
  /* read from socket to find synonym */
  find_synonym(word, result, sockfd);
  /* copy synonym (aka result) back to word buffer to avoid memory errors */
  memset(word, 0, BUFSIZE);
  memcpy(word, result, strlen(result)); 
  return NULL;
}

int main(int argc, char **argv) {
  /* Part A */
  // comment this part out after you get Part A working
  // NOTE: make sure this is commented out before submitting to autograder!
  //part_a();

  /* Part B */
  // uncomment when you're ready to test thread code 
  part_b();

  return 0;
}

