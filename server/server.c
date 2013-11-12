/*///////////////////////////////////////////////////////////
*
* FILE:     server.c
* AUTHOR:   W. Scott Johnson and Katherine Cannella
* PROJECT:  CS 3251 Project 2 - Professor Traynor
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

void killSignalHandler(int signo) {
    if (logfile != stdout) {
        fclose(logfile);
    }
    exit(EXIT_SUCCESS);
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
        MessageType type;
        fread(&type, sizeof(MessageType), 1, client->stream);
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
                fprintf(logfile, forClient("Got unknown command. Forcing Disconnect.\n"), client->ip, client->port);
                doLeave(client);
        }
    }    
}

void doList(ClientInfo *client) {
    fprintf(logfile, forClient("Getting list of files.\n"));
    uint32_t numFiles = client->serverFiles->load;
    fwrite(&numFiles, sizeof(numFiles), 1, client->stream);
    checkStream("sending list length");
    size_t i;
    char *key;
    forKeyInHashmap(client->serverFiles, i, key) {
        fwrite(key, 1, HASH_LENGTH, client->stream);
        checkStream("sending hash during List");
        char *filename = getFromHashmap(client->serverFiles, key);
        if (filename != NULL) {        
            uint32_t namelen = strlen(filename);
            fwrite(&namelen, sizeof(namelen), 1, client->stream);
            checkStream("sending filename length during List");
            fwrite(filename, 1, namelen, client->stream);
            checkStream("sending filename during List");
		}
    }  
    fflush(client->stream);
    checkStream("flushing list");
}

void doDiff(ClientInfo *client) {
    fprintf(logfile, forClient("Sending list of desired files.\n"));
    uint32_t entries;
    fread(&entries, sizeof(entries), 1, client->stream);
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
        fread(client->desiredFiles, HASH_LENGTH, entries, client->stream);
        checkStream("getting diff file hash");
        client->numDesiredFiles = entries;
    }
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
        if (filename != NULL) {        
            uint32_t namelen = strlen(filename);
            fwrite(&namelen, sizeof(namelen), 1, client->stream);
            checkStream("sending filename length during Pull");
            fwrite(filename, 1, namelen, client->stream);
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
            fwrite(&filesize, sizeof(filesize), 1, client->stream);
            checkStream("sending filesize during Pull");
            int bytes;
            unsigned char data[4096];
            while ((bytes = fread(data, 1, 4096, file)) != 0) {
                fwrite(data, 1, bytes, client->stream);
                checkStream("sending file during Pull");
            }
            if (fclose(file) == EOF) {
                fprintf(logfile, forClient("Fatal error while closing file during Pull: %s\n"), strerror(errno));
                doLeave(client);
            }
        }           
    }
    fflush(client->stream);
    checkStream("flushing files during Pull");
}

void doCap(Client Info *client) {
	// TODO choose list of files
	// TODO send number of files
	// TODO for each file send
	//	- filename size
	//	- filename
	//	- file size
	//	- file (4096 byte chunks)
}

void doLeave(ClientInfo *client) {
    fclose(client->stream);
    fprintf(logfile, forClient("Disconnected.\n"));
    free(client->desiredFiles);
    free(client);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (signal(SIGINT, killSignalHandler) == SIG_ERR) {
        printf("Can't catch SIGINT, so logging to stdout instead.\n");
        logfile = stdout;
    } else {    
        logfile = fopen(LOGFILE, "w");
    }
   
    OpenSSL_add_all_digests(); 
    fprintf(logfile, "Creating Music Index...\n");
    Hashmap *filemap = newHashmap(INITIAL_HASHMAP_CAPACITY);
    chdir("./Music");
    DIR *directory = opendir(".");
    createIndex(filemap, directory);
    
    fprintf(logfile, "Starting Server...\n");
    int serverSocket = setupServerSocket();
    pthread_t thread;
    pthread_attr_t threadAttributes;
    pthread_attr_init(&threadAttributes);
    pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_DETACHED);
    
    fprintf(logfile, "Server started. Listening for connections...\n");
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
}
