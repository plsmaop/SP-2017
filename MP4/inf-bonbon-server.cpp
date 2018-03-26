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

#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <queue>
#include <deque>
#include <utility>

#include "protocol.hpp"
#include "json.hpp"
#include "child.hpp"
using json = nlohmann::json;
using namespace std;

int handle_command() {
    char buf[100];
    size_t size = read(0, buf, 100);
    buf[size] = '\0';
    if(memcmp(buf, "suck", 4) == 0) {
        fprintf(stderr, "Server: User Command: SUCK!!\n");
        return 1;
    }
    else fprintf(stderr, "Server: FUCK YOU!!\n");
    return 0;
}

std::string compile(json &tmp_json, int fd) {
    string filter = "struct User {char name[33];unsigned int age;char gender[7];char introduction[1025];};";
    filter+=tmp_json["filter_function"];
    string filter_path = "./filter/" + tmp_json["name"].get<std::string>() + "_" + to_string(fd);
    int filter_fd = open((filter_path+".c").c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);
    size_t size = write(filter_fd, filter.c_str(), filter.length());
    if(size != filter.length()) {
        perror("Server: Write Filter Function Error:");
        exit(errno);
    }
    char comp[300];
    sprintf(comp, "gcc -fPIC -O2 -std=c11 %s.c -shared -o %s.so", filter_path.c_str(), filter_path.c_str());
    int retval = system(comp);
    //fprintf(stderr, "Server: Compile result: %d\n", retval);
    close(filter_fd);
    return filter_path;
}

bool handle_msg(char* msg, char* tmp, size_t &tmp_size) {
    int msg_index = 0;
    while(true) {
        if(tmp[msg_index] == '\n' || tmp[msg_index] == '\0') break;
        else msg_index++;
    }
    // it is a json
    if(tmp[msg_index] == '\n') {
        tmp[msg_index] = '\0';
        memcpy(msg, tmp, msg_index+1);
        memcpy(tmp, &tmp[msg_index + 1], tmp_size - msg_index - 1);
        tmp_size = tmp_size - msg_index - 1;
        if(tmp_size < 0) tmp_size = 0;
        tmp[tmp_size] = '\0';
        return true;
    }
    return false;
}
int update_queue(struct Child *child, int fd1, int fd2) {
    struct Header header;
    memset(&header, 0, sizeof(struct Header));
    header.type = DELETE;
    header.fd = fd1;
    header.matched_fd = fd2;
    for(int i = 0; i < CHILD_NUM; ++i) write(child->write_fd[i], &header, sizeof(struct Header));
}

int send_match(json &tmp_json, char *json_string) {
    json new_json = tmp_json;
    new_json["cmd"] = "matched";
    std::string j_string = new_json.dump();
    memcpy(json_string, j_string.c_str(), j_string.length());
    json_string[j_string.length()] = '\n';
    json_string[j_string.length()+1] = '\0';
    //fprintf(stderr, "Server: matched json:\n%s\n", json_string);
}

int parse_json(struct User *tmp_user, json tmp_json) {
    // name
    size_t name_size = tmp_json["name"].get<std::string>().length();
    memcpy(tmp_user->name, tmp_json["name"].get<std::string>().c_str(), name_size);
    tmp_user->name[name_size] = '\0';
                        
    // gender
    size_t gender_size = tmp_json["gender"].get<std::string>().length();
    memcpy(tmp_user->gender, tmp_json["gender"].get<std::string>().c_str(), gender_size);
    tmp_user->gender[gender_size] = '\0';

    // introduction
    size_t intro_size = tmp_json["introduction"].get<std::string>().length();
    memcpy(tmp_user->introduction, tmp_json["introduction"].get<std::string>().c_str(), intro_size);
    tmp_user->introduction[intro_size] = '\0';

    // age
    tmp_user->age = tmp_json["age"];
}

int make_fifo() {
    remove("./fifo");
    remove("./filter");
    mkdir("./fifo", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("./filter", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    for(int i = 0; i < MAX_CHILD_NUM; ++i) {
        string pipe = "./fifo/in" + to_string(i);
        remove(pipe.c_str());
        int ret = mkfifo(pipe.c_str(), 0644);
        assert(!ret);

        pipe = "./fifo/out" + to_string(i);
        remove(pipe.c_str());
        ret = mkfifo(pipe.c_str(), 0644);
        assert(!ret);
    }
}

int main(int argc, char **argv) {
    make_fifo();

    // 1 2 3 4 for try and match
    // 5 for compile
    struct Child child;
    memset(&child, 0, sizeof(struct Child));
    child.current_process = 0;
    child.matched = 0;
    pid_t pid;
    for(int i = 0; i < CHILD_NUM; i++) {
        pid = fork();
        if( pid == 0) {
            /*if(i == CHILD_NUM ) {
                fprintf(stderr, "Compiler %d's pid: %d\n", i, getpid());
                compile(i);
                exit(0);
            }
            else {*/
                fprintf(stderr, "Child %d: pid: %d\n", i, getpid());
                
                child_process(i);
                exit(0);
            //}
        } 
        child.pid[i] = pid;
    }
    // socket
    int port = stoi(argv[1]);
    fprintf(stderr, "Server: bind port: %d\n", port);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        perror("Server: SockFD Error:");
        exit(errno);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // bind
    int retval = bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if(retval == -1) {
        printf("socket fail\n");
        exit(1);
    }

    int MAX_FD = sockfd;
    retval = listen(sockfd, 1000);
    if(retval == -1) {
        perror("Server: Listen Error");
        exit(errno);
    }
    // FD
    fd_set readset;
    fd_set working_readset;
    FD_ZERO(&readset);
    FD_ZERO(&working_readset);
    FD_SET(sockfd, &readset);
    FD_SET(0, &readset);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    
    bool fd_queue[1200];
    memset(fd_queue, 0, sizeof(fd_queue));
    fd_queue[sockfd] = true;

    // child name pipe
    sleep(1);
    for(int i = 0; i < CHILD_NUM; i++) {
        string pipe = "./fifo/in" + to_string(i);
        int input_pipe = open(pipe.c_str(), O_WRONLY);
        child.write_fd[i] = input_pipe;
        pipe = "./fifo/out" + to_string(i);
        int output_pipe = open(pipe.c_str(), O_RDONLY);
        child.read_fd[i] = output_pipe;
        child.state[i] = true;
        fd_queue[output_pipe] = true;
        FD_SET(output_pipe, &readset);
        fprintf(stderr, "Server: child %d: write: %d, read: %d\n", i, input_pipe, output_pipe);
        MAX_FD = MAX_FD > output_pipe ? MAX_FD : output_pipe;
    }
    usleep(500000);
    // buffer
    char buffer[10000];
    char temp_msg[10000];
    size_t buffer_size = 10000;
    size_t tmp_size = 0;
    memset(buffer, 0, 10000);
    memset(temp_msg, 0, 10000);

    // queue
    json json_pool[1200];
    memset(json_pool, 0, sizeof(json_pool));
    bool isDone[2000];
    memset(isDone, true, sizeof(isDone));
    int pair[1200];
    memset(pair, 0, sizeof(pair));
    vector<int> FD;

    //int queueing_num = 0;
    fprintf(stderr, "Server: Start Listening\n");
    fprintf(stderr, "SOCKFD: %d\n", sockfd);
    while(1) {
        // wait()
       /* for(int fucked_child = 0; fucked_child < CHILD_NUM; ++fucked_child) {
            int status;
            pid_t ret = waitpid(child.pid[fucked_child], &status, WNOHANG);
            //fprintf(stderr, "RET: %d, CPID: %d\n", ret, child.pid[i]);
            // restart child process
            if(ret == child.pid[fucked_child]) {
                fprintf(stderr, "Server: Child %d : %d is fucked up!! SIGNAL: %d %d %d\n", fucked_child, ret, WIFSIGNALED(status), WTERMSIG(status), WCOREDUMP(status));
                // reopen child process and clear old data
                FD_CLR(child.read_fd[fucked_child], &readset);
                close(child.read_fd[fucked_child]);
                close(child.write_fd[fucked_child]);
                fd_queue[child.read_fd[fucked_child]] = false;
                std::string in_pipe = "./fifo/in" + to_string(fucked_child);
                remove(in_pipe.c_str());
                mkfifo(in_pipe.c_str(), 0644);

                std:string out_pipe = "./fifo/out" + to_string(fucked_child);
                remove(out_pipe.c_str());
                mkfifo(out_pipe.c_str(), 0644);
                pid_t new_process;
                if((new_process = fork()) == 0) {
                    fprintf(stderr, "Child %d: pid: %d\n", fucked_child, getpid());
                    child_process(fucked_child);
                    exit(0);
                }
                child.pid[fucked_child] = new_process;


                // new name pipe


                int input_pipe = open(in_pipe.c_str(), O_WRONLY);
                child.write_fd[fucked_child] = input_pipe;
                int output_pipe = open(out_pipe.c_str(), O_RDONLY);
                child.read_fd[fucked_child] = output_pipe;
                child.state[fucked_child] = true;
                fd_queue[output_pipe] = true;
                FD_SET(output_pipe, &readset);
                // update MAX_FD
                MAX_FD = sockfd;
                for(int i = 1; i < 1200; ++i) 
                    if(fd_queue[i]) MAX_FD = MAX_FD > i ? MAX_FD : i;
                for(int i = 0; i < CHILD_NUM; ++i)
                    MAX_FD = MAX_FD > child.read_fd[i] ? MAX_FD : child.read_fd[i];
                child.matched += 1;
                child.matched %= CHILD_NUM;
                // restore process queue    
                for(int i = 0; i < FD.size(); ++i) {
                    if(isDone[FD[i]]) continue;
                    json tmp_json = json_pool[FD[i]];
                    
                    struct Header header;
                    memset(&header, 0, sizeof(struct Header));
                    // 
                    if((i+1) % (fucked_child+1) == 0 ) header.type = TO_MATCH;
                    else header.type = PUT_IN_QUEUE;
                    fprintf(stderr, "Server: Restore FD %d\n", FD[i]);
                    header.fd = FD[i];
                    header.matched_fd = FD[i];
                    std::string filter_path = "./filter/" + tmp_json["name"].get<std::string>() + "_" + to_string(FD[i]);
                    memcpy(&(header.filter_path), filter_path.c_str(), filter_path.length());
                    header.filter_path[filter_path.length()] = '\0';
                    struct User tmp_user;
                    memset(&tmp_user, 0, sizeof(struct User));
                    parse_json(&tmp_user, tmp_json);
                    write(input_pipe, &header, sizeof(struct Header));
                    write(input_pipe, &tmp_user, sizeof(struct User));
                }
            }
        }*/

        memcpy(&working_readset, &readset, sizeof(fd_set));
        int retval = select(MAX_FD+1, &working_readset, NULL, NULL, &timeout);
        if (retval < 0) {
            perror("Server: Select() Went Wrong:");
            exit(errno);
        }
        if (retval == 0) continue;
        // accept user command
        if(FD_ISSET(0, &working_readset)) {
            fprintf(stderr, "Server: something from user's command\n");
            int leave = handle_command();
            // notify child that server is going to close
            if(leave) {
                struct Header header;
                memset(&header, 0, sizeof(struct Header));
                header.type = EXIT;
                for(int i = 0; i < CHILD_NUM; ++i) write(child.write_fd[i], &header, sizeof(struct Header));
                fprintf(stderr, "Server: Flushing Name Pipe\n");
                sleep(1);
                break;
            }
            else continue;
        }

        // user matched!!
        if(FD_ISSET(child.read_fd[child.matched], &working_readset)) {
            int match_num = 0;
            int working_child = child.matched;
            read(child.read_fd[working_child], &match_num, sizeof(int));
            vector<struct Match> match_pool;
            struct Match match;
            child.matched += 1;
            child.matched %= CHILD_NUM;
            //queueing_num -= 2;
            for(int i = 0; i < match_num; ++i) {
                memset(&match, 0, sizeof(struct Match));
                read(child.read_fd[working_child], &match, sizeof(struct Match));
                if(isDone[match.fd] || isDone[match.matched_fd]) continue;
                if(match.type == SEND_MATCH) match_pool.push_back(match);
            }

            memset(&match, 0, sizeof(struct Match));

            if(match_pool.size() <= 0) continue;
            match = match_pool[0];
            isDone[match.fd] = true;
            isDone[match.matched_fd] = true;
            pair[match.fd] = match.matched_fd;
            pair[match.matched_fd] = match.fd;
            // update waiting queue
            update_queue(&child, match.fd, match.matched_fd);
            
            // send matched
            char json_string[10000];
            memset(json_string, 0, 10000);
            send_match(json_pool[match.fd], json_string);
            send(match.matched_fd, json_string, strlen(json_string), 0);

            memset(json_string, 0, 10000);
            send_match(json_pool[match.matched_fd], json_string);
            send(match.fd, json_string, strlen(json_string), 0);
            continue;
        }
        // other

        for (int fd = 1; fd < MAX_FD+1; ++fd) {
            //if(queueing_num < 0) queueing_num = 0;
            if (!FD_ISSET(fd, &working_readset)) continue;
            bool isChild = false;
            for(int i = 0; i < CHILD_NUM; ++i) {
                if(fd == child.read_fd[i]) {
                    isChild = true;
                    break;
                }
            }
            if(isChild) continue;
            //fprintf(stderr, "Server: FD %d has something\n", fd);
            // for socket
            // 分成兩個情形：接受新連線用的 socket 和資料傳輸用的 socket
            if (fd == sockfd) {
            // sockfd 有事件，表示有新連線
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int client_fd = accept(fd, (struct sockaddr*) &client_addr, &addrlen);
                if (client_fd >= 0) FD_SET(client_fd, &readset); // 加入新創的描述子，用於和客戶端連線
                MAX_FD = MAX_FD > client_fd ? MAX_FD : client_fd;
                fd_queue[client_fd] = true;
            }
            else {
            // 這裏的描述子來自 accept() 回傳值，用於和客戶端連線
                size_t sz;
                memset(buffer, 0, 10000);
                sz = recv(fd, buffer, buffer_size, 0); // 接收資料
                //fprintf(stderr, "Server: Receive size: %d\n", sz);
                if (sz == 0) {// recv() 回傳值爲零表示客戶端已關閉連線 
                    // 關閉描述子並從 readset 移除
                    //queueing_num -= 2;
                    close(fd);
                    FD_CLR(fd, &readset);
                    isDone[fd] = true;
                    isDone[pair[fd]] = true;
                    fd_queue[fd] = false;
                    memset(&(json_pool[fd]), 0, sizeof(json));
                    memset(&(json_pool[pair[fd]]), 0, sizeof(json));
                    update_queue(&child, fd, pair[fd]);
                    MAX_FD = sockfd;
                    for(int i = 1; i < 1200; ++i) {
                       if(fd_queue[i]) {
                            MAX_FD = MAX_FD > i ? MAX_FD : i;
                        }
                    }
                    // the situation of no closing socket directly
                    if(pair[fd] ==0) continue;
                    char quit[] = "{\"cmd\":\"other_side_quit\"}\n";
                    send(pair[fd], quit, strlen(quit), 0);
                    pair[pair[fd]] = 0;
                    pair[fd] = 0;
                }
                else if (sz == -1) {// 發生錯誤
                    fprintf(stderr, "Server: Receive size: %d\n", sz);
                    if(errno == 104) {
                        fprintf(stderr, "fuck\n");
                        close(fd);
                        FD_CLR(fd, &readset);
                        isDone[fd] = true;
                        isDone[pair[fd]] = true;
                        fd_queue[fd] = false;
                        memset(&(json_pool[fd]), 0, sizeof(json));
                        memset(&(json_pool[pair[fd]]), 0, sizeof(json));
                        update_queue(&child, fd, pair[fd]);
                        MAX_FD = sockfd;
                        for(int i = 1; i < 1200; ++i) {
                            if(fd_queue[i]) MAX_FD = MAX_FD > i ? MAX_FD : i;
                        }
                        // the situation of no closing socket directly
                        if(pair[fd] ==0) continue;
                        char quit[] = "{\"cmd\":\"other_side_quit\"}\n";
                        send(pair[fd], quit, strlen(quit), 0);
                        
                        pair[pair[fd]] = 0;
                        pair[fd] = 0;
                        continue;
                    }
                    perror("Server: RECV ERROR\n");
                    exit(EXIT_FAILURE);
                    //進行錯誤處理

                }
                else if( sz > 0) {// sz > 0，表示有新資料讀入
                    buffer[sz] = '\0';
                    char buff[10000];
                    memset(buff, 0, 10000);
                    memcpy(&temp_msg[tmp_size], buffer, sz+1);
                    tmp_size += sz;
                    bool isJson = handle_msg(buff, temp_msg, tmp_size);
                    // parsing JSON
                    if(isJson) {
                        struct User tmp_user;
                        memset(&tmp_user, 0, sizeof(struct User));
                        std::string buf = buff;
                        memset(buff, 0, 10000);
                        json tmp_json = json::parse(buf);
                        // Command
                        string cmd = tmp_json["cmd"].get<std::string>();
                        
                        // try match
                        if(cmd == "try_match") {
                            // available
                            isDone[fd] = false;
                            FD.push_back(fd);
                            
                            // transform json to struct user
                            parse_json(&tmp_user, tmp_json);
                            json_pool[fd] = tmp_json;
                            //fprintf(stderr, "Server: store json:\n%s\n", json_pool[fd].dump().c_str());
                            std::string filter_path = compile(tmp_json, fd);
                            
                            // notify that the server has recv client's request
                            char cmd[] = "{\"cmd\":\"try_match\"}\n";
                            send(fd, cmd, strlen(cmd), 0);

                            // assign task
                            int write_fd = child.write_fd[child.current_process];
                            struct Header header;
                            memset(&header, 0, sizeof(struct Header));
                            header.type = TO_MATCH;
                            header.fd = fd;
                            header.matched_fd = fd;
                            memcpy(header.filter_path, filter_path.c_str(), filter_path.length());
                            header.filter_path[filter_path.length()] = '\0';
                            write(write_fd, &header, sizeof(struct Header));
                            write(write_fd, &tmp_user, sizeof(struct User));
                            
                            for(int i = 0; i < CHILD_NUM; ++i) {
                                if(i == child.current_process) continue;
                                write_fd = child.write_fd[i];
                                header.type = PUT_IN_QUEUE;
                                write(write_fd, &header, sizeof(struct Header));
                                write(write_fd, &tmp_user, sizeof(struct User));
                            }
                            child.current_process+= 1;
                            child.current_process%= CHILD_NUM;
                            //fprintf(stderr, "Server: Tasks haved been assigned\n");
                            
                        }
                        else if(cmd == "quit") {
                            isDone[fd] = true;
                            isDone[pair[fd]] = true;
                            memset(&(json_pool[fd]), 0, sizeof(json));
                            memset(&(json_pool[pair[fd]]), 0, sizeof(json));
                            update_queue(&child, fd, pair[fd]);
                            
                            int write_fd = pair[fd];
                            char quit[] = "{\"cmd\":\"quit\"}\n";
                            send(fd, quit, strlen(quit), 0);
                            if(pair[fd] == 0) break;
                            char other_quit[] = "{\"cmd\":\"other_side_quit\"}\n";
                            send(write_fd, other_quit, strlen(other_quit), 0);
                            pair[pair[fd]] = 0;
                            pair[fd] = 0;
                            //queueing_num -= 2;
                        }
                        else if(cmd =="send_message") {
                            if(pair[fd] == 0) break;
                            //size_t buf_size = strlen(buffer);
                            //buffer[buf_size] = '\n';
                            //buffer[buf_size+1] = '\0';
                            char send_ack[10000];
                            memset(send_ack, 0, 10000);
                            std::string ack = tmp_json.dump();
                            memcpy(send_ack, ack.c_str(), ack.length());
                            send_ack[ack.length()] = '\n';
                            fprintf(stderr, "%s", send_ack);
                            send(fd, send_ack, strlen(send_ack), 0);
                            //std::string buf = tmp_json.dump();
                            /*json ack;
                            ack["cmd"] = "send_message";
                            ack["message"] = tmp_json["message"].get<std::string>();
                            ack["sequence"] = tmp_json["sequence"];
                            std::string ack_string = ack.dump() + "\n";*/
                            //char send_ack[] = "{\"cmd\":\"send_message\"}\n";
                            //char send_ack[10000];
                            //memset(send_ack, 0, 10000);
                            //memcpy(send_ack, ack_string.c_str(), ack_string.length());
                            //send_ack[buf.length()] = '\n';
                            //send(fd, send_ack, strlen(send_ack), 0);
                            json new_json = tmp_json;
                            new_json["cmd"] = "receive_message";
                            char send_msg[10000];
                            memset(send_msg, 0, 10000);
                            std::string j_string = new_json.dump();
                            //fprintf(stderr, "%s", j_string.c_str());
                            memcpy(send_msg, j_string.c_str(), j_string.length());
                            send_msg[j_string.length()] = '\n';
                            //send_msg[j_string.length()+1] = '\0';
                            //fprintf(stderr, "%s", send_msg);
                            send(pair[fd], send_msg, strlen(send_msg), 0);
                            //fprintf(stderr, "%s", send_msg);
                        }
                        isJson = handle_msg(buff, temp_msg, tmp_size);
                    }
                }
            }
        }
    }
    close(sockfd);
    for(int i = 0; i < CHILD_NUM; ++i) {
        close(child.write_fd[i]);
        close(child.read_fd[i]);
    }
    return 0;
}