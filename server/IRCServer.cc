
const char * usage =
"                                                               \n"
"IRCServer:                                                   \n"
"                                                               \n"
"Simple server program used to communicate multiple users       \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   IRCServer <port>                                          \n"
"                                                               \n"
"Where 1024 < port < 65536.                                     \n"
"                                                               \n"
"In another window type:                                        \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where talk-server      \n"
"is running. <port> is the port number you used when you run    \n"
"daytime-server.                                                \n"
"                                                               \n";

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "IRCServer.h"
#include "HashTableVoid.cc"
#include "mystring.c"

int QueueLength = 5;
FILE * passwordFile;
struct USER {		
	const char * username;
	const char * password;
};
struct MESSAGE {
	char * message;
	int messageNumber;
};
struct ROOM {
	char * name;
	int userCount;
	int messageCount;
	int numAllocUsers;
	USER * users; 
	MESSAGE messages[100];
};
HashTableVoid serverTable;
int numAllocRooms = 100;
ROOM * room = (ROOM *) malloc(sizeof(ROOM *) * numAllocRooms);
int numUsers = 0;
int numRooms = 0;
int
IRCServer::open_server_socket(int port) {

	// Set the IP address and port for this server
	struct sockaddr_in serverIPAddress; 
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);
  
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

	return masterSocket;
}

void
IRCServer::runServer(int port)
{
	int masterSocket = open_server_socket(port);

	initialize();
	
	while ( 1 ) {
		
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
		processRequest( slaveSocket );		
	}
}

int
main( int argc, char ** argv )
{
	// Print usage if not enough arguments
	if ( argc < 2 ) {
		fprintf( stderr, "%s", usage );
		exit( -1 );
	}
	
	// Get the port from the arguments
	int port = atoi( argv[1] );

	IRCServer ircServer;

	// It will never return
	ircServer.runServer(port);
	
}

//
// Commands:
//   Commands are started y the client.
//
//   Request: ADD-USER <USER> <PASSWD>\r\n
//   Answer: OK\r\n or DENIED\r\n
//
//   REQUEST: GET-ALL-USERS <USER> <PASSWD>\r\n
//   Answer: USER1\r\n
//            USER2\r\n
//            ...
//            \r\n
//
//   REQUEST: CREATE-ROOM <USER> <PASSWD> <ROOM>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: LIST-ROOMS <USER> <PASSWD>\r\n
//   Answer: room1\r\n
//           room2\r\n
//           ...
//           \r\n
//
//   Request: ENTER-ROOM <USER> <PASSWD> <ROOM>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: LEAVE-ROOM <USER> <PASSWD>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: SEND-MESSAGE <USER> <PASSWD> <MESSAGE> <ROOM>\n
//   Answer: OK\n or DENIED\n
//
//   Request: GET-MESSAGES <USER> <PASSWD> <LAST-MESSAGE-NUM> <ROOM>\r\n
//   Answer: MSGNUM1 USER1 MESSAGE1\r\n
//           MSGNUM2 USER2 MESSAGE2\r\n
//           MSGNUM3 USER2 MESSAGE2\r\n
//           ...\r\n
//           \r\n
//
//    REQUEST: GET-USERS-IN-ROOM <USER> <PASSWD> <ROOM>\r\n
//    Answer: USER1\r\n
//            USER2\r\n
//            ...
//            \r\n
//

void
IRCServer::processRequest( int fd )
{
	// Buffer used to store the comand received from the client
	const int MaxCommandLine = 1024;
	char commandLine[ MaxCommandLine + 1 ];
	int commandLineLength = 0;
	int n;
	
	// Currently character read
	unsigned char prevChar = 0;
	unsigned char newChar = 0;
	
	//
	// The client should send COMMAND-LINE\n
	// Read the name of the client character by character until a
	// \n is found.
	//

	// Read character by character until a \n is found or the command string is full.
	while ( commandLineLength < MaxCommandLine &&
		read( fd, &newChar, 1) > 0 ) {

		if (newChar == '\n' && prevChar == '\r') {
			break;
		}
		
		commandLine[ commandLineLength ] = newChar;
		commandLineLength++;

		prevChar = newChar;
	}
	
	// Add null character at the end of the string
	// Eliminate last \r
	commandLineLength--;
        commandLine[ commandLineLength ] = 0;

	printf("RECEIVED: %s\n", commandLine);

//	printf("The commandLine has the following format:\n");
//	printf("COMMAND <user> <password> <arguments>. See below.\n");
//	printf("You need to separate the commandLine into those components\n");
//	printf("For now, command, user, and password are hardwired.\n");

	char * command  = (char*) malloc(sizeof(char*) * 100);
	char * user     = (char*) malloc(sizeof(char*) * 100);
	char * password = (char*) malloc(sizeof(char*) * 100);
	char * args     = (char*) malloc(sizeof(char*) * 100);

	command[0] = '\0';
	user[0] = '\0';
	password[0] = '\0';
	args[0] = '\0';

	int i = 0, j = 0, k = 0;
	for (i = 0; i < commandLineLength; i++) {
		if (commandLine[i] == ' ') {
			j++;
			i++;
			if (j <= 3) {
				k = 0;
			}
			if (j > 3) {
				args[k] = ' ';
				k++;
			}
		}if (commandLine[i] != ' ') {
			if (j == 0) {
				command[k] = commandLine[i];	
			} else if (j  == 1) {
				user[k] = commandLine[i];
			} else if (j == 2) {
				password[k] = commandLine[i];
			} else if (j > 2) {
				args[k] = commandLine[i];
			}
		}
		k++;
	}	
	
	printf("command=%s\n", command);
	printf("user=%s\n", user);
	printf( "password=%s\n", password );
	printf("args=%s\n", args);

	if (!strcmp(command, "ADD-USER")) {
		addUser(fd, user, password, args);
	}
	else if (!strcmp(command, "ENTER-ROOM")) {
		enterRoom(fd, user, password, args);
	}
	else if (!strcmp(command, "LEAVE-ROOM")) {
		leaveRoom(fd, user, password, args);
	}
	else if (!strcmp(command, "SEND-MESSAGE")) {
		sendMessage(fd, user, password, args);
	}
	else if (!strcmp(command, "GET-MESSAGES")) {
		getMessages(fd, user, password, args);
	}
	else if (!strcmp(command, "GET-USERS-IN-ROOM")) {
		getUsersInRoom(fd, user, password, args);
	}
	else if (!strcmp(command, "GET-ALL-USERS")) {
		getAllUsers(fd, user, password, args);
	}
	else if (!strcmp(command, "CREATE-ROOM")) {
		createRoom(fd, user, password, args);
	}
	else if (!strcmp(command, "LIST-ROOMS")) {
		listRooms(fd, user, password, args);
	}
	else {
		const char * msg =  "UNKNOWN COMMAND\r\n";
		write(fd, msg, strlen(msg));
	}

	// Send OK answer
	//const char * msg =  "OK\n";
	//write(fd, msg, strlen(msg));

	close(fd);	
}

void
IRCServer::initialize()
{
	// Open password file
	char line[1000];
	passwordFile = fopen("password.txt","a+");
	if (passwordFile == NULL) {
		printf("ERROR OPENING PASSWORD FILE\n");
		exit(1);
	}
	// Initialize users in room
	room[0].name = (char *)"default";
	room[0].userCount = 0;
	room[0].messageCount = 0;
	room[0].numAllocUsers = 100;
	room[0].users = (USER*) malloc(sizeof(USER*) * room[0].numAllocUsers);
	numRooms++;
	int j = 0;
	const char * none = "";

	while(fgets(line, 50, passwordFile) != NULL) {
		int i;
		char * pass = line;
		int length = strlen(line);
		for (i = 0; i < length; i++) {
			if (line[i] == ' ') {
				line[i] = '\0';
				room[0].users[j].username = (const char *) malloc(strlen(line));
				strcpy((char *)room[0].users[j].username,line);
				i++;
				pass++;
				char * temp = pass;
				while (*temp != 0) {
					temp++;
				}
				temp --;
				*temp = '\0';
				room[0].users[j].password  = (const char *) malloc(strlen(line));
				strcpy((char *)room[0].users[j].password, pass);
				
				room[0].userCount++;
				room[0].messageCount++;
				numUsers++;
				break;
			}
			pass++;
		}
		
		printf("ADDED USER: %s\n", room[0].users[j].username);
		j++;
	}
	// Initalize message list

}

bool
IRCServer::checkPassword(int fd, const char * user, const char * password) {
	// Here check the password
	int j = 0;
	while (j < numUsers) {
		if (!strcmp(room[0].users[j].username, user)) {
			if (!strcmp(room[0].users[j].password, password)) {
				return true;
			} else {
				return false;
			}
		}
		j++;
	}
	return false;
}

void
IRCServer::addUser(int fd, const char * user, const char * password, const char * args)
{
	// Here add a new user. For now always return OK.
	const char * msg;
	if (user[0] == '\0' || password[0] == '\0') {
		msg = "DENIED\r\n";
	} else {
		if (room[0].userCount + 1 > room[0].numAllocUsers - 1) {
			room[0].numAllocUsers += 10;
			room[0].users = (USER*) realloc(room[0].users, room[0].numAllocUsers);
		}
		int i = 0;
		while (i < room[0].userCount) {
			if (!strcmp(user, room[0].users[i].username)) {
				msg = "OK\r\n";
				write(fd, msg, strlen(msg));
				return;
			}
			i++;
		}
		fprintf(passwordFile, "%s %s\n", user, password); 
		fflush(passwordFile);
		room[0].users[numUsers].username = user;
		room[0].users[numUsers].password = password;
		room[0].userCount++;
		numUsers++;
		msg =  "OK\r\n";
	}
	write(fd, msg, strlen(msg));
	return;		
}

void
IRCServer::createRoom(int fd, const char * user, const char * password, const char * args)
{
	const char * msg;
	if (!checkPassword(fd, user, password)) {
		write(fd, "ERROR (Wrong password)\r\n", strlen("ERROR (Wrong password)\r\n"));
		return;
	}
	if (numRooms + 1 > numAllocRooms) {
		numAllocRooms += 5;
		room = (ROOM *) realloc(room, sizeof(ROOM *) * numAllocRooms);
	}
	int i = 1;
	while (i < numRooms) {
		if (!strcmp(room[i].name, (char *)args)) {
			msg = "OK\r\n";
			write(fd, msg, strlen(msg));
			return;
		}
		i++;
	}
	room[numRooms].name = (char *) args;
	room[numRooms].userCount = 0;
	room[numRooms].messageCount = 0;
	room[numRooms].numAllocUsers = 1;
	room[numRooms].users = (USER*) malloc(sizeof(USER*) * room[numRooms].numAllocUsers);
	numRooms++;
	msg = "OK\r\n";
	write(fd, msg, strlen(msg));
}

void
IRCServer::listRooms(int fd, const char * user, const char * password, const char * args)
{
	if (!checkPassword(fd, user, password)) {
		write(fd, "ERROR (Wrong password)\r\n", strlen("ERROR (Wrong password)\r\n"));
		return;
	}
	int j = 1;
	char * rooms;
	char * num = (char *) malloc(10);
	while (j < numRooms) {
		//write(fd, "ROOM", strlen("ROOM")); 
		//sprintf(num, "%d ", j + 1);
		//write(fd, num ,strlen(num));
		write(fd, room[j].name, strlen(room[j].name));
		write(fd, "\r\n", strlen("\r\n"));
		j++;
	}
}
void
IRCServer::enterRoom(int fd, const char * user, const char * password, const char * args)
{
	if (!checkPassword(fd, user, password)) {
		write(fd, "ERROR (Wrong password)\r\n", strlen("ERROR (Wrong password)\r\n"));
		return;
	}
	const char * msg;
	int found = 0;
	int j = 1, i = 0;
	while (j < numRooms) {
		if (!strcmp(room[j].name, args)) {
			int k = 0;
			while (k < room[j].userCount) {
				if (!strcmp(room[j].users[k].username, user)) {
					write(fd, "OK\r\n", strlen("OK\r\n"));
					return;
				}
				k++;
			}
			//add user to room
			if (room[j].userCount + 1 > room[j].numAllocUsers) {
				room[j].numAllocUsers += 5;
				room[j].users = (USER *) realloc(room[j].users, sizeof(USER *) * room[j].numAllocUsers);
			}
			room[j].users[k].username = user;
			room[j].users[k].password = password;
			room[j].userCount++;
			found = 1;
			msg = "OK\r\n";
			break;
		}
		j++;
	}
	if (!found) {
		msg = "ERROR (No room)\r\n";
	}
	write(fd, msg, strlen(msg));
}

void
IRCServer::leaveRoom(int fd, const char * user, const char * password, const char * args)
{
	if (!checkPassword(fd, user, password)) {
		write(fd, "ERROR (Wrong password)\r\n", strlen("ERROR (Wrong password)\r\n"));
		return;
	}
	const char * msg;
	int j = 1, k = 0;
	while (j < numRooms) {
		msg = "ERROR (not a room)\r\n";
		if (!strcmp(room[j].name, args)) {
			msg = "ERROR (No user in room)\r\n";
			while (k < room[j].userCount) {
				if (!strcmp(room[j].users[k].username, user)) {
					printf("user: %s username: %s\n", room[j].users[k].username, user);
					room[j].userCount--;
					printf("%d\n", room[j].userCount);
					while (k < room[j].userCount) {
						strcpy((char *)room[j].users[k].username,(char *)room[j].users[k+1].username);
						k++;
					}
					msg = "OK\r\n";
					break;
				}
				k++;
			}
		}
		j++;
	}
	write(fd, msg, strlen(msg));
}

void
IRCServer::sendMessage(int fd, const char * user, const char * password, const char * args)
{
	if (!checkPassword(fd, user, password)) {
		write(fd, "ERROR (Wrong password)\r\n", strlen("ERROR (Wrong password)\r\n"));
		return;
	}
	//parse messages
	const char * msg = "DENIED\r\n";
	char * thisRoom = (char *)malloc(100);
	char * message = (char *)malloc(100);
	int i, j = 0, space = 0;
	int foundUser = 0, foundRoom = 0;
	for (i = 0; i < strlen(args); i++) {
		if (args[i] == ' ') {
			space = 1;
		}
		if (!space) {
			thisRoom[i] = args[i];
		} else {
			message[j] = args[i];
			j++;
		}
	}
	int  k = 1;
	j = 0;
	while (k < numRooms) {
		printf("room: -%s- thisRoom -%s- strcmp %d\n", room[k].name, thisRoom, strcmp(room[k].name, thisRoom));
		//msg = "OK\r\n";
		if (!strcmp(room[k].name, thisRoom)) {
			foundRoom = 1;
			while (j < room[k].userCount) {
				if (!strcmp(room[k].users[j].username, user)) {	
					foundUser = 1;
					if (room[k].messageCount >= 100) {
						int l = 0;
						while (l < room[k].messageCount - 1) {
							strcpy(room[k].messages[l].message, room[k].messages[l+1].message);
							l++;
						}
						room[k].messageCount--;
					}
					room[k].messages[room[k].messageCount].message = (char *)malloc(strlen((const char *)message) + strlen((const char *)user) + 1);
					strcpy(room[k].messages[room[k].messageCount].message, user);
					strcat(room[k].messages[room[k].messageCount].message, message);
					room[k].messageCount++;
					msg = "OK\r\n";
					write(fd, msg, strlen(msg));
					return;
				}
				j++;
			}
		}
		k++;
	}
	if (!foundUser) {
		msg = "ERROR (user not in room)\r\n";
	}
	if (!foundRoom) {
		msg = "ERROR (No room)\r\n";
	}
	write(fd, msg, strlen(msg));
}

void
IRCServer::getMessages(int fd, const char * user, const char * password, const char * args)
{
	if (!checkPassword(fd, user, password)) {
		write(fd, "ERROR (Wrong password)\r\n", strlen("ERROR (Wrong password)\r\n"));
		return;
	}
	const char * msg;
	char * thisRoom = (char *)malloc(100);
	char * lastMessageNum = (char *)malloc(100);
	int i, j = 0, space = 0;
	int foundUser = 0, foundRoom = 0;
	for (i = 0; i < strlen(args); i++) {
		if (args[i] == ' ') {
			space = 1;
			i++;
		}
		if (!space) {
			lastMessageNum[i] = args[i];
		} else {
			thisRoom[j] = args[i];
			j++;
		}
	}
	printf("-%s- -%s-\n", lastMessageNum, thisRoom);
	int lastMessage = atoi(lastMessageNum);
	int k = 1; 
	j = lastMessage + 1;
	i = 0;
	char * num = (char *) malloc(10);
	while (k < numRooms) {
		msg = "ERROR (No Room)\r\n";
		if (!strcmp(room[k].name, thisRoom)) {
			foundRoom = 1;
			msg = "ERROR (User not in room)";
			while (i < room[k].userCount) {
				if (!strcmp(room[k].users[i].username, user)) {	
					foundUser = 1;
					if (j >= room[k].messageCount) {
						msg = "NO-NEW-MESSAGES";
						write(fd, msg, strlen(msg));
						write(fd, "\r\n", strlen("\r\n"));
						return;
					}
					while (j < room[k].messageCount) {
						sprintf(num, "%d", j);
						write(fd, num, strlen(num));
						write(fd, " ", strlen(" "));
						write(fd, room[k].messages[j].message, strlen(room[k].messages[j].message));			
						write(fd, "\r\n", strlen("\r\n"));
						j++;
					}
					write(fd, "\r\n", strlen("\r\n"));
					return;
				}
				i++;
			}
		}
		k++;
	}
	if (!foundUser) {
		msg = "ERROR (User not in room)\r\n";
	}
	if (!foundRoom) {
		msg = "ERROR (No room)\r\n";
	}
	write(fd, msg, strlen(msg));
}

void
IRCServer::getUsersInRoom(int fd, const char * user, const char * password, const char * args) 
{
	if (!checkPassword(fd, user, password)) {
		write(fd, "ERROR (Wrong password)\r\n", strlen("ERROR (Wrong password)\r\n"));
		return;
	}
	int j = 0, k = 1;
	char * users;
	char * num = (char *) malloc(10);
	while (k < numRooms) {
		if (!strcmp(room[k].name, args)) {
			//sort users in room
			int x, y;
			char * tempName = (char *) malloc(30);
			char * tempPass = (char *) malloc(30);
			for (x = 0; x < room[k].userCount - 1; x++) {
				for (y = 0; y < room[k].userCount - 1; y++) {
					if (strcmp((char *)room[k].users[y].username, (char *)room[k].users[y+1].username) > 0) {
						strcpy(tempName, (char *)room[k].users[y].username);
						strcpy(tempPass, (char *)room[k].users[y].password);
						strcpy((char *)room[k].users[y].username, (char *)room[k].users[y + 1].username);
						strcpy((char *)room[k].users[y].password, (char *)room[k].users[y + 1].password);
						strcpy((char *)room[k].users[y + 1].username, tempName);
						strcpy((char *)room[k].users[y + 1].password, tempPass);
					}
				}
			}	
			while (j < room[k].userCount) {
				write(fd, room[k].users[j].username, strlen(room[k].users[j].username));
				write(fd, "\r\n", strlen("\r\n"));
				j++;
			}
			write(fd, "\r\n", strlen("\r\n"));
			break;
		}
		k++;
	}
}

void
IRCServer::getAllUsers(int fd, const char * user, const char * password,const  char * args)
{
	if (!checkPassword(fd, user, password)) {
		write(fd, "ERROR (Wrong password)\r\n", strlen("ERROR (Wrong password)\r\n"));
		return;
	}
	int j = 0;
	char * users;
	char * num = (char *) malloc(10);
	//sort users in room
	int x, y;
	char * tempName = (char *) malloc(30);
	char * tempPass = (char *) malloc(30);
	for (x = 0; x < room[0].userCount - 1; x++) {
		for (y = 0; y < room[0].userCount - 1; y++) {
			if (strcmp((char *)room[0].users[y].username, (char *)room[0].users[y + 1].username) > 0) {
				strcpy(tempName, (char *)room[0].users[y].username);
				strcpy(tempPass, (char *)room[0].users[y].password);
				strcpy((char *)room[0].users[y].username, (char *)room[0].users[y + 1].username);
				strcpy((char *)room[0].users[y].password, (char *)room[0].users[y + 1].password);
				strcpy((char *)room[0].users[y + 1].username, tempName);					
				strcpy((char *)room[0].users[y + 1].password, tempPass);
			}
		}
	}
	while (j < numUsers) {
		write(fd, room[0].users[j].username, strlen(room[0].users[j].username));
		write(fd, "\r\n", strlen("\r\n"));
		j++;
	}
	write(fd, "\r\n", strlen("\r\n"));
}

