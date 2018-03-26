#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <iostream>
#include <vector>
#include <openssl/md5.h>
#include <sys/mman.h>

#include "boss.h"

struct DUMP_INFO {
    int size;
    int ptr;
    int fd;
};

int n_treasure_count(struct server_config *config, unsigned char* hash, int n_treasure) {
    char hash_string[33];
    for(int i = 0; i < 16; ++i) {
        char tmp[3];
        sprintf(tmp, "%02x", hash[i]);
        tmp[2] = '\0';
        memcpy(&hash_string[(2*i)], tmp, 2);
    }
    hash_string[32] = '\0';
    int hash_ptr = 0;
    while(hash_string[hash_ptr++] == '0') {
        if(n_treasure < hash_ptr && hash_string[hash_ptr] != '0') {
            n_treasure = hash_ptr;
            memcpy(config->hash, hash_string, 33);
            break;
        }
    }
    return n_treasure;
}

int MD5(int fd, struct server_config *config) {
    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_CTX mdContext;
    //int bytes;
    //unsigned char data[200001];
    MD5_Init (&mdContext);
    memcpy(&mdContext, &(config->mdContext), sizeof(MD5_CTX));
    int n_treasure_tmp = config->n_treasure;
   //if((bytes = read (fd, data, 200000)) != 0) {
    if(config->mine_ptr != config->size) {
        //memcpy(config->treasure+config->size, data, bytes);
        //int process_ptr = 0;
        //while(process_ptr < bytes) {
        //int count = config->size - 1;
        //if(config->size > 1000000) count = 300000;
        //while((config->mine_ptr+count) < config->size && count--){
            MD5_CTX tmpContext;
            MD5_Init (&tmpContext);
            unsigned char data[8192];
            int process_ptr;
            int pkt_size = (config->size/1000000) > 0 ? (config->size/1000000) : 1;
            process_ptr = (config->mine_ptr+pkt_size) > config->size ? (config->size - config->mine_ptr) : pkt_size;
            
            memcpy(data, config->treasure+ config->mine_ptr, process_ptr);
            //MD5_Update(&mdContext, &data[process_ptr], 1);
            MD5_Update(&mdContext, data, process_ptr);
            memcpy(&tmpContext, &mdContext, sizeof(MD5_CTX));
            MD5_Final (c, &tmpContext);
            n_treasure_tmp = n_treasure_count(config, c, n_treasure_tmp);
            if(n_treasure_tmp == config->n_treasure_tmp+1) {
                //fprintf(stderr, "%d-treasure: %s\n", n_treasure_tmp, config->hash);
                config->n_treasure_tmp = n_treasure_tmp;
            }
            //process_ptr++;
            config->mine_ptr += process_ptr;
        //}
        //config->size = config->size + bytes;
        memcpy(&config->mdContext, &mdContext,sizeof(MD5_CTX));
    }
    else {
        fprintf(stderr, "Loading mine Done!\n");
        config->n_treasure = config->n_treasure_tmp;
        config->isDone = true;
        MD5_Final (c,&(mdContext));
        for(int i = 0; i < 16; ++i) {
            char tmp[2];
            sprintf(tmp, "%02x", c[i]);
            memcpy(config->hash + (2*i), tmp, 2);
        }
        config->hash[32] = '\0';
    }
}

int load_config_file(struct server_config *config, char *path)
{
    /* TODO finish your own config file parser */
    int fd = open(path, O_RDONLY);
    if(fd == -1) {
        perror("Server: Open Config Error");
        exit(EXIT_FAILURE);
    }
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    unsigned char *config_content = new unsigned char[size+1];
    if(size != (read(fd, config_content, size))) {
        perror("Server: Read Config Error");
        exit(EXIT_FAILURE);
    }
    config_content[size] = '\0';    
    
    unsigned char* config_ptr = config_content;
    // parsing mine 
    while(*config_ptr != ' ') config_ptr++;
    char *mine_tmp = config->mine_file;
    int path_ptr = 0;
    while(*(++config_ptr) != '\n') mine_tmp[path_ptr++] = *config_ptr; 
    mine_tmp[path_ptr] = '\0';
    int num_miner = 0;
    config->pipes = new struct pipe_pair[33];
    struct pipe_pair *pipes_tmp = config->pipes;
    memset(pipes_tmp, 0, 33);
    while(*config_ptr) {
        if(*config_ptr == ':') {
            config_ptr++;
            pipes_tmp[num_miner].input_pipe = new char[PATH_MAX+1];
            char *miner_tmp = pipes_tmp[num_miner].input_pipe;
            int path_ptr = 0;
            while(*(++config_ptr) != ' ') miner_tmp[path_ptr++] = *config_ptr;
            miner_tmp[path_ptr] = '\0';
            path_ptr = 0;
            pipes_tmp[num_miner].output_pipe = new char[PATH_MAX+1];
            miner_tmp = pipes_tmp[num_miner].output_pipe;
            while(*(++config_ptr) != '\n') miner_tmp[path_ptr++] = *config_ptr;
            miner_tmp[path_ptr] = '\0';
            num_miner++;
        }
        ++config_ptr;
    }

    // other content
    config->num_miners = num_miner;
    close(fd);
    delete[] config_content;
    return 0;
}

int assign_jobs(int mode, struct fd_pair *client_fds, struct server_config *config)
{
    /* TODO design your own (1) communication protocol (2) job assignment algorithm */
    int load = (256/config->num_miners) - 1;
    int start = 0;
    for(int i = 0; i < config->num_miners; ++i) {
        int range = start+load;
        if(range > 255) range = 255;
        struct fd_pair tmp = client_fds[i];
        char pkt[100000];
        //mode
        write(tmp.input_fd, &mode, sizeof(int));
        write(tmp.input_fd, &(config->mdContext), sizeof(MD5_CTX));
        write(tmp.input_fd, &(config->n_treasure), sizeof(int));
        //fprintf(stderr, "n_treasure: %d\n", config->n_treasure);
        if(mode == 1) {
            write(tmp.input_fd, &start, sizeof(int));
            //fprintf(stderr, "start: %d\n", start);
            write(tmp.input_fd, &range, sizeof(int));
            //fprintf(stderr, "end: %d\n", range);
        }
        else if(mode == 3) {
            int size = strlen(config->win_name) + 1;
            write(tmp.input_fd, &size, sizeof(int));
            write(tmp.input_fd, (config->win_name), size);
            write(tmp.input_fd, config->hash, 32);
        }
        start = range + 1;
    }
}

int handle_command(struct server_config *config, struct fd_pair *client_fds, bool quit)
{
    /* TODO parse user commands here */
    char cmd[20];/* command string *///;
    char tmp[PATH_MAX + 10];
    char path[PATH_MAX+1];
    int cmd_size = 0;
    int path_size = 0;
    int tmp_ptr = 0;
    off_t size = read(0, tmp, PATH_MAX + 10);

    while(tmp[tmp_ptr]!= '\n') {
        if(tmp[tmp_ptr] == ' ') {
            while(tmp[++tmp_ptr] != '\n') path[path_size++] = tmp[tmp_ptr];
            path[path_size] = '\0';
            break;
        }
        cmd[cmd_size++] = tmp[tmp_ptr];
        tmp_ptr++;
    }
    cmd[cmd_size] = '\0';
    //delete[] tmp_ptr;
    if(quit) return -2;
    
    if (strcmp(cmd, "status") == 0)
    {
        //printf("cmd: %s\n", cmd);
        /* TODO show status */
        if(config->n_treasure > 0)
            printf("best %d-treasure %s in %d bytes\n", config->n_treasure, config->hash, config->size+config->append_size);
        else printf("best 0-treasure in 0 bytes\n");
        if(config->isDone) {
            int status = 2;
            for(int i = 0; i < config->num_miners; ++i) 
                write(client_fds[i].input_fd, &status, sizeof(int));
        }
    }
    else if (strcmp(cmd, "dump") == 0)
    {
        /* TODO write best n-treasure to specified file */
        //remove(path);
        
        struct stat dump_stat;
        int dump_fd;
        int open_stat = stat(path, &dump_stat);
        //fprintf(stderr, "path: %s\n", path);
        if((dump_stat.st_mode & S_IFMT) == S_IFIFO) {
            dump_fd = open(path, O_WRONLY);
            //fprintf(stderr, "fifo: %d\n", dump_fd);
        }
        else {
            remove(path);
            dump_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);
            ftruncate(dump_fd, config->size+config->append_size);
        }
        if(dump_fd == -1) {
            perror("Server: Open Dumping File Error");
            return -1;
        }
        else if(!config->isDone) return 0;
        else if(dump_fd > 0) fprintf(stderr, "Server: Open %s and Watching dumping\n", path);
        config->isDumping = true;
        return dump_fd;
    }
    else if(strcmp(cmd, "quit") == 0)
    {
        /* TODO tell clients to cease their jobs and exit normally */
        int quit = 4;
        if(!config->isDumping) {
            for(int i = 0; i < config->num_miners; ++i) write(client_fds[i].input_fd, &quit, sizeof(int));
        }
        return -2;
    }
    else if(strcmp(cmd, "fuck") == 0) {
        int fuck = 6;
        for(int i = 0; i < config->num_miners; ++i) write(client_fds[i].input_fd, &fuck, sizeof(int));
        exit(0);
    }
    else if(strcmp(cmd, "mine") == 0) {
        if(config->size > 10) {
            fprintf(stderr, "Processing: %d bytes\n", config->mine_ptr);
        }
        else {
            int i = 0;
            unsigned char *origin = config->treasure;
            unsigned char *append = config->append;
            while(i < config->size) fprintf(stderr, "%d ", origin[i++]);
            i = 0;
            while(i < config->append_size) fprintf(stderr, "%d ", append[i++]);
            fprintf(stderr, "\n");
            i = 0;
            while(i < config->size) fprintf(stderr, "\\x%02x", origin[i++]);
            i = 0;
            while(i < config->append_size) fprintf(stderr, "\\x%02x", append[i++]);
            fprintf(stderr, "\n");
        }
    }
    else fprintf(stderr, "Server: No Such Command!\n");
    return 0;
}

int handle_treasure(int fd, struct server_config *config) {
    // MD5_CTX
    MD5_CTX tmpContext;
    MD5_Init(&tmpContext);
    read(fd, &tmpContext, sizeof(MD5_CTX));
    // n_treasure
    int tmp_n = 0;
    read(fd, &tmp_n, sizeof(int));
    //fprintf(stderr, "n_treasrue: %d\n", tmp_n);
    // name
    int name_size = 0;
    read(fd, &name_size, sizeof(int));
    //fprintf(stderr, "name_size: %d\n", name_size);
    char win_name[1000];
    read(fd, win_name, name_size);
    //fprintf(stderr, "name: %s\n", config->win_name);
    // append mine
    int append_size = 0;
    read(fd, &append_size, sizeof(int));
    //fprintf(stderr, "append_size: %d\n", config->append_size);
    char append[1000];
    read(fd, append, append_size);
    //fprintf(stderr, "treasure: %s\n", config->append);
    // hash 
    char hash[33];
    read(fd, hash, 33);
    //fprintf(stderr, "hash: %s\n", config->hash);
    
    if(tmp_n != config->n_treasure+1) return 0;
    else config->n_treasure = tmp_n;
    memcpy(&(config->mdContext), &tmpContext, sizeof(MD5_CTX));
    memcpy(config->win_name, win_name, name_size);
    
    memcpy(config->append + config->append_size, append, append_size);
    config->append_size += append_size;
    memcpy(config->hash, hash, 33);
    printf("A %d-treasure discovered! %s\n", config->n_treasure, config->hash);
    return 1;
}

int initialize_config(struct server_config *config) {
    MD5_CTX mdContext;
    MD5_Init(&mdContext);
    memcpy(&(config->mdContext), &mdContext, sizeof(MD5_CTX));
    memcpy(&(config->tmpContext), &mdContext, sizeof(MD5_CTX));
    config->mine_file = new char[PATH_MAX+1];
    config->hash = new char[33];
    config->win_name = new char[1500];
    config->append = new unsigned char[1000];
    config->n_treasure = 0;
    config->n_treasure_tmp = 0;
    config->isLarge = false;
    config->isDone = false;
    config->append_size = 0;
    config->mine_ptr = 0;
    config->isDumping = false;
    memset(config->win_name, 0, 1500);
    memset(config->hash, 0, 33);
    memset(config->append, 0, 1000);
    memset(config->mine_file, 0, PATH_MAX+1);
    return 0;
}

int load_mine_data(struct server_config *config) {
    int mine_fd = open(config->mine_file, O_RDWR);
    off_t size = lseek(mine_fd, 0, SEEK_END);
    //config.size = size;
    
    lseek(mine_fd, 0, SEEK_SET);
    //config->treasure = new unsigned char[size+1];
    config->treasure = (unsigned char*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, mine_fd, 0);
    if(config->treasure ==  MAP_FAILED) {
        perror("Server: Mmap Error");
    }
    config->size = size;
    //fprintf(stderr, "Treasure: %s\n", config->treasure);
    return mine_fd;
}   

int main(int argc, char **argv)
{
    /* sanity check on arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    /* load config file */
    struct server_config config;
    initialize_config(&config);
    load_config_file(&config, argv[1]);
    int mine_fd = load_mine_data(&config);
    
    /* open the named pipes */
    struct fd_pair client_fds[config.num_miners];
    for (int ind = 0; ind < config.num_miners; ind += 1)
    {
        struct fd_pair *fd_ptr = &client_fds[ind];
        struct pipe_pair *pipe_ptr = &config.pipes[ind];

        fd_ptr->input_fd = open(pipe_ptr->input_pipe, O_WRONLY);
        assert (fd_ptr->input_fd >= 0);
        delete[] pipe_ptr->input_pipe;

        fd_ptr->output_fd = open(pipe_ptr->output_pipe, O_RDONLY);
        assert (fd_ptr->output_fd >= 0);
        delete[] pipe_ptr->output_pipe;
    }
    delete[] config.pipes;

    /* initialize data for select() */
    int maxfd;
    fd_set readset;
    fd_set working_readset;
    fd_set writeset;
    fd_set working_writeset;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;

    FD_ZERO(&writeset);
    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);
    // TODO add input pipes to readset, setup maxfd
    maxfd = 0;
    for(int i = 0; i < config.num_miners; ++i) {
        FD_SET(client_fds[i].output_fd, &readset);
        maxfd = client_fds[i].output_fd > maxfd? client_fds[i].output_fd : maxfd;
        int start = 0;
        write(client_fds[i].input_fd, &start, sizeof(int));
    }

    /* assign jobs to clients */
    // mode, fd, num_miners, treasure, n_treasure, win_name
    bool quit = false;
    int dump_num = 0;
    std::vector<DUMP_INFO> dump_vector;
    
    while (1)
    {
        if(dump_num == 0) {
            config.isDumping = false;
            if(quit) {
                int leave = 4;
                for(int i = 0; i < config.num_miners; ++i) {
                    write(client_fds[i].input_fd, &leave, sizeof(int));
                }
                sleep(1);
                break;
            }
        }
        if(!config.isDone) {
            MD5(mine_fd, &config);
            if(config.isDone) {
                assign_jobs(1, client_fds, &config);
                config.mine_ptr = 0;
                sleep(1);
            }
        }
        memcpy(&working_readset, &readset, sizeof(readset));
        //memcpy(&working_writeset, &writeset, sizeof(writeset));
        int cur_fd = maxfd; 
        for(int i = 0; i < dump_vector.size(); ++i) {
            cur_fd = cur_fd > dump_vector[i].fd ? cur_fd : dump_vector[i].fd;
            if(dump_vector[i].fd != 0) FD_SET(dump_vector[i].fd, &writeset);
        }
        int ready = select(cur_fd+1, &working_readset, &writeset, NULL, &timeout);
        /*if(ready > 0) fprintf(stderr, "Server: Something is ready\nNUMBER OF FD: %d\n", ready);
        else if(ready == -1) {
            perror("Server: Selecet Broken");
        }*/
        if (FD_ISSET(STDIN_FILENO, &working_readset))
        {
            /* TODO handle user input here */
            // -1 means quit, 0 menas nothing, other means select()
            int flag = handle_command(&config, client_fds, quit);
            if(flag == -2) {
                quit = true;
                if(dump_num == 0) {
                    break;
                }
                else
                    fprintf(stderr, "Server: Server will quit after dumping file\nNo Command will be accepted now\n");
            }
            else if (flag > 0) {
                dump_num++;
                struct DUMP_INFO tmp;
                tmp.size = config.size;
                tmp.fd = flag;
                tmp.ptr = 0;
                dump_vector.push_back(tmp);
                //FD_SET(flag, &writeset);
            }
            //else {
                //fprintf(stderr, "Server: Dumping file open error\n");
            //}
        }
        for(int i = 0; i < dump_vector.size(); ++i) {
            if(dump_vector[i].fd!= 0) {
                if(FD_ISSET(dump_vector[i].fd, &writeset)){
                    if(dump_vector[i].size > dump_vector[i].ptr) {
                        //fprintf(stderr, "Server: Something is selected\n");
                        int writein_size = (dump_vector[i].ptr + 3000000) < dump_vector[i].size ? (3000000) : dump_vector[i].size - dump_vector[i].ptr; 
                        unsigned char data[3000001];
                        memcpy(data, &(config.treasure[dump_vector[i].ptr]), writein_size);
                        size_t size = write(dump_vector[i].fd, data, writein_size);
                        dump_vector[i].ptr += writein_size;
                        //size += write(dump_vector[i], config.append, config.append_size);
                        fprintf(stderr, "Server: Write %d bytes\n", size);
                    }
                    else if(dump_vector[i].size == dump_vector[i].ptr) {
                        ftruncate(dump_vector[i].fd, config.size+config.append_size);
                        write(dump_vector[i].fd, config.append, config.append_size);
                        dump_vector[i].ptr += config.append_size;
                        dump_num--;
                        fprintf(stderr, "Server: Dumping successfully!\nWrite in size: %d\n", dump_vector[i].ptr);
                        //FD_CLR(dump_vector[i].fd, &writeset);
                        //FD_ZERO(&writeset);
                        fflush(NULL);
                        close(dump_vector[i].fd);
                        dump_vector[i].fd = 0;
                    }
                }
            }
        }
        // TODO check if any client send me some message
        for(int i = 0; i < config.num_miners; ++i) {
            if(FD_ISSET(client_fds[i].output_fd, &working_readset) && !quit) {
                // find greater n_treasure
                if((handle_treasure(client_fds[i].output_fd, &config) == 1)) {
                    assign_jobs(3, client_fds, &config);
                }
            }
        }
        FD_ZERO(&writeset);
    }

    /* TODO close file descriptors */
    fprintf(stderr, "Server: Flushing Packet\n");
    close(mine_fd);
    munmap(config.treasure, config.size);
    
    usleep(1500000);
    fflush(NULL);
    fprintf(stderr, "Server: 88 ^_^\n");
    for(int i = 0; i < config.num_miners; ++i) {
        close(client_fds[i].input_fd);
        close(client_fds[i].output_fd);
    }
    //delete[] config.treasure;
    //delete[] config.hash;
    //delete[] config.win_name;
    //delete[] config.mine_file;
    return 0;
}