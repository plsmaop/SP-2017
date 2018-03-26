#define TO_MATCH 1
#define PUT_IN_QUEUE 2
#define DELETE 3
#define SEND_MATCH 4
#define NO_MATCH 5
#define EXIT 9487

#define CHILD_NUM 20
#define MAX_CHILD_NUM 100

struct Child {
	int write_fd[21];
	int read_fd[21];
	pid_t pid[21];
	bool state[21];
	int current_process;
	int matched;
	int compile_fd;
};

struct Header {
	int type;
	int fd;
	int matched_fd;
	char filter_path[300];
};

struct User {
	char name[33];
	unsigned int age;
	char gender[7];
	char introduction[1025];
};

struct Match {
	int type;
	int fd;
	int matched_fd;
};

// for matching process
struct User_Data {
	int type;
	int fd;
	struct User user;
	char filter_path[300];
};

// for compile
struct Compile {
	int type;
	char filter_path[300];
};
