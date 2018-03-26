struct pipe_pair
{
    char *input_pipe;
    char *output_pipe;
};

struct fd_pair
{
    int input_fd;
    int output_fd;
};

struct server_config
{
    char *win_name;
    int n_treasure;
    unsigned char *treasure;
    int size;
    char *hash;
    char *mine_file;
    struct pipe_pair *pipes;
    int num_miners;
    bool isLarge;
    unsigned char *append;
    int append_size;
    int n_treasure_tmp;
    int mine_ptr;
    bool isDone;
    MD5_CTX mdContext; 
    MD5_CTX tmpContext; 
    bool isDumping;
};

// mode, range, treasure, n_treasure name