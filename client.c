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

// the port that the client will be connecting to
#define SERVER_RECEIVER_PORT "4950"
#define CLIENT_RECEIVER_PORT "3490"
#define MAXBUFLEN 100
#define MAX_USER_MESSAGE_LENGTH 1000

int sockfd;
int terminationMessageNumber = 0;
pthread_t receiverSideId;

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
// struct addrinfo* serverAddressInformation;
void* receiverSide(void* args) {
	struct addrinfo* serverAddressInformation = (struct addrinfo*)(args);
	char buffer[MAXBUFLEN];
	struct sockaddr serverSenderSocketAddress;
	int serverSenderSocketAddressLength;
	char tempInternetAddressString[INET6_ADDRSTRLEN];
	int numberOfReceivedBytes;
	struct dataSegment data;
	int j;
	
	while (1) {
		// get the packet came to receiver socket
		checkError(numberOfReceivedBytes = recvfrom(sockfd, &data, sizeof(data), 0, (struct sockaddr *)(&serverSenderSocketAddress), &serverSenderSocketAddressLength));
		// extract data
		sequenceNumber = data.acknowledgementNumber;
		acknowledgementNumber = data.sequenceNumber;
		for (j = 0; data.message[j] != '\0'; j++) buffer[j] = data.message[j];
		buffer[j] = '\0';
		while (j < MAX_USER_MESSAGE_LENGTH) buffer[j++] = '\0';

		// print the packet content
		fprintf(stdout, "%s", buffer);
	}

}

// argv[1]: server IP address or server host name
int main(int argc, char *argv[]) {
	// server address information variables
	struct addrinfo hints, *serverAddresses, *serverAddressInformation;
	char* serverIPAddress;
	char userMessage[MAX_USER_MESSAGE_LENGTH];
	int numberOfBytesSent;
	int i = 0, j, k;
	struct dataSegment data;

	checkTerminalArgumentNumberCorrect(argc, 2);
	serverIPAddress = argv[1];

	// get address information of the server
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	checkError(getaddrinfo(serverIPAddress, SERVER_RECEIVER_PORT, &hints, &serverAddresses));

	// create a proper socket that has same characteristics (IPv4, SOCK_DGRAM, UDP) with the founded address information (socket) of the server
	for (serverAddressInformation = serverAddresses; serverAddressInformation != NULL; serverAddressInformation = serverAddressInformation->ai_next) {
		// create socket
		if ((sockfd = socket(serverAddressInformation -> ai_family, serverAddressInformation -> ai_socktype, serverAddressInformation -> ai_protocol)) == -1) {
			continue;
		}
		break;
	}
	if (serverAddressInformation == NULL) {
		fprintf(stderr, "ERROR: Failed to create socket\n");
		exit(1);
	}
	// At this point, we have a socket to send data to the server

	while (1) {
		// get message from the user
		fgets(userMessage, MAX_USER_MESSAGE_LENGTH, stdin);
		if (shouldTerminate(userMessage)) return 0;
		if (userMessage[0] == '\n') continue;
		data = generateDataSegment(&sequenceNumber, &acknowledgementNumber, userMessage);

		// send user message to the server
		checkError(numberOfBytesSent = sendto(sockfd, &data, sizeof(data), 0, serverAddressInformation -> ai_addr, serverAddressInformation -> ai_addrlen));

		// if this is the first message
		if (i == 0) {
			// enable receiver side of the client
			pthread_create(&receiverSideId, 0, receiverSide, (void*) (serverAddressInformation));
			i++;
		}
	}
}