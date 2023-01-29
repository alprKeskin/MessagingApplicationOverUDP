#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

// the port users will be connecting to
#define SERVER_RECEIVER_PORT "4950"
#define CLIENT_RECEIVER_PORT "3490"
#define MAXBUFLEN 100
#define MAX_USER_MESSAGE_LENGTH 1000
int sockfd;
int terminationMessageNumber = 0;
pthread_t senderSideId;

int sequenceNumber = 1;
int acknowledgementNumber = 1;

struct dataSegment {
	int sequenceNumber;
	int acknowledgementNumber;
	char message[MAX_USER_MESSAGE_LENGTH];
};

void* get_in_addr(struct sockaddr *sa) {
	if (sa -> sa_family == AF_INET) {
		return &(((struct sockaddr_in *) sa) -> sin_addr);
	}
	return &(((struct sockaddr_in6 *) sa) -> sin6_addr);
}

void checkTerminalArgumentNumberCorrect(int givenArgumentNumber, int expectedArgumentNumber) {
	if (givenArgumentNumber != expectedArgumentNumber) {
		fprintf(stderr, "ERROR: %d argument is expected. But %d is given.\n", expectedArgumentNumber, givenArgumentNumber);
		exit(1);
	}
}

int checkError(int value) {
	if (value == -1) {
		fprintf(stderr, "ERROR!");
		exit(1);
	}
	else {
		return 1;
	}
}

int shouldTerminate(char* message) {
	// check if it is a termination message
	if (message[0] == '\n') {
		if (terminationMessageNumber == 0) {
			terminationMessageNumber++;
			return 0;
		}
		else {
			terminationMessageNumber++;
			close(sockfd);
			return 1;
		}
	}
	else {
		// reset the termination message number
		terminationMessageNumber = 0;
		return 0;
	}
}

struct dataSegment generateDataSegment(int* sequenceNumberArg, int* acknowledgementNumberArg, char* messageArg) {
	int j;
	struct dataSegment data;

	// pack data
	data.sequenceNumber = *sequenceNumberArg;
	data.acknowledgementNumber = *acknowledgementNumberArg;
	for (j = 0; messageArg[j] != '\0'; j++) data.message[j] = messageArg[j];
	data.message[j] = '\0';
	while (j < MAX_USER_MESSAGE_LENGTH) data.message[j++] = '\0';

	return data;
}


// args:
// struct sockaddr* clientSockAddrInformation;
void* senderSide(void* args) {
	int numberOfBytesSent;
	char userMessage[MAX_USER_MESSAGE_LENGTH];
	struct sockaddr* clientSockAddrInformation = (struct sockaddr *)(args);
	char strr[INET6_ADDRSTRLEN];
	struct dataSegment data;
	int j;

	while (1) {
		// get message from the user
		fgets(userMessage, MAX_USER_MESSAGE_LENGTH, stdin);
		if (shouldTerminate(userMessage)) exit(0);
		if (userMessage[0] == '\n') continue;	
		data = generateDataSegment(&sequenceNumber, &acknowledgementNumber, userMessage);

		// send user message to the client
		checkError(numberOfBytesSent = sendto(sockfd, &data, sizeof(data), 0,clientSockAddrInformation, sizeof(*clientSockAddrInformation)));
	}
}


int main(void) {
	struct addrinfo hints, *serverAddresses, *serverAddressInformation;
	struct sockaddr clientSockAddrInformation;
	int clientSockAddrInformationLength = sizeof(clientSockAddrInformation);
	int numberOfReceivedBytes;
	char buffer[MAXBUFLEN];
	char tempInternetAddressString[INET6_ADDRSTRLEN];
	int i = 0, j;
	struct dataSegment data;

	// get address information of the server to create sockets that have desired characteristics
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	checkError(getaddrinfo(NULL, SERVER_RECEIVER_PORT, &hints, &serverAddresses));

	// create a proper socket that has the characteristics (IPv4, SOCK_DGRAM, UDP) with the founded address information of the server
	for (serverAddressInformation = serverAddresses; serverAddressInformation != NULL; serverAddressInformation = serverAddressInformation -> ai_next) {
		if((sockfd = socket(serverAddressInformation -> ai_family, serverAddressInformation -> ai_socktype, serverAddressInformation -> ai_protocol)) == -1) {
			continue;
		}
		// bind the socket to assign a port number
		// port number information (SERVER_RECEIVER_PORT) is inside of the 2. argument of bind function.
		if (bind(sockfd, serverAddressInformation -> ai_addr, serverAddressInformation -> ai_addrlen) == -1) {
			continue;
		}
		break;
	}
	if (serverAddressInformation == NULL) {
		fprintf(stderr, "ERROR: Failed to create socket\n");
		exit(1);
	}
	// At this point, we have a socket to receive data from the client
	freeaddrinfo(serverAddresses);

	while (1) {
		// get message from the client
		checkError(numberOfReceivedBytes = recvfrom(sockfd, &data, sizeof(data), 0, (struct sockaddr *)(&clientSockAddrInformation), &clientSockAddrInformationLength));
		// extract data
		sequenceNumber = data.acknowledgementNumber;
		acknowledgementNumber = data.sequenceNumber;
		for (j = 0; data.message[j] != '\0'; j++) {
			buffer[j] = data.message[j];
		}
		buffer[j] = '\0';
		while (j < MAX_USER_MESSAGE_LENGTH) {
			buffer[j++] = '\0';
		}
		// At this point, we are ready to send messages to the client

		fprintf(stdout, "%s", buffer);

		// if this is the first message received
		if (i == 0) {
			// enable message sender side of the server
			pthread_create(&senderSideId, 0, senderSide, (void *)(&clientSockAddrInformation));
			i++;
		}
	}
	return 0;
}