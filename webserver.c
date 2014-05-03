//Daniel Hono
//ICSI 416/516
//2014/5/2 
//Project 3 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef DEFINES
#define DEFINES

/* Basic booleans*/
#define true 1
#define false 1 

#define PORT "40000" //Server listens on port 40000
#define BACKLOG 10
#define BLOCKSIZE 512

#define httpOK "HTTP/1.1 200 OK\r\n"
#define unknownReq "HTTP/1.1 500 Internal Server Error\r\n"
#define notFound "HTTP/1.1 404 File Not Found\r\n"
#define cLength "Content-Length: "
#define cType "Content-Type: "
#define end "\r\n"

#endif

/*Function Prototypes*/
void setAddressInfo(struct addrinfo **); 
int setSocket(struct addrinfo*);
void myBind(struct addrinfo*, int);
void myListen(int); 
int acceptHTTPConnection(int);
void processGETReqs(int);
int openReqFile(char*);
char* getContentType(char*); 
void sendFile(int, int , int); 

int main(void) {

	struct addrinfo *res;
	int sock = 0,  httpFd = 0; 

	//Set up the connection. 
	setAddressInfo(&res);	
	sock = setSocket(res);
	myBind(res, sock);
	myListen(sock);
	
	printf("Server uses the current working directory as starting directory!!\n");
	printf("Server runs until user force quits!\n\n");

	//Handle server stuff.
	//Basically runs in an infinite loop until manually closed. 
	while(true) {
		httpFd = acceptHTTPConnection(sock);
		processGETReqs(httpFd);
		close(httpFd);
	}

	//Clean up
	close(sock);
	return 0;
}

/* Sets a linked list of address results to the resultlist */
void setAddressInfo(struct addrinfo **resultList) {

	struct addrinfo hints; //hints structure 
	int status; //Used for error checking on getaddrinfo return type. 

	memset(&hints, 0, sizeof hints);

	hints.ai_family = AF_UNSPEC; //v4 or v5
	hints.ai_socktype = SOCK_STREAM; //TCP
	hints.ai_flags = AI_PASSIVE; //fill in my ip for me. 

	if( (status = getaddrinfo(NULL, PORT, &hints, resultList)) != 0) { 
		fprintf(stderr, "Could not getaddrinfo~! %s\n", gai_strerror(status)); exit(EXIT_FAILURE);
	}

	return;
}

/*Returns a socket file descriptor to be used in the network communication */
/*Parameter info: addrinfo structure to be used in setting up the socket details */
int setSocket(struct addrinfo* info) {

	int sk = 0;
	struct timeval timeoutz;
	timeoutz.tv_sec = 3; 
	timeoutz.tv_usec = 0; 

	sk = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if(sk < 0) {
		fprintf(stderr, "Could not set socket.\n");
		exit(EXIT_FAILURE);
	}

	if((setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeoutz, sizeof(timeoutz))) < 0) {
		fprintf(stderr, "setsocket failed\n."); 
		exit(EXIT_FAILURE);
	}

	return sk; 
}

/*Function to bind a socket to a port */
void myBind(struct addrinfo* servinfo, int socketFd) {

	if(bind(socketFd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
		fprintf(stderr, "Error binding to port!\n");
		exit(EXIT_FAILURE);
	}
	return;
}

/* Listen on the port for connections */
void myListen(int socketFd) {

	if(listen(socketFd, BACKLOG) < 0) {
		fprintf(stderr, "Error Listening on socket #: %d\n", socketFd);
		exit(EXIT_FAILURE);
	}
	return;
}

/* Wait for a connection. In this case it's going to be an HTTP connection from a webbrowser or telnet or something */
/*Parameter(s): A socket listening on a port */
/*Return: returns a socket file descriptor to an open connection */
int acceptHTTPConnection(int listeningSocket) {

	struct sockaddr_storage client_addr; 	//Hold information about the client connection from an accept()
	socklen_t addr_size; 			//size of sockaddr struct to be used in the accept();
	int clientFd = 0;

	if((clientFd = accept(listeningSocket, (struct sockaddr *)&client_addr, &addr_size)) < 0) {
		fprintf(stderr, "Error accepting the connection!\n");
		exit(EXIT_FAILURE);
	}
	return clientFd; 
}

/*Function to process http GET requests from the webbrowser */
void processGETReqs(int openSocket) {

	char* requests = NULL;
	char* getToken = NULL;  
	char* fileName = NULL;
	FILE* socket = NULL; 
	int requestedFile = 0, contentLength = 0; 
	char* fileTypeHeader = NULL;
	char* fileLengthHeader = NULL;
	char* fileType = NULL; 
	char sizeFileString[20];

	//Allocate space for the fileTypeHeader
	fileTypeHeader = (char*)malloc(sizeof(char) * 1000);
	if(fileTypeHeader == NULL) {
		fprintf(stderr, "Could not allocate heap memory!\n"); exit(EXIT_FAILURE);
	}


	fileLengthHeader = (char*)malloc(sizeof(char)*1000);
	if(fileLengthHeader == NULL) {
		fprintf(stderr, "Could not allocate heap space\n");
		exit(EXIT_FAILURE);
	}

	requests = (char*)malloc(sizeof(char)*1001);
	if (requests == NULL) {
		fprintf(stderr, "Could not allocate heap memory!\n");
		exit(EXIT_FAILURE);
	}


	/*Clear out the buffers */
	memset(requests, 0, 1001);
	memset(fileTypeHeader, 0, sizeof(char) * 1000);
	memset(fileLengthHeader, 0, sizeof(char)*1000);
	memset(sizeFileString, 0, sizeof(char)*20);


	read(openSocket, (void*)requests, (size_t)1000);
	getToken = strtok(requests, "\r\n");

	if(getToken != NULL) {
		if(strncasecmp("GET", getToken, 3) == 0) { //actually a GET request 

			printf("GET request found!\n"); //debug 
			printf("%s\n", getToken); fflush(stdout); //debug

			//Get the filename requested. 
			strtok(getToken, " ");
			fileName = strtok(NULL, " ");
			printf("File reqested: %s\n", fileName); //debug 

			//respond
			requestedFile = openReqFile(fileName); 
			if (requestedFile == 0) {

				printf("File not found: 404\n"); //debug 
				write(openSocket, notFound, strlen(notFound));

			}  else {

				//Respond with an OK
				write(openSocket, httpOK,  strlen(httpOK)); 
			
				//Send content-size header
				contentLength = lseek(requestedFile, 0, SEEK_END);
				lseek(requestedFile, 0, SEEK_SET);
				sprintf(sizeFileString, "%d", contentLength);
				strncat(fileLengthHeader, cLength, strlen(cLength));
				strncat(fileLengthHeader, sizeFileString, strlen(sizeFileString));
				strncat(fileLengthHeader, end, strlen(end));
				write(openSocket, fileLengthHeader, strlen(fileLengthHeader));
				printf("sent size header: %s", fileLengthHeader); //debug 

				//Send content-type header. 
				fileType = getContentType(fileName);
				strncat(fileTypeHeader, cType, strlen(cType));
				strncat(fileTypeHeader, fileType, strlen(fileType));
				strncat(fileTypeHeader, end, strlen(end));
				write(openSocket, fileTypeHeader, strlen(fileTypeHeader));
				printf("Sent file header:%s", fileTypeHeader); //debug 
				
				//prepare to send content 
				write(openSocket, end, strlen(end));

				//send content
				printf("sending file\n"); //debug
				sendFile(openSocket, requestedFile,  contentLength);
			}

		} else { //any other request. 
			write(openSocket, unknownReq, strlen(unknownReq)); //respond with an error. 
		}

	} else {
		printf("no token found\n"); //debug
	}

	//Clean up
	free(requests);
	socket = fdopen(openSocket, "r");
	fflush(socket);
	fclose(socket);
	free(fileType);
	return;
}

/*Attempt to open a file reqested by a GET request */
/*Parameter(s): String containing full path to the file attempting to be opened*/
/*Return: A nonzero file descriptor if the file was succesully opened, 0 if the open failed*/
int openReqFile(char* filename) {

	int f = 0; //file descriptor 
	if( (f = open(++filename, O_RDONLY)) < 0) { //open the file with name after the '/' character 
		return 0; //return 0 if failed. 
	} else {
		return f;  //return opened file descriptor if opened. 
	}
}

/*Return the content type based on the file extension of the file being requested*/
/*Parameter(s): String that represents the file requested from the webbrowser*/
/*Return: pointer to the file extension of the file, or NULL if no file extension found */
char* getContentType(char* request) {

	char* contentType; 
	char* fullContentType; 

	fullContentType = (char*)malloc(sizeof(char)*30);
	if(fullContentType == NULL) {
		fprintf(stderr, "Could not allocate heap space!\n");
		exit(EXIT_FAILURE);
	}

	memset(fullContentType, 0, 30);

	contentType = strrchr(request, '.');
	if(contentType == NULL) {
		return NULL;
	} else {
		contentType += 1; //get rid of the '.'
		if(strcmp(contentType, "html") == 0) {
			strcpy(fullContentType, "text/html");
			return fullContentType; 
		} else if(strcmp(contentType, "pdf") == 0) {
			strcpy(fullContentType, "application/pdf");
			return fullContentType;
		} else if(strcmp(contentType, "txt") == 0) {
			strcpy(fullContentType, "text/plain");
			return fullContentType;
		} else if(strcmp(contentType, "jpg") == 0) {
			strcpy(fullContentType, "image/jpeg");
			return fullContentType;
		} else if(strcmp(contentType, "png") == 0) {
			strcpy(fullContentType, "image/png"); 
			return fullContentType; 
		} else if(strcmp(contentType, "gif") == 0) {
			strcpy(fullContentType, "image/gif");
			return fullContentType;
		} else if(strcmp(contentType, "css") == 0) {
			strcpy(fullContentType, "text/css");
			return fullContentType;
		}  else {
			strcpy(fullContentType, "text/html"); //if the type isn't found, just treat it like an html type....
			return fullContentType; 
		}
	}
}

/*Function to send a requested file to the web browser*/
/*Paramter(s): OpenSocket - an open TCP socket with the web browser's connection, 
 openFileDescriptor - an opened file descriptor of the file to be sent, fileSize - the size of the file to be sent in bytes */
/*Returns void*/
void sendFile(int openSocket, int openFileDescriptor, int fileSize) {

	unsigned char* buffer = NULL; //Buffer for writting. 
	int bytesWritten = 0; //Bytes written to the socket
	int bytesRead = 0;  //Bytes read in that current iteration. 
	FILE* socket = NULL; //for flushing the socket. 

	//Allocate the write buffer
	buffer = (unsigned char*)malloc(sizeof(unsigned char)*BLOCKSIZE);
	if(buffer == NULL) {
		printf("Could not allocate heap space.\n"); exit(EXIT_FAILURE); 
	}	

	//Clear out junk data
	memset(buffer, 0, sizeof(unsigned char)*BLOCKSIZE);

	//Flush the socket before we begin
	socket = fdopen(openSocket, "r+");
	fflush(socket);

	//Write the file's bytes to the socket
	while(bytesWritten < fileSize) {

		bytesRead = read(openFileDescriptor, (void*)buffer, BLOCKSIZE);
		bytesWritten += write(openSocket, (void*)buffer, bytesRead);

		if(bytesWritten == fileSize) {
			break;
		}

		memset((void*)buffer, 0, sizeof(unsigned char)*BLOCKSIZE);
	}
	
	printf("Bytes written to web browser: %d\n", bytesWritten); //debug 
	return;
}


