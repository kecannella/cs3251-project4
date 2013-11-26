/*///////////////////////////////////////////////////////////
*
* FILE:     server.c
* AUTHOR:   W. Scott Johnson and Katherine Cannella
* PROJECT:  CS 3251 Project 4 - Professor Traynor
* DESCRIPTION:  Network Server Code
*
*////////////////////////////////////////////////////////////

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"

#define forClient(toPrint) "Client %s:%d: "toPrint, client->ip, client->port
#define checkStream(errmsg) \
    if (feof(client->stream)) { \
        fprintf(logfile, forClient("Fatal error while "errmsg": Connection closed unexpectedly.\n")); \
        doLeave(client); \
    } \
    if (ferror(client->stream)) { \
        fprintf(logfile, forClient("Fatal error while "errmsg": %s\n"), strerror(errno)); \
        doLeave(client); \
    }

#define INITIAL_HASHMAP_CAPACITY 1024
#define SERVER_PORT 2048
#define MAX_PENDING 10
#define LOGFILE "serverlog.txt"

FILE *logfile;

typedef struct {
    struct sockaddr_in addr;
    int socket;
    uint16_t port;
    char ip[INET_ADDRSTRLEN];
    FILE *stream;
    Hashmap *serverFiles;
    char *desiredFiles;
    uint32_t numDesiredFiles;
} ClientInfo;

void doList(ClientInfo *);
void doDiff(ClientInfo *);
void doPull(ClientInfo *);
void doCap(ClientInfo *);
void doLeave(ClientInfo *);


char getByte(FILE *stream) {
    char byte;
    fread(&byte, sizeof(char), 1, stream);
    return byte;
}

size_t sendInt(FILE *stream, int val) {
    uint32_t converted = htonl(val);
    return sizeof(converted) * fwrite(&converted, sizeof(converted), 1, stream);
}

int getInt(FILE *stream) {
    uint32_t val;
    fread(&val, sizeof(uint32_t), 1, stream);
    return ntohl(val);
}

size_t sendString(FILE *stream, char *str, size_t size) {
    return fwrite(str, 1, size, stream);
}

size_t getString(FILE *stream, char *str, size_t size) {
    return fread(str, 1, size, stream);
}

int setupServerSocket() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    exitIfError(serverSocket < 0, "creating socket");
    
    int optval = 1;
    exitIfError(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0, 
        "setting socket options");
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
    serv_addr.sin_port          = htons(SERVER_PORT);
    
    exitIfError(bind(serverSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0, "binding socket");        
    exitIfError(listen(serverSocket, MAX_PENDING) < 0, "listening for connections");        
    return serverSocket;
}

void *handleClient(void *arg) {
    ClientInfo *client = (ClientInfo *) arg;
    inet_ntop(AF_INET, &client->addr.sin_addr, client->ip, INET_ADDRSTRLEN);
    client->port = ntohs(client->addr.sin_port);
    fprintf(logfile, forClient("Connected.\n"));
    client->stream = fdopen(client->socket, "a+");
    checkStream("opening socket stream");
    client->desiredFiles = NULL;
    while (1) {
        MessageType type = getByte(client->stream);
        checkStream("getting command from client");
        switch (type) {
            case LIST:                
                doList(client);
                break;
            case DIFF:
                doDiff(client);
                break;
            case PULL:
                doPull(client);
                break;
			case CAP:
				doCap(client);
				break;
            case LEAVE:
                doLeave(client);
                break;
            default:
                fprintf(logfile, forClient("Got unknown command. Forcing Disconnect.\n"));
                doLeave(client);
        }
    }    
}

void doList(ClientInfo *client) {
    fprintf(logfile, forClient("Getting list of files.\n"));
    sendInt(client->stream, client->serverFiles->load);
    checkStream("sending list length");
    size_t i;
    char *key;
    forKeyInHashmap(client->serverFiles, i, key) {
        fwrite(key, 1, HASH_LENGTH, client->stream);
        checkStream("sending list");
        char *filename = getFromHashmap(client->serverFiles, key);
        if (filename != NULL) {        
            uint32_t namelen = strlen(filename);
            sendInt(client->stream, namelen);
            checkStream("sending filename length during List");
            sendString(client->stream, filename, namelen);
            checkStream("sending filename during List");
		}
    }  
    fflush(client->stream);
    checkStream("flushing list");
}

void doDiff(ClientInfo *client) {
    fprintf(logfile, forClient("Sending list of desired files.\n"));
    uint32_t entries = getInt(client->stream);
    checkStream("getting number of desired files");
    if (entries > client->serverFiles->load) {
        fprintf(logfile, forClient("Asking for too many files! Forcing disconnect.\n"));
        doLeave(client);
    }
    if (entries > 0) {
        client->desiredFiles = realloc(client->desiredFiles, HASH_LENGTH * entries);
        if (client->desiredFiles == NULL) {
            fprintf(logfile, forClient("Fatal error while realloc'ing desiredFiles: %s\n"), strerror(errno));
            doLeave(client);
        }
        getString(client->stream, client->desiredFiles, HASH_LENGTH * entries);
        checkStream("getting diff file hash");
        client->numDesiredFiles = entries;
    }
}

uint32_t sendFile(ClientInfo *client, char *filename) {
    if (filename == NULL) {
        return;
    }        
    uint32_t bytesSent = 0;
    uint32_t namelen = strlen(filename);
    bytesSent += sendInt(client->stream, namelen);
    checkStream("sending filename length during Pull");
    bytesSent += sendString(client->stream, filename, namelen);
    checkStream("sending filename during Pull");
    
    FILE *file = fopen(filename, "r");
    if (ferror(client->stream)) {
        fprintf(logfile, forClient("Fatal error while opening file during Pull: %s\n"), strerror(errno));
        doLeave(client);
    }
    struct stat st;
    if (stat(filename, &st) == -1) {                    
        fprintf(logfile, forClient("Fatal error while getting filesize during Pull: %s\n"), strerror(errno));
        doLeave(client);
    }
    uint32_t filesize = st.st_size;    
    bytesSent += sendInt(client->stream, filesize);
    checkStream("sending filesize during Pull");

    int bytes;
    bytesSent = 0;
    unsigned char data[8192];
    while ((bytes = getString(file, data, 8192)) != 0) {
        bytesSent += sendString(client->stream, data, bytes);
        checkStream("sending file during Pull");
    }
    if (fclose(file) == EOF) {
        fprintf(logfile, forClient("Fatal error while closing file during Pull: %s\n"), strerror(errno));
        doLeave(client);
    }
    return bytesSent;
}

void doPull(ClientInfo *client) {
    if (client->desiredFiles == NULL) {
        fprintf(logfile, forClient("Pulling files before sending a list of desired files! Forcing disconnect.\n"));
        doLeave(client);
    }
    fprintf(logfile, forClient("Pulling desired files.\n"));
    int i;
    for (i = 0; i < client->numDesiredFiles; i++) {
        char *filename = getFromHashmap(client->serverFiles, client->desiredFiles + (i * HASH_LENGTH));
        sendFile(client, filename);   
    }
    fflush(client->stream);
    checkStream("flushing files during Pull");
}

void doCap(ClientInfo *client) {
    uint32_t cap;
    fread(&cap, sizeof(cap), 1, client->stream);
    uint32_t sent = 0;
    
	//TODO open xml file - sort by play count??
	priority_list *list = newPriorityList();
    DIR *directory = opendir(".");
	createPriorityIndex(list,directory,client->desiredFiles);
    while (sent < cap) {	
	    char *filename = getFromHashmap(client->serverFiles, (char *)getFromPriorityList(list)); // TODO read xml file, get next best song
	    uint32_t bytesToSend = 0;
	    bytesToSend = strlen(filename) + 3*sizeof(uint32_t); // need to send length of filename, filename, size of file, and (potentially) endOfList
	    struct stat st;
        if (stat(filename, &st) == -1) {                    
            fprintf(logfile, forClient("Fatal error while getting filesize during Pull: %s\n"), strerror(errno));
            doLeave(client);
        }
        bytesToSend += st.st_size; // need to send entire file
        if (sent + bytesToSend > cap) {
            break;
        }
	    sent += sendFile(client, filename);
    }
    freePriorityList(list, NULL);
    uint32_t endOfList = 0;
    fwrite(&endOfList, sizeof(endOfList), 1, client->stream);
}

void doLeave(ClientInfo *client) {
    fclose(client->stream);
    fprintf(logfile, forClient("Disconnected.\n"));
    free(client->desiredFiles);
    free(client);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    logfile = stdout; //fopen(LOGFILE, "w");
    //setvbuf(logfile, NULL, _IONBF, 0);
    
    OpenSSL_add_all_digests(); 
    fprintf(stdout, "Creating Music Index...\n");
    Hashmap *filemap = newHashmap(INITIAL_HASHMAP_CAPACITY);
    chdir("./Music");
    DIR *directory = opendir(".");
    createIndex(filemap, directory);
    
    fprintf(stdout, "Starting Server...\n");
    int serverSocket = setupServerSocket();
    pthread_t thread;
    pthread_attr_t threadAttributes;
    pthread_attr_init(&threadAttributes);
    pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_DETACHED);
    
    fprintf(stdout, "Server started. Listening for connections...\n");
    while(1) {
        ClientInfo *client = malloc(sizeof(ClientInfo));
        exitIfError(client == NULL, "mallocing client");
        client->serverFiles = filemap;
        socklen_t addrlen;
        addrlen = sizeof(client->addr);
        client->socket = accept(serverSocket, (struct sockaddr *) &client->addr, &addrlen);
        if (client->socket < 0) {
            fprintf(logfile, "Error while accepting connection: %s\n", strerror(errno));
            free(client);     
        } else {
            pthread_create(&thread, &threadAttributes, handleClient, client);
        }
    }
    // should never get here
    freeHashmap(filemap, free, NULL);
    closedir(directory);
    fclose(logfile);
    EVP_cleanup();
    return EXIT_SUCCESS;
}
