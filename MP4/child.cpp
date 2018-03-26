#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <queue>
#include <deque>
#include <utility>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <climits>

#include "protocol.hpp"

using namespace std;

int match(int child_id) {
    fprintf(stderr, "Server: Match Process %d\n", getpid());
    string match_in_pipe = "./fifo/match_in" + to_string(child_id);
    int output_match = open(match_in_pipe.c_str(), O_WRONLY);
    string match_out_pipe = "./fifo/match_out" + to_string(child_id);
    int input_match = open(match_out_pipe.c_str(), O_RDONLY);

    fd_set readset;
    fd_set working_readset;
    FD_ZERO(&readset);
    FD_SET(input_match, &readset);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;

    int MAX_FD = input_match;

    while(true) {
        memcpy(&working_readset, &readset, sizeof(fd_set));
        int retval = select(MAX_FD+1, &working_readset, NULL, NULL, &timeout);
        //if(retval >0) fprintf(stderr, "fuck\n");
        if(FD_ISSET(input_match, &working_readset)) {
            struct User_Data match;
            memset(&match, 0, sizeof(struct User_Data));
            read(input_match, &match, sizeof(struct User_Data));
            //fprintf(stderr, "TO MATCH : %d\n", match.fd);
            // DLOPEN
            void* handle = dlopen(match.filter_path, RTLD_LAZY);
            assert(NULL != handle);
            dlerror();
            int (*filter)(struct User) = (int (*)(struct User)) dlsym(handle, "filter_function");
            const char *dlsym_error = dlerror();
            if (dlsym_error) {
                //fprintf(stderr, "Child %d: Cannot load symbol 'filter': %s\n", child_id, dlsym_error);
                dlclose(handle);
                return 1;
            }
            while(true) {
                struct User_Data user_data;
                memset(&user_data, 0, sizeof(struct User_Data));
                read(input_match, &user_data, sizeof(struct User_Data));
                if(user_data.type == EXIT) break;
                
                //fprintf(stderr, "match %d and %d pid: %d\n", match.fd, user_data.fd, getpid());
                int match_result = filter(user_data.user);
                //fprintf(stderr, "NOT FUCKED YET!! %d %d pid : %d\n", match.fd, user_data.fd, getpid());
                if(match_result != 1) {
                    int fail = -1;
                    write(output_match, &fail, sizeof(int));
                    continue;
                }

                // dlopen
                void* current_handle = dlopen(user_data.filter_path, RTLD_LAZY);
                assert(NULL != current_handle);

                dlerror();
                int (*user_filter)(struct User) = (int (*)(struct User)) dlsym(current_handle, "filter_function");
                const char *dlsym_error = dlerror();
                if (dlsym_error) {
                    //fprintf(stderr, "Child %d: Cannot load symbol 'filter': %s\n", i, dlsym_error);
                    dlclose(current_handle);
                    return 1;
                }
                match_result = user_filter(match.user);
                //fprintf(stderr, "NOT FUCKED YET222!! %d %d\n", user_data.fd, match.fd);
                if(match_result == 1) {
                    //fprintf(stderr, "%d match %d!!\n", match.fd, user_data.fd);
                    size_t size = write(output_match, &match_result, sizeof(int));
                    //fprintf(stderr, "WRITE IN SIZE: %lld\n", size);
                }
                else {
                    int fail = -1;
                    write(output_match, &fail, sizeof(int));
                }
                dlclose(current_handle);
            }
            dlclose(handle);
        }
    }
    return 1;
}

int child_process(int child_id) {
    string match_in_pipe = "./fifo/match_in" + to_string(child_id);
    remove(match_in_pipe.c_str());
    int ret = mkfifo(match_in_pipe.c_str(), 0644);
    assert(!ret);
    string match_out_pipe = "./fifo/match_out" + to_string(child_id);
    remove(match_out_pipe.c_str());
    ret = mkfifo(match_out_pipe.c_str(), 0644);
    assert(!ret);
    
    pid_t match_pid = fork();
    if(match_pid == 0) {
        match(child_id);
        exit(0);
    }
    fprintf(stderr, "Child %d: START\n", child_id);
    string in_pipe = "./fifo/in" + to_string(child_id);
    int input_pipe = open(in_pipe.c_str(), O_RDONLY);
    string out_pipe = "./fifo/out" + to_string(child_id);
    int output_pipe = open(out_pipe.c_str(), O_WRONLY);
    
    // communicate with match child process

    int input_match = open(match_in_pipe.c_str(), O_RDONLY);
    int output_match = open(match_out_pipe.c_str(), O_WRONLY);

    fd_set readset;
    fd_set working_readset;
    FD_ZERO(&readset);
    FD_SET(input_pipe, &readset);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;

    int MAX_FD = input_pipe;
    vector<struct User_Data> user_queue;
    queue<struct User_Data> match_queue;
    //queue<pair<int, vector<int>  > > matched;
    // for filter function
    int (*filter)(struct User);
    void* current_handle;

    bool isDone[2000];
    memset(isDone, true, sizeof(isDone));
    while(true) {
        memcpy(&working_readset, &readset, sizeof(fd_set));
        int retval = select(MAX_FD+1, &working_readset, NULL, NULL, &timeout);

        if(match_queue.size() > 0) {
            struct User_Data match_data = match_queue.front();
            match_queue.pop();
            vector<int> matched_user;
            if(!isDone[match_data.fd]) {
                // write to match pipe
                write(output_match, &match_data, sizeof(struct User_Data));

                for(int i = 0; i < user_queue.size(); ++i) {
                    if(match_data.fd == user_queue[i].fd) break;
                    else if(user_queue[i].fd == -1) continue;
                    struct User_Data user_data = user_queue[i];
                    if(isDone[user_data.fd]) continue;

                    write(output_match, &user_data, sizeof(struct User_Data));
                    int match_result = 0;

                    fd_set matchset;
                    fd_set working_matchset;
                    FD_ZERO(&matchset);
                    FD_SET(input_match, &matchset);

                    struct timeval timeout;
                    timeout.tv_sec = 0;
                    timeout.tv_usec = 200000;
                    
                    while(true) {
                        memcpy(&working_matchset, &matchset, sizeof(fd_set));
                        int retval = select(input_match+1, &working_matchset, NULL, NULL, &timeout);

                        // restore match child process
                        int status;
                        pid_t ret = waitpid(match_pid, &status, WNOHANG);
                        //if(ret != 0) fprintf(stderr, "RET: %d\n", ret);
                        if(ret == match_pid) {
                            fprintf(stderr, "Server: match %d is fucked up!! SIGNAL: %d %d %d\n", match_pid, WIFSIGNALED(status), WTERMSIG(status), WCOREDUMP(status));
                            close(input_match);
                            close(output_match);
                            remove(match_in_pipe.c_str());
                            ret = mkfifo(match_in_pipe.c_str(), 0644);
                            assert(!ret);
                            
                            remove(match_out_pipe.c_str());
                            ret = mkfifo(match_out_pipe.c_str(), 0644);
                            assert(!ret);
                            
                            //fprintf(stderr, "DOING %d and %d\n", match_data.fd, user_data.fd);

                            if((match_pid = fork()) == 0) {
                                //fprintf(stderr, "new match pid: %d\n", getpid());
                                match(child_id);
                                exit(0);
                            }
                            input_match = open(match_in_pipe.c_str(), O_RDONLY);
                            output_match = open(match_out_pipe.c_str(), O_WRONLY);

                            write(output_match, &match_data, sizeof(struct User_Data));
                            break;
                        }
                        if(retval > 0 && FD_ISSET(input_match, &working_matchset)) {
                            size_t size = read(input_match, &match_result, sizeof(int));
                            //fprintf(stderr, "%d\n", match_result);
                            
                            if(size > 0) {
                                //if(match_result == -1) fprintf(stderr, "%d and %d don't match\n", match_data.fd, user_data.fd);
                                break;
                            }
                        }
                    }
                    //fprintf(stderr, "Child %d: match result: %d\n", child_id, match_result);
                    if(match_result == 1) {
                        matched_user.push_back(user_data.fd);
                        //fprintf(stderr, "Child %d: %d currently match %d!!\n", child_id, match_data.fd, user_data.fd);
                    }
                    //dlclose(current_handle);
                }
                struct User_Data stop;
                memset(&stop, 0, sizeof(struct User_Data));
                stop.type = EXIT;
                write(output_match, &stop, sizeof(struct User_Data));
                //dlclose(handle);
            }
            struct Match match_pkt;
            memset(&match_pkt, 0, sizeof(struct Match));
            if (matched_user.size() > 0) {
                match_pkt.type = SEND_MATCH;
                match_pkt.fd = match_data.fd;
                int pair_num = matched_user.size();
                write(output_pipe, &pair_num, sizeof(int));
                for(auto match_user : matched_user) {
                    match_pkt.matched_fd = match_user;
                    write(output_pipe, &match_pkt, sizeof(struct Match));
                }
            }
            else {
                //fprintf(stderr, "Child %d: NO Match for %d, Put it to the back of queue\n", child_id, match.fd);
                match_pkt.type = NO_MATCH;
                match_pkt.fd = 0;
                match_pkt.matched_fd = 0;
                int pair_num = 1;
                write(output_pipe, &pair_num, sizeof(int));
                write(output_pipe, &match_pkt, sizeof(struct Match));
            }
        }
        if(FD_ISSET(input_pipe, &working_readset)) {
            struct Header header;
            struct User user;
            struct User_Data user_data;
            memset(&header, 0, sizeof(struct Header));
            memset(&user, 0, sizeof(struct User));
            memset(&user_data, 0, sizeof(struct User_Data));
            memset(&(user_data.user), 0, sizeof(struct User));
            read(input_pipe, &header, sizeof(struct Header));
            
            // Server is going to close
            if(header.type == EXIT) break;
            
            // delete matched user
            if(header.type == DELETE) {
                isDone[header.fd] = true;
                isDone[header.matched_fd] = true;
                //fprintf(stderr, "Child %d: %d and %d has been matched\nDelete them from queue\n", child_id, header.fd,header.matched_fd);
                for(int i = 0; i < user_queue.size(); ++i) {
                    if(user_queue[i].fd == header.fd || user_queue[i].fd == header.matched_fd) 
                        user_queue[i].fd = -1;
                    //fprintf(stderr, "Child %d IN QUEUE: %d\n", child_id, user_queue[i].fd);
                }
                continue;
            }
            // new User
            isDone[header.fd] = false;
            /*if(header.type == TO_MATCH) {
                //fprintf(stderr, "Child %d: new user %d comes in\n", child_id, header.fd);
                //fprintf(stderr, "Current Queue:\n");
                for(int i = 0; i < user_queue.size(); ++i) {
                    if(!isDone[user_queue[i].fd]) fprintf(stderr, "%d ", user_queue[i].fd);
                }
                fprintf(stderr, "\n");
            }*/
            read(input_pipe, &user, sizeof(struct User));
            //fprintf(stderr, "Child %d: Intro: %s\n", child_id, user.introduction);

            user_data.type = header.type;
            user_data.fd = header.fd;
            memcpy(user_data.filter_path, header.filter_path, strlen(header.filter_path));
            memcpy(&(user_data.filter_path[strlen(header.filter_path)]), ".so", 3);
            user_data.filter_path[strlen(header.filter_path) + 3] = '\0';
            memcpy(&(user_data.user), &user, sizeof(struct User));
            
            user_queue.push_back(user_data);
            // waiting for compile
            // for matching
            if(header.type == TO_MATCH) {
                match_queue.push(user_data);
            }
        }
    }
    close(input_pipe);
    close(output_pipe);
    return 0;
}

int compile(int fd) {
    fprintf(stderr, "Child %d: START\n", fd);
    string pipe = "./fifo/in" + to_string(fd);
    int input_pipe = open(pipe.c_str(), O_RDWR);

    if(input_pipe <= 0) {
        fprintf(stderr, "Child %d: FUCK OPEN PIPE ERROR\n", fd);
    }

    pipe = "./fifo/out" + to_string(fd);
    int output_pipe = open(pipe.c_str(), O_RDWR);

    if(output_pipe <= 0) {
        fprintf(stderr, "Child %d: FUCK OPEN PIPE ERROR\n", fd);
    }

    fd_set readset;
    fd_set working_readset;
    FD_ZERO(&readset);
    FD_SET(input_pipe, &readset);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;

    int MAX_FD = input_pipe;

    while(true) {
        memcpy(&working_readset, &readset, sizeof(fd_set));
        int retval = select(MAX_FD+1, &working_readset, NULL, NULL, &timeout);
        if(FD_ISSET(input_pipe, &working_readset)) {
            struct Compile compile;
            memset(&compile, 0, sizeof(struct Compile));

            read(input_pipe, &compile, sizeof(struct Compile));
            if(compile.type == EXIT) break;
            else if(compile.type != TO_MATCH) continue;
            std::string filter_path = compile.filter_path;
            char comp[1000];
            sprintf(comp, "gcc -fPIC -O2 -std=c11 %s.c -shared -o %s.so", filter_path.c_str(), filter_path.c_str());
            fprintf(stderr, "Child %d: COMPILE TARGET: %s\n", fd, comp);
            int ret = system(comp);
            fprintf(stderr, "Child %d: COMPILE RESULT: %d\n", fd, ret);
        }
    }
    close(input_pipe);
    close(output_pipe);
    return 0;
}