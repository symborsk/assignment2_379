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
#define	MY_PORT	2222
#define MAXSIZE 20

/* ---------------------------------------------------------------------
 This	is  a sample server which opens a stream socket and then awaits
 requests coming from client processes. In response for a request, the
 server sends an integer number  such	 that  different  processes  get
 distinct numbers. The server and the clients may run on different ma-
 chines.
 --------------------------------------------------------------------- */
void handShake(int snew);
uint16_t lengthOfUsername(unsigned char userName[MAXSIZE]);
void sendCurrentUserNames(int snew);
void getUsername(int snew);

typedef struct users{
	int length;
	unsigned char usernamestr [MAXSIZE];
	pid_t pid;
} user;

int number = 0;
int numberofclients=0;
int fd;
user* sharedMem;

int main(){	
	
	fd_set fd_active;
	fd_set read_fd_set;
	struct timeval timer;
	timer.tv_sec=10;
	timer.tv_usec=2000;

	sharedMem = mmap(NULL, sizeof(user)*MAXSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	if(sharedMem == MAP_FAILED){
		printf("Error: %s\n", strerror(errno));
		exit(1);
	}

	int	sock, snew, fromlength, number, outnum;
	uint16_t inlength,usernameLength, in_uint16_t;
	uint32_t inusername;
	struct	sockaddr_in	master, from;
	
	sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror ("Server: cannot open master socket");
		exit (1);
	}

	master.sin_family = AF_INET;
	master.sin_addr.s_addr = INADDR_ANY;
	master.sin_port = htons (MY_PORT);

	if (bind (sock, (struct sockaddr*) &master, sizeof (master))) {
		perror ("Server: cannot bind master socket");
		exit (1);
	}

	listen (sock, 5);
	FD_ZERO(&fd_active);
	FD_SET(sock,&fd_active);

	pid_t forkstatus = 1; 
	for(;;){
		number = 0;
		

		read_fd_set=fd_active;

		if(select(FD_SETSIZE,&read_fd_set,NULL,NULL,NULL)<0){
			
			
			perror("select()");
			exit(1);
		}
		
		

		//Only want to frok if we are at the parent


		int i=0;
		for(i;i<FD_SETSIZE;i++){			
			if(FD_ISSET(i, &read_fd_set)){
				if(i == sock){
					fromlength = sizeof (from);
					
					snew = accept (sock, (struct sockaddr*) & from, & fromlength);

					printf("forking\n");
					numberofclients++;
					forkstatus = fork();
					

					//Child process
					if(forkstatus == 0){
						//user currentuser=listofusers[numberofclients];
						
						printf("im a child\n");
						close(sock);
						
						
						if(setsockopt(snew,SOL_SOCKET,SO_RCVTIMEO, (char *)&timer, sizeof(timer))<0){
							perror("setting timeout failed");
						}
						if (snew < 0) 
						{
							perror ("Server: accept failed");
							exit (1);
						}
						
						handShake(snew);
						printf("going into function\n");
						
						FD_SET(snew, &fd_active);

						// in_uint16_t=-1;
						// int a = recv(snew,&inlength,sizeof(inlength),0);
						

						// if(a  ==  -1){
							
						// 	uint16_t exit_val = -1;
						// 	//uint16_t outexit=htons(exit_val);
						// 	send(snew,&exit_val,sizeof(exit_val),0); 
						// 	//shutdown(snew,SHUT_RDWR);	
						// 	close (snew);	
						// }
						// else{
						// 	// usernameLength=ntohs(inlength);
						

						// 	// unsigned char usernamestr [usernameLength];
						// 	// recv(snew,&usernamestr,sizeof(usernamestr),0);
							
							
						// 	// currentuser.length=usernameLength;
						// 	// strcpy(currentuser.usernamestr,usernamestr);
						// 	// currentuser.pid=0;


						// 	close(snew);
						// 	exit(1);
						// }
						close(snew);
						exit(1);
					}
					//Parent process
					else{
						close(snew);	
					}
				}
				
				else
				{
					FD_CLR(i,&fd_active);
				}
			}

		}	
	
	}
}

void handShake(int snew){
 	uint16_t outnum=0;

 	unsigned char outarray [2];
	uint16_t holdingNumber = 0xcf;
	outarray[0] = (unsigned char) (holdingNumber);
	holdingNumber = 0xa7;
	outarray[1] = (unsigned char) (holdingNumber);
	send (snew, &outarray, sizeof (outarray),0);

	unsigned char numberOfUsers = (unsigned char)numberofclients-1; 
	printf("Number of users  %d\n",numberofclients );
	send (snew,&numberOfUsers, sizeof(numberOfUsers),0);
	sendCurrentUserNames(snew);
	getUsername(snew);	
}	

void sendCurrentUserNames(int snew){
	int i;
	printf("numberofclients %d\n",numberofclients);
	for(i=0;i<numberofclients-1;i++){
		user* currentuser= &sharedMem[i];
		
		printf("this is the usernamestr %s\n",currentuser->usernamestr );
		printf("this is the username length%d\n",currentuser->length );

		
		unsigned char length = (unsigned char) currentuser->length;
		unsigned char username [currentuser->length];
		username[0] = length;
		
		// printf("this is the usernamestr %s\n",currentuser.usernamestr );
		// printf("this is the username length%d\n",currentuser.length );
		memcpy(&username[1], (void *)&currentuser->usernamestr, currentuser->length-1);
		
		int sendval=send(snew, &username, sizeof(username),0);
		printf("%s\n",username );

		if(sendval<=0){
			printf("send failed\n");
		}


	}

}
uint16_t lengthOfUsername(unsigned char userName[MAXSIZE]){
	uint16_t i;
	for(i = 0; i < MAXSIZE; i++){

		
		if(userName[i] == '\0')
			break;
	}

	return i + 1;
}

void getUsername(snew){
	unsigned char buff [MAXSIZE];
	
	int receivebytes = recv(snew, &buff, sizeof(buff), 0);
	if(receivebytes<=0){
		close(snew);
		printf("closing socket\n");
		exit(1);
	}

	unsigned char temp = buff[0];
	uint16_t sizeOfUsername =  (uint16_t)temp;
	

	unsigned char userName[sizeOfUsername-1];
	memcpy(userName,(void *)&buff[1],sizeof(userName));

	int i;
	printf("Saving new user\n");
	printf("numberofclients = %d\n",numberofclients );
	printf("Name: %s\n",userName );

	user * currentuser = &sharedMem[numberofclients-1];

	currentuser->length=sizeOfUsernam-1;
	memcpy(&(currentuser->usernamestr), (void *)userName, sizeof(userName));
	currentuser->pid=0;

	printf("stored currentuser name length: %d \n", currentuser->length);
	printf("currentuser userName: %s\n ", currentuser->usernamestr);



}
