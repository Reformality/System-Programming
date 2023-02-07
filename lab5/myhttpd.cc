
const char * usage =
"                                                               \n"
"myhttpd:                                                       \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   myhttpd <port>                                              \n"
"                                                               \n"
"Where 1024 < port < 65536.                                     \n"
"                                                               \n"
"In another window type:                                        \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where myhttpd          \n"
"is running. <port> is the port number you used when you run    \n"
"myhttpd.                                                       \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and    \n"
"the time of the day.                                           \n"
"                                                               \n";


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <string> //cpp string
#include <signal.h> 
#include <pthread.h>
#include <iostream>
#include <fcntl.h>
#include <dlfcn.h>
#include <link.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>

#include <algorithm>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
void processDirRead(int fd, std::string path, std::string var);

typedef void (*httprunfunc)(int ssock, const char* querystring);

typedef struct ServiceTime {
  double minTime;
  double maxTime;
  char minTimeURL[1000];
  char maxTimeURL[1000];
} ServiceTime;

pthread_mutex_t mutex;
int QueueLength = 5;
ServiceTime * stime;

//stats global variables
time_t timeStart; //server starting time
int request = 0;
int port = 7894; //set default port

//log
std::string logStr("");

// Processes time request
void processHTTPRequest( int socket );
void processHTTPRequestThread( int socket );
void * loopthread(int masterSocket);
void processCGI(char* path, int fd);
void processStat(int fd);
void processLog(int fd);

void zombieHandler(int sig) {
  if (sig == SIGCHLD) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
  }
}

void processHTTPRequestThread( int socket ) {
  processHTTPRequest(socket);
  close(socket);
}

int
main( int argc, char ** argv )
{
  //stats variables
  time(&timeStart);
  stime = (ServiceTime *) mmap(NULL, sizeof(ServiceTime), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

  
  // Print usage if not enough arguments
  if ( argc < 2 || argc > 3) {
    fprintf( stderr, "%s", usage );
    exit( -1 );
  }

  // int port = 7894; //set default port
  char flag = 'i';  //default to iterative mode
  
  if(argc == 2){
    if(strcmp(argv[1],"-f") == 0){
      flag = 'f'; //Process mode, no port
    } else if(strcmp(argv[1],"-t") == 0){
      flag = 't'; //Thread per request, no port
    } else if(strcmp(argv[1],"-p") == 0){
      flag = 'p'; //Pool of Threads, no port
    } else {  //'i' mode with port
      port = atoi( argv[1] );
    }
  } else if(argc == 3){
    if(strcmp(argv[1],"-f") == 0){
      flag = 'f'; //Process mode, no port
    } else if(strcmp(argv[1],"-t") == 0){
      flag = 't'; //Thread per request, no port
    } else if(strcmp(argv[1],"-p") == 0){
      flag = 'p'; //Pool of Threads, no port
    }
    port = atoi( argv[2] );
  }
  
  printf("\n\nmode: %c\nport: %d \n",flag,port);
  
  
  
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; //ip address of the port
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) ); //init to 0
  serverIPAddress.sin_family = AF_INET; //intenet family
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port); //chose a port
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }

  while(flag == 'i') {
    printf("server in Iterative mode\n\n");
    // Accept incoming connections
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);

    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }

    // Process request.
    request++;
    processHTTPRequest( slaveSocket );

    // Close socket
    close( slaveSocket );
  }

  while(flag == 'f') {
    printf("server in Process mode\n\n");
    request++;
    // Accept incoming connections
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }

    struct sigaction sa;
    sa.sa_handler = zombieHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    int ret = fork();
    if (ret == 0) {
      processHTTPRequest( slaveSocket );
      close(slaveSocket);
      exit(0);
    } else {
      if(sigaction(SIGCHLD, &sa, NULL)){
        perror("sigaction");
        exit(2);
      }
      close(slaveSocket);
    }
  }

  while(flag == 't') {
    printf("server in Thread per request mode\n\n");
    request++;
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }
    pthread_t t1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t1, &attr, (void * (*) (void *)) processHTTPRequestThread,
                (void *) slaveSocket);

  }

  if(flag == 'p') {
    request++;
    // Accept incoming connections
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);

    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }

    pthread_t thread[QueueLength];
    pthread_mutex_init(&mutex, NULL);
    for(int i=0; i<QueueLength; i++){
      // request++;
      pthread_create(&thread[i], NULL, (void * (*) (void *)) loopthread, (void *) masterSocket);
    }
    pthread_join(thread[0], NULL);

  }
}

void
processHTTPRequest( int fd )
{
  clock_t startClock = clock();
  
  // Buffer used to store the name received from the client
  const int MaxReq = 4096;
  char req[ MaxReq + 1 ];
  int reqLength = 0;
  int n;

  // Currently character read
  unsigned char newChar;
    
  while ( reqLength < MaxReq &&
	  ( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {
    
    req[ reqLength ] = newChar;
    reqLength++;

    if (reqLength > 4 && 
        req[reqLength-4]=='\r' && req[reqLength-3]=='\n' && 
        req[reqLength-2]=='\r' && req[reqLength-1]=='\n') {
          break;
    }

  } //end while (end reading)

  // Add null character at the end of the string
  req[ reqLength ] = 0;

  // parse request root
  const char * path_start = strchr(req, ' ') + 1;
  const char * path_end = strchr(path_start, ' ');
  char path[path_end - path_start];
  strncpy(path, path_start, path_end - path_start); //put into path
  path[sizeof(path)] = 0; //null terminator
  // printf("\nroot = %s\n\n\n", path);

  // modify path
  std::string newPath("./http-root-dir/");
  if (strcmp(path, "/")==0) {
    newPath += "/htdocs/index.html";
    logStr.append("<dt>source host: data.cs.purdue.edu:");
    logStr.append(std::to_string(port));
    logStr.append(", path: ");
    logStr.append("/index.html");
    logStr.append("</dt>\n");
  } else if (strcmp(path, "/favicon.ico")==0) {
    return;
  } else if (strstr(path,"cgi-bin")!=0){  //find cgi-bin
    logStr.append("<dt>source host: data.cs.purdue.edu:");
    logStr.append(std::to_string(port));
    logStr.append(", path: ");
    logStr.append(path);
    logStr.append("</dt>\n");
    processCGI(path, fd);
    // printf("\n\n\n!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");
    return;
  } else if (strcmp(path, "/stats")==0) {
    processStat(fd);
    return;
  } else if (strcmp(path, "/logs")==0) {
    processLog(fd);
    return;
  } 
  else if (strcmp(path, "/dir1/")==0) {
    newPath += "htdocs";
    newPath += path;
    processDirRead(fd, newPath, "");
    return;
  }
  else if (strcmp(path, "/dir1")==0||strcmp(path, "/dir1/subdir1")==0) {
    newPath += "htdocs";
    newPath += path;
    newPath += "/";
    processDirRead(fd, newPath, "");
    return;
  }
  else if (strstr(path,"/dir1/?")!=0) {
    std::string pathCpp(path);
    std::string var("");
    var.append(pathCpp.substr(pathCpp.find("?")+1));
    newPath += "htdocs";
    newPath += pathCpp.substr(0,pathCpp.find("?"));
    processDirRead(fd, newPath, var);
    return;
  }
  else if (strstr(path, "/dir1/subdir1?")!=0) {
    std::string pathCpp(path);
    std::string var("");
    var.append(pathCpp.substr(pathCpp.find("?")+1));
    newPath += "htdocs";
    newPath += pathCpp.substr(0,pathCpp.find("?"));
    newPath += "/";
    processDirRead(fd, newPath, var);
    return;
  }
  else {
    std::string addedStr = path;
    newPath += "htdocs";
    newPath += addedStr;
    logStr.append("<dt>source host: data.cs.purdue.edu:");
    logStr.append(std::to_string(port));
    logStr.append(", path: ");
    logStr.append(newPath);
    logStr.append("</dt>\n");
  }

  // set the file type
  std::string contentType("text/html"); 
  if (newPath.find(".png") != std::string::npos) {
    contentType.replace(contentType.begin(),contentType.end(),"image/png");
  } else if (newPath.find(".jpg") != std::string::npos) {
    contentType.replace(contentType.begin(),contentType.end(),"image/jpg");
  } else if (newPath.find(".gif") != std::string::npos) {
    contentType.replace(contentType.begin(),contentType.end(),"image/gif");
  } else if (newPath.find(".svg") != std::string::npos) {
    contentType.replace(contentType.begin(),contentType.end(),"image/svg+xml");
  }
  
  printf( "req=%s\n", req );  //print the GET

  // Basic HTTP Authentication
  std::string req_cpp = req;
  std::string user_pw("Authorization: Basic d3UxMzI2OjEyMzQ1\r\n"); //d3UxMzI2OjEyMzQ1 (wu1326:12345)
  if (req_cpp.find(user_pw) == std::string::npos) {
    const char * err1 = "HTTP/1.1 401 Unauthorized\r\n";
    const char * err2 = "WWW-Authenticate: Basic realm=\"myhttpd-cs252-wu1326\"\r\n";
    const char * err3 = "\r\n\r\n";
    write(fd, err1, strlen(err1));
    write(fd, err2, strlen(err2));
    write(fd, err3, strlen(err3));
    // close socket
    close(fd);
    printf("\n\nIncorrect username or password!\n\n");
    return;
  }

  // send reply
  const char * hdr1 = "HTTP/1.1 200 Document follows\r\n"
                      "Server: CS252 Lab5\r\n";
  const char * hdr2 = "Content-type: ";
  // const char * contentType = "text/html"; 
  const char * hdr3 = "\r\n\r\n";
  write (fd, hdr1, strlen(hdr1));
  write (fd, hdr2, strlen(hdr2));
  // write (fd, contentType, strlen(contentType));
  write (fd, contentType.c_str(), strlen(contentType.c_str()));
  write (fd, hdr3, strlen(hdr3));
  
  //testing
  printf("\nrequest:\n");
  printf("%s",hdr1);
  printf("%s",hdr2);
  printf("%s",contentType.c_str());
  printf("%s",hdr3);
  printf("^^^^^^^^^^^^^^^^\n");

  // write document
  FILE * ptr;
  int fileLength = 0;
  char ch;
  ptr = fopen(newPath.c_str(), "r");
  while ((fileLength = read(fileno(ptr), &ch, sizeof(ch))) != 0) {
    int writeLength = write(fd, &ch, sizeof(ch));
    if (writeLength != fileLength) {
      perror("write");
      return;
    }
  }
  fclose(ptr);

  clock_t endClock = clock();
  double cpu_time_used = ((double) (endClock - startClock)) / CLOCKS_PER_SEC;
  if (cpu_time_used < stime->minTime || stime->minTimeURL[0] == '\0') {
    stime->minTime = cpu_time_used;
    sprintf(stime->minTimeURL, "http://data.cs.purdue.edu:%d%s",port,path);
  }
  if (cpu_time_used > stime->maxTime || stime->maxTimeURL[0] == '\0') {
    stime->maxTime = cpu_time_used;
    sprintf(stime->maxTimeURL, "http://data.cs.purdue.edu:%d%s",port,path);
  }
  
  // close socket
  // close(fd);
  
}

void * loopthread(int masterSocket) {
  while(1){
    printf("server in Pool of thread mode\n\n");
    // Accept incoming connections
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    pthread_mutex_lock(&mutex);
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    pthread_mutex_unlock(&mutex);

    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }

    // Process request.
    processHTTPRequest( slaveSocket );

    // Close socket
    close( slaveSocket );
  }
}

void processCGI(char* path, int fd){
  std::string pathCpp(path);
  std::string newPath("./http-root-dir");
  std::string var("");
  int hasVar = 0;
  if (pathCpp.find("?") != std::string::npos){  //can find ?
    //get variable after '?'
    var.append(pathCpp.substr(pathCpp.find("?")+1));
    //get path before '?'
    newPath += pathCpp.substr(0,pathCpp.find("?"));
    hasVar = 1;
    std::cout << '\n' << newPath << "?" << var << '\n'; //test
  } else {  //cannot find ?
    newPath += pathCpp;
    std::cout <<'\n' << newPath << "no var" << '\n';  //test
  }

  //open check
  int testOpen = open(newPath.c_str(), O_RDONLY);

  if(testOpen > 0) {
    //fork
    int pid = fork();
    if (pid == 0){
      setenv("REQUEST_METHOD", "GET", 1);
      if(hasVar) {
        setenv("QUERY_STRING", var.c_str(), 1);
      }
      //write http reply
      const char * hdr1 = "HTTP/1.0 200 Document follows\r\n";
      const char * hdr2 = "Server: CS252 Lab5\r\n";
      write(fd, hdr1, strlen(hdr1));
      write(fd, hdr2, strlen(hdr2));

      if (newPath.find(".so") == std::string::npos) {  //not .so file
        dup2(fd, 1);
        dup2(fd, 2);
        execvp(newPath.c_str(), NULL);
      } else {  //.so file
        void * lib = dlopen( newPath.c_str(), RTLD_LAZY );
        if ( lib == NULL ) {
          fprintf( stderr, "./hello.so not found\n");
          perror( "dlopen");
          exit(1);
        }

        httprunfunc hello_httprun;
        hello_httprun = (httprunfunc) dlsym( lib, "httprun");
        if ( hello_httprun == NULL ) {
          perror( "dlsym: httprun not found:");
          exit(1);
        }

        hello_httprun(fd, var.c_str());

        if (dlclose(lib) != 0) {
          perror("dlclose");
          exit(1);
        }

      }
    }
    //wait for child
    // waitpid(pid, NULL, 0);
    // exit(0);
  }
}

void processStat(int fd) {
  //write http reply
  const char * hdr1 = "HTTP/1.0 200 Document follows\r\n";
  const char * hdr2 = "Server: CS252 Lab5\r\n";
  const char * hdr3 = "Content-Type: text/html\r\n";
  write(fd, hdr1, strlen(hdr1));
  write(fd, hdr2, strlen(hdr2));
  write(fd, hdr3, strlen(hdr3));
  write(fd, "\r\n", strlen("\r\n"));

  char * htmlFile = (char *) malloc(sizeof(char)*1000);

  time_t timeNow;
  time(&timeNow); //get the current time

  sprintf(htmlFile,
    "<html><body> \
    <h1>CS 252 Lab Server Status</h1> \
    <dl> \
    <dt>Author: Zekun Wu</dt> \
    <dt>Uptime: %d seconds</dt> \
    <dt>Requests: %d</dt> \
    <dt>Minimum Service Time: %f seconds</dt> \
    <dt>Maximum Service Time: %f seconds</dt> \
    <dt>Minimum Service URL: %s</dt> \
    <dt>Maximum Service URL: %s</dt> \
    </dl> \
    </body></html>",(int) difftime(timeNow, timeStart), request, stime->minTime, stime->maxTime, stime->minTimeURL, stime->maxTimeURL);

  write(fd, htmlFile, strlen(htmlFile));
  free(htmlFile);
  htmlFile = NULL;
  write(fd, "\r\n\r\n",strlen("\r\n\r\n"));
}

void processLog(int fd) {
  //write http reply
  const char * hdr1 = "HTTP/1.0 200 Document follows\r\n";
  const char * hdr2 = "Server: CS252 Lab5\r\n";
  const char * hdr3 = "Content-Type: text/html\r\n";
  write(fd, hdr1, strlen(hdr1));
  write(fd, hdr2, strlen(hdr2));
  write(fd, hdr3, strlen(hdr3));
  write(fd, "\r\n", strlen("\r\n"));

  // char * htmlFile = (char *) malloc(sizeof(char)*1000);
  std::string sendStr("<html><body> \
    <h1>CS 252 Lab Server Request Log</h1> \
    <dl> \
    <dt>Author: Zekun Wu</dt>");
  sendStr.append(logStr);
  sendStr.append("</dl> \
    </body></html>");
  

  // sprintf(htmlFile, "%s",sendStr.c_str);

  write(fd, sendStr.c_str(), strlen(sendStr.c_str()));
  // free(htmlFile);
  // htmlFile = NULL;
  write(fd, "\r\n\r\n",strlen("\r\n\r\n"));
}



//dir


// void insertToHtmlHeader(std::string &&str) {
//     int idx = html.find("$1");
//     html.erase(idx, 2);
//     html.insert(idx, str);
//     idx = html.find("$1");
//     html.erase(idx, 2);
//     html.insert(idx, str);
// }

// void insertToHtml(std::string &li) {
//     int idx = html.find("$2");
//     html.insert(idx, li);
// }

// void erase$2() {
//     html.erase(html.find("$2"), 2);
// }

std::string getTime(struct timespec &time) {
    char buf[100];
    strftime(buf, sizeof buf, "%D %T", gmtime(&time.tv_sec));
    return std::string(buf);
}

std::string getSize(long size) {
    char buf[100];
    sprintf(buf, "%ld", size);
    return std::string(buf);
}

struct myFile {
    std::string name;
    std::string time;
    std::string size;
};

std::string sortType = "C=S;O=A";

bool myComparator(const struct myFile &lhs, const struct myFile &rhs) {
    if (sortType == "C=N;O=D") {
        return lhs.name < rhs.name;
    } 
    else if (sortType == "C=N;O=A") {
       return lhs.name > rhs.name;
    } 
    else if (sortType == "C=M;O=D") {
        return lhs.time < rhs.time;
    }
    else if (sortType == "C=M;O=A") {
        return lhs.time > rhs.time;
    }
    else if (sortType == "C=S;O=D") {
        return lhs.size < rhs.size;
    }
    else if (sortType == "C=S;O=A") {
        return lhs.size > rhs.size;
    }
}

void processDirRead(int fd, std::string path, std::string var) {
    std::string html = "<html>\n"
                   "  <head>\n"
                   "    <title>$1</title>\n"
                   "  </head>\n"
                   "\n"
                   "  <body>\n"
                   "    <h1>$1</h1>\n"
                   "\n"
                   "    <li><A HREF=\"?C=N;O=A\"> Name:A</A></li>\n"
                   "    <li><A HREF=\"?C=N;O=D\"> Name:D</A></li>\n"
                   "    <li><A HREF=\"?C=M;O=A\"> Last Modified Date:A</A></li>\n"
                   "    <li><A HREF=\"?C=M;O=D\"> Last Modified Date:D</A></li>\n"
                   "    <li><A HREF=\"?C=S;O=A\"> Size:A</A></li>\n"
                   "    <li><A HREF=\"?C=S;O=D\"> Size:D</A></li>\n"
                   "\n"
                   "    <ul>\n"
                   "    $2"
                   "    </ul>\n"
                   "\n"
                   "    <hr>\n"
                   "  </body>\n"
                   "</html>";
    char abs_path[PATH_MAX];
    char *ptr = realpath(path.c_str(), abs_path);
    // insertToHtmlHeader();
    sortType = var; //added

    int idx = html.find("$1");
    html.erase(idx, 2);
    html.insert(idx, "Index of " + std::string(ptr));
    idx = html.find("$1");
    html.erase(idx, 2);
    html.insert(idx, "Index of " + std::string(ptr));


    std::string liTemplate = "<li><img src='/index.gif'><A HREF=\"$1\"> $2</A> $3; $4</li>\n";

    auto myFiles = std::vector<struct myFile>();
    int counter = 0;

    struct dirent *ent;
    DIR *dir = opendir(path.c_str());
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_DIR || ent->d_type == DT_REG) {

            struct stat file_stat;
            auto file = path + ent->d_name;
            stat(file.c_str(), &file_stat);

            struct myFile newFile;
            myFiles.push_back(newFile);
            std::cout << myFiles[counter].name << std::endl;
            myFiles[counter].name = std::string(ent->d_name);
            myFiles[counter].time = getTime(file_stat.st_mtim);
            myFiles[counter++].size = getSize(file_stat.st_size);

        } else if (ent->d_type == DT_REG) {

        } else {
            // more
        }
    }

    std::sort(myFiles.begin(), myFiles.end(), &myComparator);

    while (counter > 0) {
        std::string liTemplateCopy = std::string(liTemplate);

        counter--;
        std::cout << myFiles[counter].name << std::endl;

        int idx = liTemplateCopy.find("$1");
        liTemplateCopy.erase(idx, 2);
        liTemplateCopy.insert(idx, myFiles[counter].name);

        idx = liTemplateCopy.find("$2");
        liTemplateCopy.erase(idx, 2);
        liTemplateCopy.insert(idx, myFiles[counter].name);

        idx = liTemplateCopy.find("$3");
        liTemplateCopy.erase(idx, 2);
        liTemplateCopy.insert(idx, myFiles[counter].time);

        idx = liTemplateCopy.find("$4");
        liTemplateCopy.erase(idx, 2);
        liTemplateCopy.insert(idx, myFiles[counter].size);

        // insertToHtml(liTemplateCopy);
        int idx2 = html.find("$2");
        html.insert(idx2, liTemplateCopy);
    }

    // erase$2();
    html.erase(html.find("$2"), 2);

    const char * hdr1 = "HTTP/1.0 200 Document follows\r\n";
    const char * hdr2 = "Server: CS252 Lab5\r\n";
    const char * hdr3 = "Content-Type: text/html\r\n";
    write(fd, hdr1, strlen(hdr1));
    write(fd, hdr2, strlen(hdr2));
    write(fd, hdr3, strlen(hdr3));
    write(fd, "\r\n", strlen("\r\n"));

    const char * contents = html.c_str();
    write(fd, contents, strlen(contents));
    write(fd, "\r\n\r\n",strlen("\r\n\r\n"));
}