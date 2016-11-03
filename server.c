#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>


#define MAXSIZE 20
#define BUFFSIZE 255

pthread_rwlock_t lock=PTHREAD_RWLOCK_INITIALIZER;
/* ---------------------------------------------------------------------
 This	is  a sample server which opens a stream socket and then awaits
 requests coming from client processes. In response for a request, the
 server sends an integer number  such	 that  different  processes  get
 distinct numbers. The server and the clients may run on different ma-
 chines.
 --------------------------------------------------------------------- */
typedef struct users{
	int fd;
	uint16_t length;
	unsigned char usernamestr [MAXSIZE];
} user;

void * handShake(void*  u);
uint16_t lengthOfUsername(unsigned char userName[MAXSIZE]);
void sendCurrentUserNames(user * targetUser);
void getUsername(void* u);
void recievedBytes(user * currentUser, unsigned char* buff, uint16_t numBytes);
//void * recieveMessage(void* socket);
void removeUser(user* u);
void addUser(user* u);
void chat(user* u);
void sendChatMessage(char* p,  uint16_t size, user * sender);
void sendJoinMessage(char* p, uint16_t size);
void sendExitMessage(char* p, uint16_t size);
void sendBytes(user * currentUser, unsigned char* buff, uint16_t numBytes);
void sendJoin(user * currentUser);
void sendExit(user * currentUser);
void writeToLogJoin(user *u);
void writeToLogExit(user *u);
void exitAll();

struct sigaction priorSigHandler;
struct sigaction currentSigHandler;

int numberofclients;
int fd;
pid_t forkstatus;
pid_t sid;
user * listofusers[MAXSIZE];
FILE *fp; 
// volatile user* sharedMem=listofusers;

void sigHandler(int sig){
	exitAll();
	sigaction(SIGTERM, &priorSigHandler, 0);
	fclose(fp);
	exit(1);
}
int main(int argc, char *argv[]){
	if(argc!=2){
		printf("Invalid arguments: this server requires one input arguement as the port number\n");
		exit(1);
	}
	uint16_t portnumber =atoi(argv[1]);
	// printf("this is the portnumber%d\n",portnumber );
	// exit(1);
	sid = 0;
	char cwd[1024];
   	if (getcwd(cwd, sizeof(cwd)) != NULL)
   	{
   		printf("Starting server with log file in:\n");
       fprintf(stdout, "Current working dir: %s\n", cwd);
       printf("\n");
   	}
   	else
   	{
       perror("getcwd() error");
   	}
  

	//daemonizing
	forkstatus=fork();
	if(forkstatus!=0){
		
		exit(1);
	}

	pid_t myPid=getpid();
	
	char filename[BUFFSIZE];
	char temp[20];
	sprintf(temp,"%d",myPid);
	strcpy(filename,"server379");
	strcat(filename,temp);
	strcat(filename,".log");
	
	

	//create new process group -- don't want to look like an orphan
    sid = setsid();
    if(sid < 0)
    {
    	printf("cannot create new process group\n");
        exit(1);
    }


    if ((chdir(cwd)) < 0) {
      
      printf("Could not change working directory to /\n");
      exit(1);
    }

	
 	fp = fopen(filename, "w+");
 	if(fp == NULL){
 		printf("Failed to open a log file.\n");
 		exit(1);		
 	}

 	//Intialize sig handlers
	currentSigHandler.sa_handler = sigHandler;
	sigemptyset(&currentSigHandler.sa_mask);
	currentSigHandler.sa_flags = 0;
 	sigaction(SIGTERM, &currentSigHandler, &priorSigHandler);

	// close standard fds
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);	

	numberofclients=0;
	
	fd_set fd_active;
	fd_set read_fd_set;
	struct timeval timer;
	timer.tv_sec=15;
	timer.tv_usec=0;
	forkstatus =0;

	int	sock, snew, fromlength, number, outnum;
	uint16_t inlength,usernameLength;
	uint32_t inusername;
	struct	sockaddr_in	master, from;
	
	
	sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror ("Server: cannot open master socket");
		exit (1);
	}

	
	master.sin_family = AF_INET;
	master.sin_addr.s_addr = INADDR_ANY;
	master.sin_port = htons (portnumber);

	if (bind (sock, (struct sockaddr*) &master, sizeof (master))) {
		perror ("Server: cannot bind master socket");
		exit (1);
	}

	listen (sock, 5);
	FD_ZERO(&fd_active);
	FD_SET(sock,&fd_active);
	read_fd_set=fd_active;
	pthread_t thread;

	for(;;){


		if(select(FD_SETSIZE,&read_fd_set,NULL,NULL,NULL)<0){
			perror("select()");
			exit(1);
		}
		
		int i=0;
		for(i;i<FD_SETSIZE;i++){			
			if(FD_ISSET(i, &read_fd_set)){
				if(i == sock){
					fromlength = sizeof (from);
					
					snew = accept (sock, (struct sockaddr*) & from, & fromlength);

					if(setsockopt(snew,SOL_SOCKET,SO_RCVTIMEO, (char *)&timer, sizeof(timer))<0){
						fprintf(fp, "%s\n",strerror(errno) );
					}
					if (snew < 0) 
					{
						fprintf(fp, "%s\n",strerror(errno) );
						continue;
					}
					
					user* newUser = (user *)malloc(sizeof(user));
					memset(newUser->usernamestr,'\0',MAXSIZE);
					newUser->fd = snew;
					newUser->length = 0;
					
					//Create a listener thread
					int ret = pthread_create(&thread, NULL, &handShake, (void*)newUser);
				}
				
				else
				{
					FD_CLR(i,&fd_active);
				}
			}

		}	
	
	}
}

void addUser(user* u){
	
	pthread_rwlock_wrlock(&lock);

	int i;
	for(i = 0; i< MAXSIZE ; i++){
		
		if(listofusers[i] == NULL){
			continue;
		}

		// This basically compares the longest username is all it does
		if(strncmp(listofusers[i]->usernamestr, u->usernamestr,(u->length > listofusers[i]->length)? u->length:listofusers[i]->length)==0){
				close(u->fd);
				free(u);
				pthread_rwlock_unlock(&lock);
				pthread_exit(NULL);
		}
	}
	
	for(i=0;i<MAXSIZE;i++){
		

		if(listofusers[i] == NULL){	
			
			listofusers[i] = u;
			numberofclients++;
			pthread_rwlock_unlock(&lock);
			sendJoin(u);
			return;
		}
	}
	
	pthread_rwlock_unlock(&lock);
}

void removeUser(user * u){
	
	int i;
	pthread_rwlock_wrlock(&lock);
	for(i = 0; i< MAXSIZE ; i++){
		
		if(listofusers[i] == NULL){
			continue;
		}

		if(listofusers[i]->fd == u->fd){
			listofusers[i] = NULL;
			numberofclients--;
			break;
		}
	}

	pthread_rwlock_unlock(&lock);
}

void * handShake(void* u){
 	user* currentUser = (user *) u;
 	uint16_t outnum=0;

 	unsigned char outarray [2];
	uint16_t holdingNumber = 0xcf;
	outarray[0] = (unsigned char) (holdingNumber);
	holdingNumber = 0xa7;
	outarray[1] = (unsigned char) (holdingNumber);
	//send (currentUser->fd, &outarray, sizeof (outarray),0);
	sendBytes(currentUser,outarray,2);

	unsigned char numberOfUsers = (unsigned char)numberofclients; 
	send (currentUser->fd,&numberOfUsers, sizeof(numberOfUsers),0);
	
	sendCurrentUserNames(currentUser);
	getUsername(currentUser);
	chat(currentUser);

}	

void sendCurrentUserNames(user * targetUser){
	int i;
	pthread_rwlock_rdlock(&lock);

	for(i=0;i <MAXSIZE;i++){

		user* currentUser = listofusers[i];
		
		if(currentUser == NULL){
			continue;
		}
		unsigned char outLength = (unsigned char)currentUser->length;
		
		sendBytes(targetUser, &outLength, 1);
		sendBytes(targetUser,currentUser->usernamestr,currentUser->length);
		
	}

	pthread_rwlock_unlock(&lock);
}

uint16_t lengthOfUsername(unsigned char userName[MAXSIZE]){
	uint16_t i;
	for(i = 0; i < MAXSIZE; i++){

		if(userName[i] == '\0')
			break;
	}

	return i + 1;
}

void closeSocket(user * u){
	
	if(u->length != 0){
		sendExit(u);
		removeUser(u);
	}

	close(u->fd);
	free(u);
	pthread_exit(NULL);
}

void getUsername(void* u){
	user* currentUser = (user *) u;
	
	unsigned char sizeOfUsername;

	recievedBytes(currentUser,(unsigned char*) &sizeOfUsername, 1);
	
	uint16_t size = (uint16_t) sizeOfUsername;
	unsigned char buff [MAXSIZE];

	recievedBytes(currentUser, buff, sizeOfUsername);
	
	memcpy(currentUser->usernamestr, (void *)buff, sizeof(buff));
	
	currentUser->length= size;
	
	addUser(currentUser); 

}

void chat(user * u){
	user * currentUser = u;

	uint16_t inlength;
	for(;;){
		unsigned char buffSize[2];
		recievedBytes(currentUser, buffSize, 2);
		
		uint16_t temp = buffSize[1] << 8 + buffSize[0];
		uint16_t size = ntohs(temp);

		unsigned char buffMessage[BUFFSIZE];
		memset(buffMessage, '\0', BUFFSIZE);
		
		if(size==0){
			continue;
		}
		recievedBytes(currentUser, buffMessage, size);



		// uint16_t outsize=size+(currentUser->length)+2;
		// unsigned char outMessage[BUFFSIZE];
		// memset(outMessage, '\0', BUFFSIZE);

		// //Set up the array so it sends a readable message "username : message"
		// memcpy(outMessage,(void*) currentUser->usernamestr,currentUser->length);
		// memset(&outMessage[currentUser->length], ':',1);
		// memset(&outMessage[currentUser->length+1 ], ' ',1);
		// memcpy(&outMessage[currentUser->length +2], (void *) buffMessage, size);

		// outMessage[outsize] = '\0';

		sendChatMessage(buffMessage,size,u);

		memset(buffMessage, '\0', BUFFSIZE);
		// memset(outMessage, '\0', BUFFSIZE);

	}
}
 
void sendChatMessage(char* p,  uint16_t size, user * sender){

	pthread_rwlock_rdlock(&lock);
	int i;
	for(i = 0; i < MAXSIZE; i++){
		user * currentUser  = listofusers[i];
		
		if(currentUser != NULL){
			unsigned char type = 0x0;
			uint16_t hostToNet=htons(size);
	
			sendBytes(currentUser,&type,1);
			sendBytes(currentUser,(unsigned char *)&(sender->length),1);
			sendBytes(currentUser,sender->usernamestr,sender->length);
			sendBytes(currentUser,(unsigned char*)&hostToNet,2);
			sendBytes(currentUser,p,size);
		}
	}
	pthread_rwlock_unlock(&lock);
}

void sendJoinMessage(char* p, uint16_t size){

	pthread_rwlock_rdlock(&lock);
	
	int i;
	for(i = 0; i < MAXSIZE; i++){
		user * currentUser  = listofusers[i];
		
		if(currentUser != NULL){
			unsigned char type = 0x1;
			unsigned char charSize = (unsigned char) size;
			
			sendBytes(currentUser,&type,1);
			sendBytes(currentUser, &charSize, 1);
			sendBytes(currentUser, p ,size);
		}
	}

	pthread_rwlock_unlock(&lock);
}

void sendExitMessage(char* p, uint16_t size){
	pthread_rwlock_rdlock(&lock);
	
	int i;
	for(i = 0; i < MAXSIZE; i++){
		user * currentUser  = listofusers[i];
		
		if(currentUser != NULL){
			unsigned char type = 0x2;
			unsigned char charSize = (unsigned char)size;

			sendBytes(currentUser,&type,1);
			sendBytes(currentUser, &charSize, 1);
			sendBytes(currentUser, p, size);
		}
	}
	
	pthread_rwlock_unlock(&lock);
}

void sendBytes(user * currentUser, unsigned char* buff, uint16_t numBytes){
	int sock =currentUser->fd;

	int numberToSend = numBytes;
	unsigned char * spotInBuffer = buff;
	
	while(numberToSend > 0){
		int sentBytes = send(sock, spotInBuffer, numberToSend, 0);
		if(sentBytes <= 0){
			
			if(sentBytes == -1)
				fprintf(fp, "%s\n",strerror(errno));
			
			closeSocket(currentUser);			
		}
	
		// Increment the char pointer to current sunfilled spot 
		spotInBuffer += sentBytes;
		numberToSend -= sentBytes;
	}
}

void recievedBytes(user * currentUser, unsigned char* buff, uint16_t numBytes){

	int sock = currentUser->fd;

	int numberToRead = numBytes;
	unsigned char * spotInBuffer = buff;
	
	while(numberToRead > 0){
		
		int recievedBytes = recv(sock, spotInBuffer, numberToRead, 0);
		if(recievedBytes <= 0){
			
			if(recievedBytes == -1)
				fprintf(fp, "%s\n",strerror(errno));

			closeSocket(currentUser);			
		}

		// Increment the char pointer to current sunfilled spot 
		spotInBuffer += recievedBytes;
		numberToRead -= recievedBytes;
	}
}

void sendJoin(user * currentUser){
	
	sendJoinMessage(currentUser->usernamestr,currentUser->length);
	writeToLogJoin(currentUser);
}

void sendExit(user * currentUser){

	sendExitMessage(currentUser->usernamestr,currentUser->length);
	writeToLogExit(currentUser);
}

void exitAll(){
	int i;
	pthread_rwlock_wrlock(&lock);
	for(i=0;i<MAXSIZE;i++){
		user * u = listofusers[i];
		if(u!=NULL){
			writeToLogExit(u);
			close(u->fd);
			free(u);
		}
		
	}
	pthread_rwlock_unlock(&lock);
	
}
void writeToLogJoin(user *u){
	char line[BUFFSIZE];

	// char usernametemp[BUFFSIZE];
	// memset(usernametemp,'\0',u->length);
	// strncpy
	memset(line,'\0',BUFFSIZE);
	strncpy(line,u->usernamestr,u->length);
	strcat(line," joined");
	fprintf(fp, "%s\n",line );
}

void writeToLogExit(user *u){
	char line[BUFFSIZE];
	memset(line,'\0',BUFFSIZE);
	strncpy(line,u->usernamestr,u->length);
	strcat(line," left");
	fprintf(fp, "%s\n",line );
}