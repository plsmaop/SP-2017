#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <openssl/md5.h>

struct Mine {
    long long work_times = 0;
    int start;
    int start_tmp;
    int end;
    int n_treasure;
    bool work ;
    unsigned char *mine;
    char *win_name;
    char *mine_hash;
    char *working_hash;
    char *name;
    int mine_size;
    MD5_CTX mdContext;
    MD5_CTX tmpContext;
};

int MD5(struct Mine *mine, unsigned char *working_mine, int mine_size) {
    unsigned char c[MD5_DIGEST_LENGTH];
    int bytes;
    MD5_CTX tmpContext;
    MD5_Init (&tmpContext);
    MD5_Init(&(mine->tmpContext));
    memcpy(&tmpContext, &(mine->mdContext), sizeof(MD5_CTX));
    MD5_Update (&tmpContext, working_mine, mine_size);
    memcpy(&(mine->tmpContext), &(tmpContext), sizeof(MD5_CTX));
    MD5_Final (c,&tmpContext);
    for(int i = 0; i < 16; ++i) {
        char tmp[2];
        sprintf(tmp, "%02x", c[i]);
        memcpy(mine->working_hash + (2*i), tmp, 2);
    }
    mine->working_hash[32] = '\0';
}

int initialze_mine(struct Mine *mine) {
    MD5_Init(&(mine->mdContext));
    MD5_Init(&(mine->tmpContext));
    mine->start = 0;
    mine->start_tmp = 0;
    mine->end = 0;
    mine->n_treasure = 0;
    mine->work = false;
    mine->mine = new unsigned char[1001];
    memset(mine->mine, 0, 1000);
    mine->name = new char[1201];
    memset(mine->name, 0, 1200);
    mine->win_name = new char[1201];
    memset(mine->win_name, 0, 1200);
    mine->mine_hash = new char[33];
    memset(mine->mine_hash, 0, 33);
    mine->mine_size = 0;
    mine->working_hash = new char[33];
    memset(mine->working_hash, 0, 33);
}

int clear_mine(struct Mine *mine) {
    //mine->start = 0;
    mine->start_tmp = 0;
    mine->end = 0;
    mine->n_treasure = 0;
    mine->work = false;
    memset(mine->mine, 0, 1000);
    memset(mine->win_name, 0, 1200);
    memset(mine->mine_hash, 0, 33);
    mine->mine_size = 0;
    memset(mine->working_hash, 0, 33);
    MD5_Init(&(mine->mdContext));
    MD5_Init(&(mine->tmpContext));
}
void mining(struct Mine *mine, size_t ptr) {
    if (ptr == 0){
        //printf("formal:\nsize: %d tmp: %d\n", mine_size, mine_tmp_size);
        mine->mine[0] = 0;
        memcpy(mine->mine+1, mine->mine, mine->mine_size);
        mine->mine_size+=1;
        //int i = 0;
        //while(i < mine_tmp_size) printf("%d ", mine[i++]);
        //printf("\n");
    }
    else if(mine->mine[ptr-1] == 255) {
        mine->mine[ptr-1] = 0;
        mining(mine, ptr-1);
    }
    else {
        //printf("add\n");
        mine->mine[ptr-1] = mine->mine[ptr-1]+1 ;
        //printf("%d", mine[ptr-1]);
    }
}

int main(int argc, char **argv)
{
    /* parse arguments */
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    struct Mine mine;
    initialze_mine(&mine);
    mine.name = argv[1];
    char *input_pipe = argv[2];
    char *output_pipe = argv[3];
    /* create named pipes */
    int ret;
    //fprintf(stderr, "Input: %s\n", input_pipe);
    remove(input_pipe);
    ret = mkfifo(input_pipe, 0644);
    assert (!ret);

    remove(output_pipe);
    ret = mkfifo(output_pipe, 0644);
    assert (!ret);

    /* open pipes */
    int input_fd = open(input_pipe, O_RDONLY);
    assert (input_fd >= 0);

    int output_fd = open(output_pipe, O_WRONLY);
    assert (output_fd >= 0);

    /* TODO write your own (1) communication protocol (2) computation algorithm */
    /* To prevent from blocked read on input_fd, try select() or non-blocking I/O */
    // select()

    struct timeval tv;
    fd_set readset;
    fd_set working_readset;

    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    FD_ZERO(&readset);
    FD_ZERO(&working_readset);
    FD_SET(input_fd, &readset);


    /* Be aware that readset can be modified by select(). Do not select() it twice! */
    while(true) {
        memcpy(&working_readset, &readset, sizeof(readset)); // why we need memcpy() here?
        int rel = select(input_fd+1, &working_readset, NULL, NULL, &tv);
        if (FD_ISSET(input_fd, &working_readset))
        {
            bool find = false;
            int mode = 0;
            if(read(input_fd, &mode, sizeof(int)) == 0) continue;
            switch(mode) {
                case 0: //fuck
                    printf("BOSS is mindful.\n");
                    break;
                case 1: //start
                    mine.work = true;
                    read(input_fd, &(mine.mdContext), sizeof(MD5_CTX));
                    read(input_fd, &(mine.n_treasure), sizeof(int));
                    //fprintf(stderr, "n_treasure: %d\n", mine.n_treasure);
                    read(input_fd, &(mine.start), sizeof(int));
                    //fprintf(stderr, "start: %d\n", mine.start);
                    read(input_fd, &(mine.end), sizeof(int));
                    //fprintf(stderr, "end: %d\n", mine.end);
                    mine.start_tmp = mine.start_tmp;
                    break;
                case 2:  {//status
                    printf("I'm working on %s\n", mine.working_hash);
                    //fprintf(stderr, "I've ran %lld times md5 caculation\nCurrent processing mine: \n", work_times);
                    /*int i = 0;
                    while(i < 33) {
                        fprintf(stderr, "%d ", mine.mine_hash[i]);
                        i++;
                    }
                    fprintf(stderr, "\n");*/
                    //fprintf(stderr, "Client %s: Working On Length %d\n", name, mine_tmp_size);
                    break;
                }
                case 3: {//new
                    clear_mine(&mine);
                    find = true;
                    mine.work = true;
                    int name_size = 0;
                    read(input_fd, &(mine.mdContext), sizeof(MD5_CTX));
                    read(input_fd, &(mine.n_treasure), sizeof(int));
                    read(input_fd, &name_size, sizeof(int));
                    read(input_fd, (mine.win_name), name_size);
                    read(input_fd, mine.mine_hash, 32);

                    //printf("Client: Receive New Job\n");
                    break; 
                }
                case 4: { //quit
                    
                    char *tmp;
                    read(input_fd, tmp, 10000);
                    //close(input_fd);
                    //input_fd = open(input_pipe, O_RDONLY);
                    printf("BOSS is at rest.\n");
                    clear_mine(&mine);
                    break;
                }
                case 6: //fuck
                    printf("Client %s: Fuck You 88\n", mine.name);
                    exit(0);
                default:
                    break;
            }
            if(find) {
                if(strcmp(mine.win_name, mine.name) == 0) printf("I win a %d-treasure! %s\n", mine.n_treasure, mine.mine_hash);
                else printf("%s wins a %d-treasure! %s\n", mine.win_name, mine.n_treasure, mine.mine_hash);
                find = false;
                sleep(1);
            }
            //delete[] tmp;
        }
        int count = 500;
        while(mine.work && count--) {
            //work_times += 1;
            if(mine.start_tmp > mine.end) {
                mine.start_tmp = mine.start;
                mining(&mine, mine.mine_size);
            }
            unsigned char byte = (mine.start_tmp)++;
            //printf("add: %02x\n", byte);
            unsigned char mine_tmp[mine.mine_size+1];
            memcpy(mine_tmp, mine.mine, mine.mine_size);
            mine_tmp[mine.mine_size] = byte;
            //md5(mine_tmp, mine_tmp_size+1);
            MD5(&mine, mine_tmp, mine.mine_size+1);
            int ptr = 0;
            while(mine.working_hash[ptr++] == '0') {
                //printf("find: %d-treasure %s\n", ptr, hash.c_str());
                if(ptr == (mine.n_treasure+1) && mine.working_hash[ptr] != '0') {
                    memcpy(&(mine.mdContext), &(mine.tmpContext), sizeof(MD5_CTX));
                    memcpy(mine.mine, mine_tmp, ++mine.mine_size);
                    //mine.mine_size = mine_tmp_size;
                    mine.work = false;
                    char pkt[1000];
                    // n_treasure name size hash MD5_CTX_size append_mine
                    // MD5_CTX
                    write(output_fd, &(mine.mdContext), sizeof(MD5_CTX));
                    // n_treasure
                    write(output_fd, &ptr, sizeof(int));
                    // name
                    int name_size = strlen(mine.name)+1;
                    //fprintf(stderr, "name_size: %d\n", name_size);
                    write(output_fd, &name_size, sizeof(int));
                    mine.name[name_size] = '\0';
                    //fprintf(stderr, "name: %s\n", mine.name);
                    write(output_fd, (mine.name), name_size);
                    // append_mine
                    write(output_fd, &(mine.mine_size), sizeof(int));
                    write(output_fd, (mine.mine), mine.mine_size);
                    // hash 
                    //fprintf(stderr, "hash: %s\n", mine.working_hash);
                    write(output_fd, (mine.working_hash), 33);
                    memcpy(mine.mine_hash, mine.working_hash, 33);
                    break;

                }
            }
            //delete[] mine_tmp;
        }
    }
    return 0;
}
