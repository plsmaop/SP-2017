#include "csiebox_client.h"

#include "csiebox_common.h"
#include "connect.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/inotify.h> //header for inotify
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <utime.h>
#include <limits.h>

#include <algorithm>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>
using namespace std;

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
static unordered_map<ino_t, std::string> inode_map;
static unordered_map<int, std::string> inotify_map;
//static int all=0;

static int parse_arg(csiebox_client* client, int argc, char** argv);
static int login(csiebox_client* client);
//read config file, and connect to server

inline int restore_time(std::string path, struct stat stat) {
  DIR *cur = opendir(".");
  int cur_dp = dirfd(cur);
  struct timespec new_times[2];
  new_times[0] = stat.st_atim; 
  new_times[1] = stat.st_mtim;  
  if(utimensat(cur_dp, path.c_str(), new_times, AT_SYMLINK_NOFOLLOW) < 0) { 
    perror("Client: Update Time Error\n");
    exit(EXIT_FAILURE);
  }
  closedir(cur);
  return 1;
}

int sync_end(int conn_fd) {
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_END;
  header.req.datalen = sizeof(header);
  if (!send_message(conn_fd, &header, sizeof(header))) {
    fprintf(stderr, "Client: Send Sync End Fail\n");
    return 0;
  }
  return 1;
}

void csiebox_client_init(
  csiebox_client** client, int argc, char** argv) {
  csiebox_client* tmp = (csiebox_client*)malloc(sizeof(csiebox_client));
  if (!tmp) {
    fprintf(stderr, "client malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_client));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = client_start(tmp->arg.name, tmp->arg.server);
  if (fd < 0) {
    fprintf(stderr, "connect fail\n");
    free(tmp);
    return;
  }
  tmp->conn_fd = fd;
  *client = tmp;
}

int sync_file(csiebox_client* client, std::string path){
  csiebox_protocol_file req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);

  int target_file = open(path.c_str(), O_RDONLY | O_NOATIME);
  //FILE* target_file = fopen(path.c_str(), "r");
  if(target_file < 0) {
    fprintf(stderr, "Client: Open %s Error\n", path.c_str());
    exit(1);
  }
  off_t size = lseek(target_file, 0, SEEK_END);
  lseek(target_file, 0, SEEK_SET);
  //fseek(target_file, 0, SEEK_END);
  //off_t size = ftell(target_file);
  //rewind(target_file);
  char *file = new char[size];
  //rewind(target_file);
  if((read(target_file, file, size)) < 0) 
  //if((fread(file, 1, size, target_file)) != size)
    fprintf(stderr, "Client: Read %s Error\n", path.c_str());
  close(target_file);
  //fclose(target_file);
  
  req.message.body.datalen = size;
  //cout << "file size: " << size << "\n";
  //send pathlen to server so that server can know how many charachers it should receive
  //Please go to check the samplefunction in server
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "Client: Send Request Fail\n");
    return 0;
  }

  //send path and file
  //cout << "send file\n";
  send_message(client->conn_fd, file, size);
  //cout << "send path\n";
  //send_message(client->conn_fd, path, strlen(path));
  //delete[] path;
  delete[] file;
  //receive csiebox_protocol_header from server
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_FILE &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      printf("SERVER: Sync File OK\n");
      return 1;
    } else if(header.res.status == CSIEBOX_PROTOCOL_STATUS_FAIL){
      cout << "SERVER: Sync File Error\n";
      return 0;
    }
  }
  return 0;
}

int sync_hardlink(csiebox_client* client, std::string existed_path){
  csiebox_protocol_hardlink req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  //req.message.body.srclen = path.length();
  req.message.body.targetlen = existed_path.length();

  //send pathlen to server so that server can know how many charachers it should receive
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "Client: Send Request Fail\n");
    return 0;
  }

  //send existed path
  char* target_path = new char[existed_path.length()];
  memset(target_path, 0, existed_path.length());
  for(int i = 0; i < existed_path.length(); ++i) target_path[i] = existed_path[i];
  if (!send_message(client->conn_fd, target_path, existed_path.length())) {
    fprintf(stderr, "Send Existed Path: %s Fail\n", existed_path.c_str());
    return 0;
  }
  delete[] target_path;
  //receive csiebox_protocol_header from server
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      printf("SEVER: Sync Hard Link OK\n");
      return 1;
    } else {
      return 0;
    }
  }
  return 1;
}

int sync_rm(csiebox_client* client, std::string path) {
  csiebox_protocol_rm req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_RM;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  req.message.body.pathlen = path.length();
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "Client: Send Request Fail\n");
    return 0;
  }
  char *c_path = new char[path.length()];
  for(int i = 0; i < path.length(); ++i) c_path[i] = path[i];
  //send path
  if (!send_message(client->conn_fd, c_path, path.length())) {
    fprintf(stderr, "Client: Send %s Fail\n", c_path);
    return 0;
  }
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_RM &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      printf("SERVER: Remove File OK\n");
      return 1;
    } else if(header.res.status == CSIEBOX_PROTOCOL_STATUS_FAIL){
      cout << "SERVER: Remove File Error\n";
      return 0;
    }
  }
  return 0;
}

int sync_meta(csiebox_client* client, std::string path, struct stat origin_stat){
  //all++;
  csiebox_protocol_meta req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  req.message.body.pathlen = path.length();

  bool isHardLink = false;
  bool isSymLink = false;
  std::string existed_path = "";
  req.message.body.stat = origin_stat;
  switch(req.message.body.stat.st_mode & S_IFMT) {
    case S_IFDIR:  
      cout << path << " is a dir\n";               
      break;
    case S_IFLNK: {
      cout << path << " is a symlink\n";  
      isSymLink = true;     
      break;
    }
    case S_IFREG: {
      md5_file(path.c_str(), req.message.body.hash);
      if (req.message.body.stat.st_nlink >= 2){
        existed_path = inode_map[req.message.body.stat.st_ino];
        if(existed_path == "") {
          inode_map[req.message.body.stat.st_ino] = path;
          
          cout << path << " is a regular file\n";
        }
        else {
          isHardLink = true;
          cout << path << " is a hard link\n";
          //cout << existed_path << "\n";
        }
      }
      else {
        cout << path << " is a regular file\n";
      }
      break;
    }
    default:       
      //printf("unknown?\n");                
      break;
  }
  
  //send pathlen to server so that server can know how many charachers it should receive
  //Please go to check the samplefunction in server
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "Client: Send Request Fail\n");
    return 0;
  }
  char *c_path = new char[path.length()];
  for(int i = 0; i < path.length(); ++i) c_path[i] = path[i];
  //send path
  if (!send_message(client->conn_fd, c_path, path.length())) {
    fprintf(stderr, "Client: Send %s Fail\n", c_path);
    return 0;
  }
  delete[] c_path;
  if(isSymLink) {
    char *linkto = new char[req.message.body.stat.st_size];
    memset(linkto, 0, sizeof(linkto));
    if(readlink(path.c_str(), linkto, req.message.body.stat.st_size) < 0){
      fprintf(stderr, "Client: Read Symlink Error\n");
      exit(EXIT_FAILURE);
    }       
    //fprintf(stderr, "size: %d\nsymlink: %s\n", req.message.body.stat.st_size, linkto);
    if (!send_message(client->conn_fd, linkto, req.message.body.stat.st_size)) {
      fprintf(stderr, "Client: Send Symlink: %s Fail\n", linkto);
      return 0;
    }
    delete[] linkto;
  }
  //delete[] c_path;
  //receive csiebox_protocol_header from server
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    //cout << header.res.status << "\n";
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      printf("SERVER: Meta Received. No Need to Sync File or HardLink.\n");
      return 1;
    } 
    else if(header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE && isHardLink) {
      cout << "SERVER: Please Sync Hard Link\n";
      sync_hardlink(client, existed_path);
    }
    else if(header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE) {
      cout << "SERVER: Please Sync File\n";
      sync_file(client, path);
      return 0;
    }
  }
  return 1;
}

void traverse(std::string upper_path, DIR *dir, csiebox_client* client, int cur, int& depth, std::string& longest_path, int fd) {
  struct dirent *dp;
  while((dp = readdir(dir)) != NULL) {
    // fprintf(stderr, dp->d_name);
    if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
    std::string d_name = dp->d_name;
    struct stat origin_stat;
    lstat((upper_path+d_name).c_str(), &origin_stat);
    sync_meta(client, upper_path+d_name, origin_stat);
    
    if((origin_stat.st_mode & S_IFMT) == S_IFLNK) continue;
    //start traverse
    if((origin_stat.st_mode & S_IFMT) == S_IFDIR){
      //cout << d_name << "\n";
      std::string deeper_path = upper_path + d_name;
      DIR *tmp = opendir((deeper_path).c_str());
      traverse(deeper_path+ "/", tmp, client, cur+1, depth, longest_path, fd);
      closedir(tmp);
      restore_time((deeper_path).c_str(), origin_stat);
      int wd = inotify_add_watch(fd, deeper_path.c_str() , IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
      inotify_map[wd] = deeper_path;
    }
    else restore_time((upper_path + d_name).c_str(), origin_stat);
    //long_path = long_path.length() > d_name.length()? long_path : d_name;
    if(cur > depth) {
      depth = cur;
      longest_path = upper_path + d_name;
    }
  }
  rewinddir(dir);
}

//traverse the dir tree
int traverse_dir_tree(csiebox_client* client, int fd) {
  remove("longestPath.txt");
  DIR *root = opendir(".");
  if(root == NULL) fprintf(stderr, "Client: Root Dir Open Error\n");
  int depth = -1;
  std::string longest_path = "";
  traverse("", root, client, 0, depth, longest_path, fd);
  //cout << longest_path << "\n";
  
  FILE* longest_file = fopen("longestPath.txt", "w+");
  fwrite(longest_path.c_str(), 1, longest_path.length(), longest_file);
  closedir(root);
  fclose(longest_file);
  struct stat longest_stat;
  lstat("longestPath.txt", &longest_stat);
  sync_meta(client, "longestPath.txt", longest_stat);
  int wd = inotify_add_watch(fd, "." , IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
  inotify_map[wd] = ".";

  struct stat root_stat;
  lstat(".", &root_stat);
  sync_meta(client, ".", root_stat);
  sync_end(client->conn_fd);
  cout << "Client: Traverse End\n";
}


//this is where client sends request, you sould write your code here
int csiebox_client_run(csiebox_client* client) {
  if (!login(client)) {
    fprintf(stderr, "SERVER: Login Fail\n");
    return 0;
  }
  fprintf(stderr, "SERVER: Login Success\n");
  

  //This is a sample function showing how to send data using defined header in common.h
  //You can remove it after you understand
  
  //sampleFunction(client);
  
  //====================
  //        TODO
  chdir(client->arg.path);
  int fd = inotify_init();
  traverse_dir_tree(client, fd);
  //cout << all << "\n";

  //inotify
  int length, i = 0;
  char buffer[EVENT_BUF_LEN];
  memset(buffer, 0, EVENT_BUF_LEN);
  while ((length = read(fd, buffer, EVENT_BUF_LEN)) > 0) {
    i = 0;
    while (i < length) {
      struct inotify_event* event = (struct inotify_event*)&buffer[i];
      std::string upper_path = inotify_map[event->wd];
      std::string new_path = "./" + upper_path + "/" + event->name;
      struct stat upper_dir_stat, stat;
      lstat(new_path.c_str(), &stat);
      lstat(upper_path.c_str(), &upper_dir_stat);
      //cout << "monitering dir path: " << inotify_map[event->wd] << "\n";
      if(memcmp(event->name, "process_end", 11) == 0) {
        close(fd);
        return 1;
      }
      //printf("event: (%d, %d, %s)\ntype: ", event->wd, strlen(event->name), event->name);
      if (event->mask & IN_CREATE) {
        printf("Client: %s is a New File or Dir\n", event->name);
        sync_meta(client, new_path, stat);
        sync_meta(client, upper_path, upper_dir_stat);
      }
      if (event->mask & IN_DELETE) {
        printf("Client: %s Has Been Deleted\n", event->name);
        sync_rm(client, new_path);
        sync_meta(client, upper_path, upper_dir_stat);
      }
      if (event->mask & IN_ATTRIB) {
        printf("Client: %s's Attribute Has Been Changed\n", event->name);
        sync_meta(client, new_path, stat);
      }
      if (event->mask & IN_MODIFY) {
        printf("Client: %s Has Been Modified\n", event->name);
        sync_meta(client, new_path, stat);
      }
      if ((event->mask & IN_ISDIR)){
        //printf("dir\n");
        if(event->mask & IN_CREATE) {
          printf("Client: Add Watch for %s\n", event->name);
          int wd = inotify_add_watch(fd, new_path.c_str() , IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
          inotify_map[wd] = new_path;
        }
        else if (event->mask & IN_DELETE) {
          //cout << "inotify map path: " << inotify_map[event->wd] << "\n";
          //cout << "deleted path: " << new_path << "\n";
        }
      } 
      else {
        //printf("file\n");
        inode_map[stat.st_ino] = new_path;

      }
      i += EVENT_SIZE + event->len;
      sync_end(client->conn_fd);
    }
    memset(buffer, 0, EVENT_BUF_LEN);
  }
  //====================
  return 1;
}

void csiebox_client_destroy(csiebox_client** client) {
  csiebox_client* tmp = *client;
  *client = 0;
  if (!tmp) {
    return;
  }
  close(tmp->conn_fd);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_client* client, int argc, char** argv) {
  if (argc != 2) {
    return 0;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    return 0;
  }
  fprintf(stderr, "reading config...\n");
  size_t keysize = 20, valsize = 20;
  char* key = (char*)malloc(sizeof(char) * keysize);
  char* val = (char*)malloc(sizeof(char) * valsize);
  ssize_t keylen, vallen;
  int accept_config_total = 5;
  int accept_config[5] = {0, 0, 0, 0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("name", key) == 0) {
      if (vallen <= sizeof(client->arg.name)) {
        strncpy(client->arg.name, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("server", key) == 0) {
      if (vallen <= sizeof(client->arg.server)) {
        strncpy(client->arg.server, val, vallen);
        accept_config[1] = 1;
      }
    } else if (strcmp("user", key) == 0) {
      if (vallen <= sizeof(client->arg.user)) {
        strncpy(client->arg.user, val, vallen);
        accept_config[2] = 1;
      }
    } else if (strcmp("passwd", key) == 0) {
      if (vallen <= sizeof(client->arg.passwd)) {
        strncpy(client->arg.passwd, val, vallen);
        accept_config[3] = 1;
      }
    } else if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(client->arg.path)) {
        strncpy(client->arg.path, val, vallen);
        accept_config[4] = 1;
      }
    }
  }
  free(key);
  free(val);
  fclose(file);
  int i, test = 1;
  for (i = 0; i < accept_config_total; ++i) {
    test = test & accept_config[i];
  }
  if (!test) {
    fprintf(stderr, "config error\n");
    return 0;
  }
  return 1;
}

static int login(csiebox_client* client) {
  csiebox_protocol_login req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  memcpy(req.message.body.user, client->arg.user, strlen(client->arg.user));
  md5(client->arg.passwd,
      strlen(client->arg.passwd),
      req.message.body.passwd_hash);
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "send fail\n");
    return 0;
  }
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_LOGIN &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      client->client_id = header.res.client_id;
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}
