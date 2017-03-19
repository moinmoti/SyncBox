#include <bits/stdc++.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <openssl/md5.h>
#include <sys/mman.h>
#include <regex>
#include <time.h>

using namespace std;

#define PORT "8080"
#define MAXDATASIZE 1000

typedef struct socketData {
    int sockfd;
    char buffer[MAXDATASIZE+1];
} socketData;

typedef struct filedata {
    string filename;
    string permissions;
    string mtime;
    string type;
    unsigned char md5str[MD5_DIGEST_LENGTH];
    struct stat buffer;
} filedata;

map <string, filedata> files;
map <string, filedata> prevFiles;
map <string, filedata> uploadQueue;
map <string, filedata> modifyQueue;
map <string, filedata> clientFiles;
vector <pair<string, string> > userCommand;
vector <pair<string, string> > clientRequest;
bool handlerFlag;
bool updateClient;
bool autosync;

void sigchld_handler(int s) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}


void *recv_handler(void *args) {
    struct socketData clientData = *((struct socketData*)args);
    pair<string, string> command;
    while (handlerFlag) {
        try {
            recv(clientData.sockfd, clientData.buffer, MAXDATASIZE, 0);
            command.first = clientData.buffer;
            recv(clientData.sockfd, clientData.buffer, MAXDATASIZE, 0);
            command.second = clientData.buffer;
            clientRequest.push_back(command);
        } catch (...) {
            perror("recv");
        }
    }
    return NULL;
}


void update_dir_info(string dirpath) {
    DIR *dir = opendir(dirpath.c_str());
    files.clear();
        for (auto dp = readdir(dir); dp != NULL; dp = readdir(dir)) {
            struct stat statbuf;
            string filename;
            string permissions;
            string modified_time;
            string type;
            string hash;
            stat(dp->d_name, &statbuf);
            filename = dp->d_name;
            modified_time = to_string(statbuf.st_mtime);
            type = to_string(statbuf.st_mode);
            permissions = (statbuf.st_mode & S_IRUSR) ? "r" : "-";
            permissions += (statbuf.st_mode & S_IWUSR) ? "w" : "-";
            permissions += (statbuf.st_mode & S_IXUSR) ? "x" : "-";
            permissions += (statbuf.st_mode & S_IRGRP) ? "r" : "-";
            permissions += (statbuf.st_mode & S_IWGRP) ? "w" : "-";
            permissions += (statbuf.st_mode & S_IXGRP) ? "x" : "-";
            permissions += (statbuf.st_mode & S_IROTH) ? "r" : "-";
            permissions += (statbuf.st_mode & S_IWOTH) ? "w" : "-";
            permissions += (statbuf.st_mode & S_IXOTH) ? "x" : "-";
            filedata file = {};
            file.filename = filename;
            file.permissions = permissions;
            file.mtime = modified_time;
            file.type = type;
            file.buffer = statbuf;
            string fileAddr = dirpath+filename;
            unsigned long fileSize = statbuf.st_size;
            // unsigned char md5str[MD5_DIGEST_LENGTH];
            void * fileBuffer;
            int fileDes;
            if ((fileDes = open(fileAddr.c_str(), O_RDONLY, 0)) < 0) perror("read: ");
            fileBuffer = mmap(0, fileSize, PROT_READ, MAP_SHARED, fileDes, 0);
            MD5((unsigned char *)fileBuffer, fileSize, file.md5str);
            munmap(fileBuffer, fileSize);
            files[filename] = file;
            close(fileDes);
        }
        if (!prevFiles.empty()) {
            for (auto i = files.begin(); i != files.end(); i++) {
                auto j = prevFiles.find(i->first);
                if (j == prevFiles.end()) {
                    updateClient = 1;
                    uploadQueue.insert(*i);
                } else {
                    if ((j->second).md5str != (i->second).md5str) {
                        updateClient = 1;
                        modifyQueue.insert(*i);
                    }
                }
            }
        }
        prevFiles = files;
    closedir(dir);
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void *prompt_handler(void *args) {
    while (handlerFlag) {
        cout << "prompt > ";
        pair <string, string> command;
        cin >> command.first >> command.second;
        userCommand.push_back(command);
    }
    return NULL;
}


int main(int argc, char const *argv[]) {
    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo); // all done with this structure
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(1);
    }
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    printf("server: waiting for connections...\n");
    time_t current;
    time_t lastUpdated;
    while(1) { // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);
        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            handlerFlag = 1; // permit thread execution
            pthread_t promptThread;
            pthread_create(&promptThread, NULL, prompt_handler, NULL);
            struct socketData clientData;
            clientData.sockfd = new_fd;
            pthread_t recvThread;
            pthread_create(&recvThread, NULL, recv_handler, &clientData);
            string dirpath = "shared_server/";
            update_dir_info(dirpath);
            time(&lastUpdated);
            updateClient = 1;
            while (1) {
                time(&current);
                if (current - lastUpdated < 3) {
                    update_dir_info(dirpath);
                    time(&lastUpdated);
                }
                if (updateClient) {
                    if (!uploadQueue.empty()) {
                        // handle uplaods to client
                        string msg = "new files created on server";
                        if (send(new_fd, msg.c_str(), msg.size(), 0) == -1) perror("send: ");
                        msg = to_string(uploadQueue.size());
                        if (send(new_fd, msg.c_str(), msg.size(), 0) == -1) perror("send: ");
                        for (auto i = uploadQueue.begin(); i !=  uploadQueue.end(); i++) {
                            auto file = i->second;
                            if (send(new_fd, file.filename.c_str(), file.filename.size(), 0) == -1) perror("send: ");
                            if (send(new_fd, file.permissions.c_str(), file.permissions.size(), 0) == -1) perror("send: ");
                            if (send(new_fd, file.mtime.c_str(), file.mtime.size(), 0) == -1) perror("send: ");
                            if (send(new_fd, file.type.c_str(), file.type.size(), 0) == -1) perror("send: ");
                            if (send(new_fd, file.md5str, strlen((char*)file.md5str), 0) == -1) perror("send: ");
                        }
                    }
                    if (modifyQueue.empty()) {
                        // handle modification of files for client
                        string msg = "some files modified on server";
                        if (send(new_fd, msg.c_str(), msg.size(), 0) == -1) perror("send: ");
                        msg = to_string(modifyQueue.size());
                        if (send(new_fd, msg.c_str(), msg.size(), 0) == -1) perror("send: ");
                        for (auto i = modifyQueue.begin(); i !=  modifyQueue.end(); i++) {
                            auto file = i->second;
                            if (send(new_fd, file.filename.c_str(), file.filename.size(), 0) == -1) perror("send: ");
                            if (send(new_fd, file.permissions.c_str(), file.permissions.size(), 0) == -1) perror("send: ");
                            if (send(new_fd, file.mtime.c_str(), file.mtime.size(), 0) == -1) perror("send: ");
                            if (send(new_fd, file.type.c_str(), file.type.size(), 0) == -1) perror("send: ");
                            if (send(new_fd, file.md5str, strlen((char*)file.md5str), 0) == -1) perror("send: ");
                        }
                    }
                }

                if (!userCommand.empty()) {
                    // send the client, user request
                }

                if (!clientRequest.empty()) {
                    // handle client request
                }
                
                // if (send(new_fd, "Hello, world!", 13, 0) == -1) perror("send");
            }
            handlerFlag = 0;            
            close(new_fd);
            exit(0);
        }
        close(new_fd); // parent doesn't need this
    }
    return 0;
}
