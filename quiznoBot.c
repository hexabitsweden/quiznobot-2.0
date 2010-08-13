/////////////////////////////////////////////////////////////////////////////
//  quiznoBot - A simple IRC XDCC bot to facilitate the sharing of files via
//              the various IRC networks.
/////////////////////////////////////////////////////////////////////////////
//  Copyright 2010 Ron Moore
/////////////////////////////////////////////////////////////////////////////
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////
//  Instructions:
//    The bot is very simple to use:
//      quiznoBot -n [nick] -c [channel] -s [server] -p [port] -d [dir] (-v)(-v)
//    The options work as follows:
//      -n [nick] -- Specify the name of the bot on the IRC network.
//      -c [channel] -- specify the channel the bot will join.
//      -s [server] -- specify the server the bot will join (IPV4 only please)
//      -p [port] -- specify the port the server will use
//      -d [dir] -- specify the directory the server will share.
//      -v -- increase the debug level (amount of information printed to the
//            user, one should be plenty for admins, two is good for developers
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
//needed to keep mac os x happy =(
#include <netdb.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>

//defines the default timeout in seconds
#define DEFAULT_TIMEOUT 10
#define COMMAND_TOKENS 500

#define TERM_BLUE_ON_BLACK "\e[36;40m"
#define TERM_GREEN_ON_BLACK "\e[32;40m"
#define TERM_RED_ON_BLACK "\e[31;40m"
#define TERM_YELLOW_ON_BLACK "\e[35;40m"
#define TERM_RESET_COLOR "\e[0m"

enum CommandLineSettings
{
   IRC_CHANNEL_SET = 0x1,
   IRC_SERVER_SET = 0x2,
   IRC_NICK_SET = 0x4,
   BOT_DIRECTORY_SET = 0x8,
   IRC_PORT_SET = 0x10
} settings;

struct SharedFile
{
   char *filename;
   long filesize;
} *dirContents = NULL;

struct TransferRequest
{
   int filenumber;
   char nick[128];
};

struct TransferRequest transferQueue[100];
int transferQueueFront = 0;
int transferQueueBack = 0;

const char* QUIT_COMMAND = "QUIT :quiznoBot http://quizno50.x10hosting.com\n";

int sharedFileArraySize = 0;
int numSharedFiles = 0;

char channel[128];
char nick[128];
char server[128];
char directory[512];
char port[10];

int debugLevel = 0;

struct addrinfo *serverAddress;
struct addrinfo serverAddressHints;
int serverSocket = -1;

char userCommandSent = 0;
char nickCommandSent = 0;
char joinCommandSent = 0;

int transferPort = 41000;
char transferPortString[10];

int enqueueTransfer(int filenumber, char* nick)
{
   ++transferQueueBack;
   transferQueue[transferQueueBack].filenumber = filenumber;
   strcpy(transferQueue[transferQueueBack].nick, nick);
   return transferQueueBack;
}

void dequeueTransfer()
{
   ++transferQueueFront;
   if (transferQueueFront > transferQueueBack) --transferQueueFront;
}

void printUsage()
{
	printf("Usage: %squiznoBot%s [options]\n", TERM_YELLOW_ON_BLACK,
          TERM_RESET_COLOR);
	printf("%sOptions:%s\n", TERM_RED_ON_BLACK, TERM_RESET_COLOR);
	printf("\t%ss %sserver%s - %sSets the server to connect to.%s\n",
          TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK, TERM_RESET_COLOR,
          TERM_GREEN_ON_BLACK, TERM_RESET_COLOR);
	printf("\t%sc %schannel%s - %sSets the channel to join.%s\n",
          TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK, TERM_RESET_COLOR,
          TERM_GREEN_ON_BLACK, TERM_RESET_COLOR);
	printf("\t%sn %snick%s - %sSets the bot's nickname.%s\n",
          TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK, TERM_RESET_COLOR,
          TERM_GREEN_ON_BLACK, TERM_RESET_COLOR);
	printf("\t%sd %sdirectory%s - %sSets the directory the bot scans for files.%s\n",
          TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK, TERM_RESET_COLOR,
          TERM_GREEN_ON_BLACK, TERM_RESET_COLOR);
	printf("\t%sp %sport%s - %sSets the port to use to connect to the server.%s\n",
          TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK, TERM_RESET_COLOR,
          TERM_GREEN_ON_BLACK, TERM_RESET_COLOR);
	printf("\t%sv%s - %sIncreases the verbosity level (can be used more",
          TERM_RED_ON_BLACK, TERM_RESET_COLOR, TERM_GREEN_ON_BLACK);
	printf(" than once)%s\n\n", TERM_RESET_COLOR);
	printf("Examples:\n");
	printf("\t%squiznoBot%s -%ss %sirc.rizon.net%s -%sc %s#eclipse%s -%sn %sMyAwesomeBot%s",
          TERM_YELLOW_ON_BLACK, TERM_RESET_COLOR,
          TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK, TERM_RESET_COLOR,
          TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK, TERM_RESET_COLOR,
          TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK, TERM_RESET_COLOR);
	printf(" -%sp %s6667%s\n", TERM_RED_ON_BLACK, TERM_BLUE_ON_BLACK,
          TERM_RESET_COLOR);
	printf("\tConnects to irc.rizon.net on port 6667 joins #eclipse and ");
	printf("sets\n\tthe nick to MyAwesomeBot\n\n");
	printf("Defaults:\n");
	printf("\tBy Default the server is run with the following options:\n");
	printf("\tquiznoBot -d ./ -s irc.freenode.net -c #ubuntu");
	printf(" -n IRC_BOT_#### -p 6667\n");
}

void parseCommandline(int argc, char **argv)
{
   int currentArg = 1;
   while (currentArg < argc)
   {
      if (argv[currentArg][0] == '-')
      {
         switch (argv[currentArg][1])
         {
            case 's':
               if (debugLevel >= 1)
                  fprintf(stderr, "Setting server to: %s\n", 
                     argv[currentArg + 1]);
               strcpy(server, argv[currentArg + 1]);
               ++currentArg;
               settings |= IRC_SERVER_SET;
               break;
            case 'c':
               if (debugLevel >= 1)
                  fprintf(stderr, "Setting channel to: %s\n",
                     argv[currentArg + 1]);
               strcpy(channel, argv[currentArg + 1]);
               ++currentArg;
               settings |= IRC_CHANNEL_SET;
               break;
            case 'n':
               if (debugLevel >= 1)
                  fprintf(stderr, "Setting nick to: %s\n",
                     argv[currentArg + 1]);
               strcpy(nick, argv[currentArg + 1]);
               ++currentArg;
               settings |= IRC_NICK_SET;
               break;
            case 'd':
               if (debugLevel >= 1)
                  fprintf(stderr, "Setting directory to: %s\n",
                     argv[currentArg + 1]);
               strcpy(directory, argv[currentArg + 1]);
               ++currentArg;
               settings |= BOT_DIRECTORY_SET;
               break;
            case 'p':
               if (debugLevel >= 1)
                  fprintf(stderr, "Setting port to: %i\n",
                     atoi(argv[currentArg + 1]));
               strcpy(port, argv[currentArg + 1]);
               ++currentArg;
               settings |= IRC_PORT_SET;
               break;
            case 'v':
               ++debugLevel;
               fprintf(stderr, "Increasing debug level to: %i\n", debugLevel);
               break;
				case '?':
					printUsage();
					exit(0);
					break;
				case 'h':
					printUsage();
					exit(0);
					break;
            default:
               fprintf(stderr, "Unknown option: %c", argv[currentArg][1]);
               break;
         }
      }
      ++currentArg;
   }
}

void setDefaults()
{
	if ((settings & IRC_PORT_SET) == 0x0) //if the port wasn't set...
   {
      if (debugLevel >= 1)
         fprintf(stderr, "Port not set: defaulting to 6667...\n");
      strcpy(port, "6667");
   }
   
   if ((settings & IRC_SERVER_SET) == 0x0) //if the server wasn't set
   {
      if (debugLevel >= 1)
         fprintf(stderr, "Server not set: defaulting to irc.freenode.net\n");
      strcpy(server, "irc.freenode.net");
   }
   
   if ((settings & IRC_NICK_SET) == 0x0) //if the nick wasn't set
   {
      sprintf(nick, "IRC_BOT_%04d", rand() % 10000);
      if (debugLevel >= 1)
         fprintf(stderr, "Nick not set: defaulting to %s\n", nick);
   }
   
   if ((settings & BOT_DIRECTORY_SET) == 0x0) //if the directory wasn't set
   {
      getcwd(directory, 512);
      if (debugLevel >= 1)
         fprintf(stderr, "Directory not set: defaulting to %s\n", directory);
   }
   
   if ((settings & IRC_CHANNEL_SET) == 0x0) //if the channel wasn't set
   {
      strcpy(channel, "#ubuntu");
      if (debugLevel >= 1)
         fprintf(stderr, "Channel not set: defaulting to %s\n", channel);
   }
}

int IRC_GetServerResponse(char *serverMessage)
{
   char *currentChar = serverMessage;
   char temp[5];
   int tempPos = 0;
   int numSpacesPassed = 0;
   
   while (numSpacesPassed < 1)
   {
      if (*currentChar == ' ') ++numSpacesPassed;
      ++currentChar;
   }
   
   while (*currentChar != ' ')
   {
      if (*currentChar >= 0x30 && *currentChar <= 0x39)
      {
         temp[tempPos] = *currentChar;
         temp[tempPos + 1] = '\0';
         tempPos += 1;
      }
      ++currentChar;
   }
   return atoi(temp);
}

int IRC_Connect()
{
	struct addrinfo *currentAddress = serverAddress;
	int connectResult = -1;
	char ipString[128];
	
	if (debugLevel >= 1)
		fprintf(stderr, "\nConnecting to %s:%s...\n", server, port);
	
	memset(&serverAddressHints, 0, sizeof(serverAddressHints));
	serverAddressHints.ai_family = AF_INET;
	serverAddressHints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(server, port, &serverAddressHints, &serverAddress))
	{
		fprintf(stderr, "Error: Could not resolve %s!\n", server);
		return -1;
	}
	currentAddress = serverAddress;
	while (currentAddress != NULL && (serverSocket == -1 || connectResult == -1))
	{
	   if (debugLevel > 1)
	   {
	      inet_ntop(currentAddress->ai_family, currentAddress->ai_addr,
	         ipString, 128);
	      fprintf(stderr, "Trying: %s...\n", ipString);
	   }
		serverSocket = socket(currentAddress->ai_family,
									 currentAddress->ai_socktype,
									 currentAddress->ai_protocol);
		if (serverSocket != -1)
			connectResult = connect(serverSocket, currentAddress->ai_addr,
											currentAddress->ai_addrlen);
			
		//get the next address
		currentAddress = currentAddress->ai_next;
	}
	if (debugLevel >= 1)
		fprintf(stderr, "Connected to %s:%s socket num: %08x!\n", server, port,
				  serverSocket);
   
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// This function has been ripped apart because the code that's commented out
// didn't want to work on some servers. So I commented it out and just put
// a sleep in there so it'd wait a few seconds before trying to login to the
// server...
/////////////////////////////////////////////////////////////////////////////
void IRC_WaitEndOfMOTD()
{
   /*
   char buffer[4096];
   int bufferPos = 0;
   char temp[6];
   int end = 0;
   */
   
   sleep(3);
   
   /*
   memset(buffer, 0, 4096);
   while (end == 0)
   {
      if (debugLevel == 2) fprintf(stderr, "\nWaiting for server...");
      recv(serverSocket, buffer, 4096, 0);
      if (debugLevel == 2) fprintf(stderr, "\nGot response!\n");
      bufferPos = 0;
      while (bufferPos <= 4096 - 6 && end == 0)
      {
         memcpy(temp, &(buffer[bufferPos]), 5);
         temp[5] = '\0';
         if (strcmp(temp, " 376 ") == 0)
            end = 1;
         //speedup trick: if we've read a response, but it's the wrong one;
         //we'll just advance to the end of the line...
         if (temp[0] == ' ' && temp[4] == ' ')
            while (buffer[bufferPos] != '\n') ++bufferPos;
         ++bufferPos;
      }
   }
   */
}

int IRC_Login()
{
   char userCommand[100];
   char nickCommand[100];
   char joinCommand[100];
   char hostname[100];
   
   gethostname(hostname, 100);
   
   sprintf(userCommand, "USER %s %s %s :%s\n", nick, hostname, server, nick);
   sprintf(nickCommand, "NICK %s\n", nick);
   sprintf(joinCommand, "JOIN %s\n", channel);
   
   if (debugLevel == 1)
      fprintf(stderr, "Sending login commands...\n");
   else if (debugLevel > 1)
      fprintf(stderr, "Sending user command: %s\n", userCommand);
   send(serverSocket, userCommand, strlen(userCommand), 0);
   userCommandSent = 1;
   
   if (debugLevel > 1)
      fprintf(stderr, "Sending nick command: %s\n", nickCommand);
   send(serverSocket, nickCommand, strlen(nickCommand), 0);
   nickCommandSent = 1;
   
   if (debugLevel > 0)
      fprintf(stderr, "Listening 'till end of MOTD...");
   IRC_WaitEndOfMOTD();
   if (debugLevel > 0)
      fprintf(stderr, "done...\n");
   
   if (debugLevel > 1)
      fprintf(stderr, "Sending join command: %s\n", joinCommand);
   send(serverSocket, joinCommand, strlen(joinCommand), 0);
   joinCommandSent = 1;
   
   if (debugLevel == 1)
      fprintf(stderr, "Logged in...\n");
   else if (debugLevel > 1)
      fprintf(stderr, "Logged in as %s\n", nick);
   
   return 0;
}

int IRC_Disconnect()
{
   char buffer[128];
   if (debugLevel >= 1)
      fprintf(stderr, "Diconnecting from server...\n");
   send(serverSocket, QUIT_COMMAND, strlen(QUIT_COMMAND), 0);
   while (recv(serverSocket, buffer, 128, 0) > 0);
   freeaddrinfo(serverAddress);
	if (close(serverSocket) == -1)
	{
		fprintf(stderr, "Couldn't close socket: %i\n", serverSocket);
		return -1;
	}
	return 0;
}

int IRC_SendMessage(char *toSend)
{
   return send(serverSocket, toSend, strlen(toSend), 0);
}

void DIR_Scan()
{
   DIR *toScan;
   FILE *fileToAdd;
   struct dirent *dirEntry = NULL;
   char fullpath[4096];
   
   if (debugLevel > 0)
      fprintf(stderr, "\nScanning directory: %s\n", directory);
   
   toScan = opendir(directory);
   if (toScan == NULL)
   {
      fprintf(stderr, "Couldn't open directory: %s", directory);
      exit(-1);
   }
   
   dirEntry = readdir(toScan);
   while (dirEntry != NULL)
   {
      if (debugLevel >= 2)
      {
         fprintf(stderr, "Checking file %s\n", dirEntry->d_name);
      }
      if (dirEntry->d_name[0] != '.' && dirEntry->d_type == DT_REG)
      {
         strcpy(fullpath, directory);
         if (fullpath[strlen(fullpath) - 1] != '/')
            strcat(fullpath, "/");
         strcat(fullpath, dirEntry->d_name);
         fileToAdd = fopen(fullpath, "r");
         if (fileToAdd != NULL)
         {
            if (dirContents == NULL)
            {
               dirContents = malloc(sizeof(struct SharedFile) * 25);
               sharedFileArraySize = 25;
            }
            if (sharedFileArraySize == numSharedFiles)
            {
               dirContents = realloc(dirContents, sizeof(struct SharedFile) *
                  (sharedFileArraySize * 2));
               sharedFileArraySize *= 2;
            }
            fseek(fileToAdd, 0, SEEK_END);
            dirContents[numSharedFiles].filesize = ftell(fileToAdd);
            dirContents[numSharedFiles].filename = 
               calloc(sizeof(char), (strlen(dirEntry->d_name) + 1));
            strcat(dirContents[numSharedFiles].filename, dirEntry->d_name);
            if (debugLevel > 1)
               fprintf(stderr, "\tAdding file %s - %i bytes to slot #%i\n", 
                  dirContents[numSharedFiles].filename,
                  (int)dirContents[numSharedFiles].filesize, numSharedFiles);
            ++numSharedFiles;
            fclose(fileToAdd);
         }
      }
      dirEntry = readdir(toScan);
   }
   closedir(toScan);
}

char ** getMessageParts(char *message)
{
   static char **toReturn;
   char *temp;
   int i;
   
   toReturn = calloc(COMMAND_TOKENS, sizeof(char*));
   for (i = 0; i < COMMAND_TOKENS; ++i)
      toReturn[i] = calloc(128, sizeof(char));

   //reset i
   i = 0;

   temp = strtok(message, " :!\r\n");
   while (temp != NULL)
   {
      strcpy(toReturn[i], temp);
      temp = strtok(NULL, " :!\r\n");
      ++i;
   }
   
   return toReturn;
}

void freeMessageParts(char **toFree)
{
   int i;
   for (i = 0; i < COMMAND_TOKENS; ++i)
      free(toFree[i]);
   free(toFree);
}

void prepareTransfer(char **message)
{
   char *buffer = NULL;
   pid_t forkId;
   int listenSocket = 0;
   int transferSocket = 0;
   size_t filePosition = 0;
   FILE *toTransfer;
   struct addrinfo *myAddress;
   struct addrinfo hints;
   struct sockaddr connectedAddr;
   struct sockaddr_storage theirAddr;
   socklen_t theirAddrSize = 120;
   char *inputBuffer = NULL;
   char *fileToOpen = NULL;
   
   forkId = fork();
   if (forkId == -1)
   {
      fprintf(stderr, "Couldn't fork child process!\n");
      return;
   }
   else if (forkId == 0) //child process
   {
      //moved from mainLoop function
      srand(time(NULL));
      transferPort = rand() % 50 + transferPort;
      sprintf(transferPortString, "%d", transferPort);
      struct sockaddr_in myself;
      socklen_t addrSize = sizeof(struct sockaddr_in);
      int packNumber = atoi(message[6] + 1);
      char *sendBuffer;

      memset(&hints, 0, sizeof(hints));
      getsockname(serverSocket, &connectedAddr, &theirAddrSize);
      hints.ai_addr = &connectedAddr;
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_PASSIVE;
      getaddrinfo(NULL, transferPortString, &hints, &myAddress);
      listenSocket = socket(myAddress->ai_family, myAddress->ai_socktype,
                              myAddress->ai_protocol);
      if (debugLevel > 0) fprintf(stderr, "Binding...");
      bind(listenSocket, myAddress->ai_addr, myAddress->ai_addrlen);
      if (debugLevel > 0) fprintf(stderr, "done!\n");
      //moved from main loop function..
      sendBuffer = calloc(sizeof(char), 1024);
      
      if (debugLevel > 0)
         fprintf(stderr, "Sending pack #%i to %s on port %i\n", 
            atoi(message[6] + 1), message[0], transferPort);
      
      getsockname(serverSocket, (struct sockaddr*)&myself, &addrSize);
      
      sprintf(sendBuffer, "PRIVMSG %s :\001DCC SEND \"%s\" %i %i %li\001\n",
         message[0], dirContents[atoi(message[6] + 1)].filename, 
         ntohl(myself.sin_addr.s_addr), transferPort,
         dirContents[atoi(message[6] + 1)].filesize);
      send(serverSocket, sendBuffer, strlen(sendBuffer), 0);
      //end move
      listen(listenSocket, 2);
      if (debugLevel > 0) fprintf(stderr, "Listening...");
      transferSocket = accept(listenSocket, (struct sockaddr*)&theirAddr,
                              &theirAddrSize);
      if (debugLevel > 0) fprintf(stderr, "got socket: %d\n", transferSocket);
      //now we have the socket; now we can send them the data...
      buffer = calloc(512, sizeof(char));
      inputBuffer = calloc(128, sizeof(char));
      fileToOpen = calloc(sizeof(char), 128);
      sprintf(fileToOpen, "%s/%s", directory, dirContents[packNumber].filename);
      toTransfer = fopen(fileToOpen, "rb");
      if (toTransfer == NULL)
      {
         fprintf(stderr, "Failed to open file to send!\n");
         _exit(-1);
      }
      while (filePosition < dirContents[packNumber].filesize)
      {
         fread(buffer, sizeof(char), 512, toTransfer);
         send(transferSocket, buffer, 512, 0);
         recv(transferSocket, inputBuffer, 128, 0);
         filePosition = ftell(toTransfer);
      }
      close(listenSocket);
      close(transferSocket);
      _exit(0);
   }
}

void sigchld_handler(int s)
{
   while (waitpid(-1, NULL, WNOHANG) > 0);
}

void RunMainLoop()
{
	char running = 1;
	char buffer[4096];
	char **message;
   struct sigaction sa;
	struct timeval recvTimeout;
   int bytesRecved = 0;
   time_t nextAnnounce = 0;
   time_t nextAnnounceMessage = 0;
   char doingAnnounce = 0;
   int nextPackToAnnounce = 0;
   char* announceString = 0x0;
   
   //clear out the recvTimeout struct
   memset(&recvTimeout, 0, sizeof(struct timeval));
   //we'll timeout every 10 seconds so we can run thruogh the loop to announce
   //in the channel, or do other things as needed.
   recvTimeout.tv_sec = 10;
   setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout,
              sizeof(recvTimeout));
   
	while (running)
	{
      //we need to reap the zombie processes at some point; this looks like a
      //good place to do it...
      sa.sa_handler = sigchld_handler;
      sigemptyset(&sa.sa_mask);
      sa.sa_flags = SA_RESTART;
      if (sigaction(SIGCHLD, &sa, NULL) == -1)
      {
         perror("sigaction");
         exit(1);
      }
      
		//first thing we need to do is read in the input and check if it's a
      //private message for us.
		
		//clear the buffer
		memset(buffer, 0, 4096);
      
		bytesRecved = recv(serverSocket, buffer, 4096, 0);
      
      //we don't care if we timed out or if we actually got some text from
      //the server to check if we're doing an announce...
      if (time(NULL) >= nextAnnounce)
      {
         doingAnnounce = 1;
         //next line is just to keep it from coming here again
         nextAnnounce = time(NULL) + 50000; 
         announceString = calloc(sizeof(char), 2048);
         if (debugLevel > 0) fprintf(stderr, "Doing announce!\n");
      }
      if (doingAnnounce)
      {
         //we need to reset the timeout to a lower value (only a couple seconds)
         recvTimeout.tv_sec = 2;
         setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO,
                     (char*)&recvTimeout, sizeof(recvTimeout));
         if (time(NULL) > nextAnnounceMessage)
         {
            nextAnnounceMessage += 2;
            sprintf(announceString, "PRIVMSG %s :\0034,1#\002%i\002 - %s\n", channel, 
                    nextPackToAnnounce,
                    dirContents[nextPackToAnnounce].filename);
            ++nextPackToAnnounce;
            IRC_SendMessage(announceString);
         }
         if (nextPackToAnnounce >= numSharedFiles)
         {
            nextPackToAnnounce = 0;
            doingAnnounce = 0;
            nextAnnounce = time(NULL) + 90; //next announce in 90 seconds
            nextAnnounceMessage = time(NULL) + 92;
            free(announceString);
            //restore original timeout
            recvTimeout.tv_sec = 10;
            setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO,
                        (char*)&recvTimeout, sizeof(recvTimeout));
         }
      }
      
      if (bytesRecved < 0)
      {
         //there was a timeout on the recv
         if (debugLevel > 1)
            fprintf(stderr, "Recv Timeout!\n");
      }
		else if (bytesRecved == 0)
      {
         //server closed the socket
      }
      else //there was an actual message recived...
      {
         //this will divide up the message into parts...
         message = getMessageParts(buffer);
         
         if (debugLevel > 1)
         {
            int i = 0;
            for (i = 0; i < COMMAND_TOKENS; ++i)
            {
               if (strlen(message[i]) != 0)
                  printf("message[%i] = %s\n", i, message[i]);
            }
         }
         
         //here's where we process the message
         if (strcmp(message[2], "PRIVMSG") == 0)
         {
            //make sure it'sa privmsg for us, not for the channel
            if (strcmp(message[3], nickname) == 0)
            {
               //see if it's an xdcc request
               if (strcmp(message[4], "xdcc") == 0)
               {
                  //do they want a packet?
                  if (strcmp(message[5], "send") == 0)
                  {
                     prepareTransfer(message);
                  }
               }
               //see if it's a bot request
               if (strcmp(message[4], "bot") == 0)
               {
                  //do they want me to die?
                  if (strcmp(message[5], "die") == 0)
                  {
                     if (debugLevel > 0)
                        fprintf(stderr, "Shutting down...\n");
                     running = 0;
                  }
               }
            }
         }
         if (strcmp(message[0], "PING") == 0)
         {
            char tempString[128];
            sprintf(tempString, "PONG %s\n", server);
            send(serverSocket, tempString, strlen(tempString), 0);
            if (debugLevel > 1)
               fprintf(stderr, "Responding to ping: %s\n", tempString);
         }
         
         //here's where the message is freed
         freeMessageParts(message);
      }
	}

	//if the message is for us we can parse it to see if we're going to send a
	//file (and which file to send).
	//sample:
	//:quizno50!~quizno50@157.201.73.134 PRIVMSG quizno_50 :xdcc send #111
	
	//once we know what we're going to send and everything we need to send this
	//to the IRC server: (filename should have no spaces)
	//Looks like IP is going to be some weird format, my guess is the integer
	//that is represented by the number-and-dots representation. (confirmed)
	//PRIVMSG [clientNick] :DCC SEND [filename] [ip] [port] [filesize]
	
	//here is a sample DCC SEND request (receving side):
	//:quizno50!~quizno50@403E9AD6.A12536E4.577914A5.IP PRIVMSG quizno_50 :DCC SEND bruteForceTest.c 16843009 0 2583 1 T
	
	//now we need to fork the process and run the following only in the forked
	//process:
	//we need to do some more research into the IRC DCC protocol, but here's
	//where we would setup a socket to listen for the client to connect.
	//once the client has connected; start sending the file following the
	//protocol's specification.
}

int main(int argc, char **argv)
{
	//setup from the commandline
   parseCommandline(argc, argv);
   
	//seed the random number generator
   srand(time(NULL));
	
	//set defaults for values not set
	setDefaults();
   
   //scan the directory for files to share
   DIR_Scan();
	
   //connect to the server
	IRC_Connect();
   
   //login to the server
   IRC_Login();
	
	//Here's where we need to make the main loop. A multi processing solution
	//would be awesome (something like having a forked process to send files for
	//each connected client.
	RunMainLoop();
	
   //disconnect from the server
	IRC_Disconnect();
   
   return 0;
}
