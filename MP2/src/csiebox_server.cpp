#include "csiebox_server.h"

#include "csiebox_common.h"
#include "connect.h"

#include <cstdio>
#include <cstring>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdlib>
#include <utime.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <iostream>
#include <vector>
using namespace std;

static int parse_arg(csiebox_server* server, int argc, char** argv);
static void handle_request(csiebox_server* server, int conn_fd);
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info);
static void login(
  csiebox_server* server, int conn_fd, csiebox_protocol_login* login);
static void logout(csiebox_server* server, int conn_fd);
static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info);

#define DIR_S_FLAG (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)//permission you can use to create new file
#define REG_S_FLAG (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)//permission you can use to create new directory

struct dir_stat {
  struct stat dir;
  std::string path;
};


//read config file, and start to listen
void csiebox_server_init(
  csiebox_server** server, int argc, char** argv) {
  csiebox_server* tmp = (csiebox_server*)malloc(sizeof(csiebox_server));
  if (!tmp) {
    fprintf(stderr, "server malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_server));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = server_start();
  if (fd < 0) {
    fprintf(stderr, "server fail\n");
    free(tmp);
    return;
  }
  tmp->client = (csiebox_client_info**)
      malloc(sizeof(csiebox_client_info*) * getdtablesize());
  if (!tmp->client) {
    fprintf(stderr, "client list malloc fail\n");
    close(fd);
    free(tmp);
    return;
  }
  memset(tmp->client, 0, sizeof(csiebox_client_info*) * getdtablesize());
  tmp->listen_fd = fd;
  *server = tmp;
}

//wait client to connect and handle requests from connected socket fd
int csiebox_server_run(csiebox_server* server) {
  int conn_fd, conn_len;
  struct sockaddr_in addr;
  while (1) {
    memset(&addr, 0, sizeof(addr));
    conn_len = 0;
    // waiting client connect
    conn_fd = accept(
      server->listen_fd, (struct sockaddr*)&addr, (socklen_t*)&conn_len);
    if (conn_fd < 0) {
      if (errno == ENFILE) {
          fprintf(stderr, "out of file descriptor table\n");
          continue;
        } else if (errno == EAGAIN || errno == EINTR) {
          continue;
        } else {
          fprintf(stderr, "accept err\n");
          fprintf(stderr, "code: %s\n", strerror(errno));
          break;
        }
    }
    // handle request from connected socket fd
    handle_request(server, conn_fd);
  }
  return 1;
}

void csiebox_server_destroy(csiebox_server** server) {
  csiebox_server* tmp = *server;
  *server = 0;
  if (!tmp) {
    return;
  }
  close(tmp->listen_fd);
  int i = getdtablesize() - 1;
  for (; i >= 0; --i) {
    if (tmp->client[i]) {
      free(tmp->client[i]);
    }
  }
  free(tmp->client);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_server* server, int argc, char** argv) {
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
  int accept_config_total = 2;
  int accept_config[2] = {0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(server->arg.path)) {
        strncpy(server->arg.path, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("account_path", key) == 0) {
      if (vallen <= sizeof(server->arg.account_path)) {
        strncpy(server->arg.account_path, val, vallen);
        accept_config[1] = 1;
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

int handle_sync_file(int conn_fd, csiebox_protocol_file* file, std::string path) {

  //send status to client
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
  header.res.datalen = 0;
  header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;

  //use the datalen from client to recv data 
  //cout << "datalen: " << file->message.body.datalen << "\n";
  char *buf = new char[file->message.body.datalen];
  memset(buf, 0, sizeof(buf));
  recv_message(conn_fd, buf, file->message.body.datalen);
  //cout << "file received\nopen " << path << "\n";
  
  FILE* sync_file = fopen(path.c_str(), "w");
  if(sync_file == NULL) {
    fprintf(stderr, "SERVER: Open %s Error\n", path.c_str());
    return 0;
  }
  if((fwrite(buf, 1, file->message.body.datalen, sync_file)) != file->message.body.datalen){
    fprintf(stderr, "SERVER: Write %s Fail\n", path.c_str());
    header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
  }
  fclose(sync_file);
  delete[] buf;
  if(!send_message(conn_fd, &header, sizeof(header))){
    fprintf(stderr, "SERVER: Send Response Fail\n");
    return 0;
  }
  cout << "\n";
  return 1;
}

int handle_sync_hardlink(int conn_fd, csiebox_protocol_hardlink* hardlink, std::string path) {

  //send status to client
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
  header.res.datalen = 0;
  header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;

  //use the targetlen from client to recv target path 
  char *buf = new char[hardlink->message.body.targetlen+1];
  memset(buf, 0, sizeof(buf));
  recv_message(conn_fd, buf, hardlink->message.body.targetlen);
  //cout << hardlink->message.body.targetlen << "\n";
  buf[hardlink->message.body.targetlen] = '\0';
  //cout << "target path: " << buf << "\n";
  char clean_buf[hardlink->message.body.targetlen+1]; 

  //cout << "size of file: " << strlen(buf) << "\n";
  if(link(buf, path.c_str()) < 0) {
    fprintf(stderr, "SERVER: Link %s Error\n", buf);
    return 0;
  }
  delete[] buf;
  if(!send_message(conn_fd, &header, sizeof(header))){
    fprintf(stderr, "SERVER: Send Response Fail\n");
    return 0;
  }
  cout << "\n";
  return 1;
}

int handle_sync_meta(int conn_fd, csiebox_protocol_meta* meta, std::string& path) {
  //response
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
  header.res.datalen = 0;
  header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
  
  //use the pathlen from client to recv path 
  char buf[400];
  memset(buf, 0, sizeof(buf));
  recv_message(conn_fd, buf, meta->message.body.pathlen);
  buf[meta->message.body.pathlen] = '\0';
  path = buf;
  //cout << "path: " << buf << "\n";
  switch(meta->message.body.stat.st_mode & S_IFMT) {
    case S_IFDIR: {
      cout << buf << " is a dir\n";   
      DIR *dp = opendir(buf);
      if(dp == NULL) {
        closedir(dp);
        cout << "SERVER: Create Dir\n";
        if(mkdir(buf, meta->message.body.stat.st_mode) == -1) {
          fprintf(stderr, "SERVER: Create Dir Error\n");
          exit(EXIT_FAILURE);
        }
        //cout << "dir has been created!!\n";
      }     
      else closedir(dp);       
      break;
    }
    case S_IFLNK: { 
      cout << buf << " is a symlink\n";
      char *existed_path = new char[meta->message.body.stat.st_size+1];
      memset(existed_path, 0, sizeof(existed_path));
      recv_message(conn_fd, existed_path, meta->message.body.stat.st_size);
      struct stat symlink_target;
      if(lstat(buf, &symlink_target) != -1) {
        fprintf(stderr, "SERVER: Cannot Symlink Since File Target Existed\n");
        break;
      }
      //std::string target = existed_path;
      existed_path[meta->message.body.stat.st_size] = '\0';
      //cout << "size: " << meta->message.body.stat.st_size << "\n";
      //cout << "target path: " << target << "\n";
      //cout << existed_path << "\n";
      if(strncmp(existed_path, "../cdir/", 8) == 0) {
        strncpy(existed_path, &existed_path[8], meta->message.body.stat.st_size - 7);
      }
      else if(strncmp(existed_path, "cdir/", 5) == 0) {
        strncpy(existed_path, &existed_path[5], meta->message.body.stat.st_size - 4);
      } 
      if(symlink(existed_path, buf) < 0) {
        fprintf(stderr, "symlink error\n");
        exit(EXIT_FAILURE);
      }
      delete[] existed_path;
      break;
    }
    case S_IFREG: {
      cout << buf << " is a regular file\n";
      struct stat reg_target;
      if(lstat(buf, &reg_target) < 0) {
        printf("SERVER: File Doesn't Exist! Please Sync File\n");
        header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
        break;
      }
      uint8_t hash[MD5_DIGEST_LENGTH];
      memset(&hash, 0, sizeof(hash));
      md5_file(buf, hash);
      /*cout << target_path << "\n";
      cout << strlen((char*)hash) << "\n";*/
      if (memcmp(hash, meta->message.body.hash, sizeof((char*)hash)) == 0) {
        printf("%s's hashes are equal!\n", buf);
        header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
      }
      else {
        printf("SERVER: File Has Been Modified! Please Sync File\n");
        //cout << "client: " << meta->message.body.hash;
        //cout << "\nserver: " << hash << "\n";
        header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
      }
      break;
    }
    default:       
      //printf("unknown?\n");                
      break;
  }

  cout << "\n";
  //send message
  if(!send_message(conn_fd, &header, sizeof(header))){
    fprintf(stderr, "send fail\n");
    return 0;
  }
  //cout << "header has been sent\n";
  return 1;
}

int handle_sync_rm(int conn_fd, csiebox_protocol_rm* rm) {
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_RM;
  header.res.datalen = 0;
  header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
  
  //receive path
  char *buf = new char[rm->message.body.pathlen+1];
  memset(buf, 0, sizeof(buf));
  recv_message(conn_fd, buf, rm->message.body.pathlen);
  buf[rm->message.body.pathlen] = '\0';

  if(remove(buf) == -1) {
    fprintf(stderr, "SERVER: %s Remove Error\n", buf);
    header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
  }
  fprintf(stderr, "SERVER: %s Has Been Removed\n", buf);
  delete[] buf;
  if(!send_message(conn_fd, &header, sizeof(header))){
    fprintf(stderr, "SERVER: Send Header Fail\n");
    return 0;
  }
  cout << "\n";
  return 1;
}

//this is where the server handle requests, you should write your code here
static void handle_request(csiebox_server* server, int conn_fd) {
  vector<struct dir_stat> dir_stat_stack;
  std::string path ="";
  //struct stat current_stat;
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  while (recv_message(conn_fd, &header, sizeof(header))) {
    //cout << std::hex << static_cast<int>(header.req.magic) << "\n";
    if (header.req.magic != CSIEBOX_PROTOCOL_MAGIC_REQ) {
      //cout << "not req\n";
      continue;
    }
    switch (header.req.op) {
      case CSIEBOX_PROTOCOL_OP_LOGIN: {
        fprintf(stderr, "SERVER: Login\n");
        csiebox_protocol_login req;
        if (complete_message_with_header(conn_fd, &header, &req)) {
          login(server, conn_fd, &req);
        }
        break;
      }
      case CSIEBOX_PROTOCOL_OP_SYNC_META: {
        fprintf(stderr, "SERVER: Sync Meta\n");
        csiebox_protocol_meta meta;
        if (complete_message_with_header(conn_fd, &header, &meta)) {
          //sampleFunction(conn_fd, &meta); 
          //====================
          //        TODO

          handle_sync_meta(conn_fd, &meta, path);
          //current_stat = meta.message.body.stat;
          struct dir_stat stat_tmp;
          stat_tmp.dir = meta.message.body.stat;
          stat_tmp.path = path;
          dir_stat_stack.push_back(stat_tmp);
          //====================
        }
        break;
      }
      case CSIEBOX_PROTOCOL_OP_SYNC_FILE: {
        fprintf(stderr, "SERVER: Sync File\n");
        csiebox_protocol_file file;
        if (complete_message_with_header(conn_fd, &header, &file)) {
          //====================
          //        TODO
          handle_sync_file(conn_fd, &file, path);
          //====================
        }
        break;
      }
      case CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK: {
        fprintf(stderr, "SERVER: Sync Hardlink\n");
        csiebox_protocol_hardlink hardlink;
        if (complete_message_with_header(conn_fd, &header, &hardlink)) {
          //====================
          //        TODO
          handle_sync_hardlink(conn_fd, &hardlink, path);
          //====================
        }
        break;
      }
      case CSIEBOX_PROTOCOL_OP_SYNC_END: {
        fprintf(stderr, "SERVER: Sync End\n\n");
        csiebox_protocol_header end;
          //====================
          //        TODO
          for(int i = dir_stat_stack.size()-1; i >= 0; --i) {
            const char* buf = dir_stat_stack[i].path.c_str();
            struct stat stat = dir_stat_stack[i].dir;
            //struct stat target_stat;
            //lstat(buf, &target_stat);
            DIR *cur = opendir(".");
            int dp = dirfd(cur);
            struct timespec new_times[2];
            new_times[0] = stat.st_atim; 
            new_times[1] = stat.st_mtim;  
            if(utimensat(dp, buf, new_times, AT_SYMLINK_NOFOLLOW) < 0) { 
              perror("SERVER: Update Time error");
              exit(EXIT_FAILURE);
            }
        
            //cout << "sync mode\n";
            if((stat.st_mode & S_IFMT) != S_IFLNK) {
              chmod(buf, stat.st_mode);
              chown(buf, stat.st_uid, stat.st_gid);
            }
            else {
              fchmodat(dp, buf, stat.st_mode, AT_SYMLINK_NOFOLLOW);
              lchown(buf, stat.st_uid, stat.st_gid);
            }
            closedir(cur);
          }
          /*for(int i = dir_stat_stack.size()-1; i >= 0; --i) {
            const char* buf = dir_stat_stack[i].path.c_str();
            struct stat stat = dir_stat_stack[i].dir;
            struct stat target_stat;
            lstat(buf, &target_stat);
        
            if(stat.st_atime != target_stat.st_atime) cout << buf << "'s time ins't synced.\n";
          }*/
          dir_stat_stack.clear();
          path = "";
          //====================
        break;
      }
      case CSIEBOX_PROTOCOL_OP_RM: {
        fprintf(stderr, "SERVER: Sync Remove\n");
        csiebox_protocol_rm rm;
        if (complete_message_with_header(conn_fd, &header, &rm)) {
          //====================
          //        TODO
          handle_sync_rm(conn_fd, &rm);
          //====================
        }
        break;
      }
      default:
        fprintf(stderr, "unknown op %x\n", header.req.op);
        break;
    }
  }
  fprintf(stderr, "SERVER: End of Connection\n");
  logout(server, conn_fd);
  chdir("../../bin");
}

//open account file to get account information
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info) {
  FILE* file = fopen(server->arg.account_path, "r");
  if (!file) {
    return 0;
  }
  size_t buflen = 100;
  char* buf = (char*)malloc(sizeof(char) * buflen);
  memset(buf, 0, buflen);
  ssize_t len;
  int ret = 0;
  int line = 0;
  while ((len = getline(&buf, &buflen, file) - 1) > 0) {
    ++line;
    buf[len] = '\0';
    char* u = strtok(buf, ",");
    if (!u) {
      fprintf(stderr, "illegal form in account file, line %d\n", line);
      continue;
    }
    if (strcmp(user, u) == 0) {
      memcpy(info->user, user, strlen(user));
      char* passwd = strtok(NULL, ",");
      if (!passwd) {
        fprintf(stderr, "illegal form in account file, line %d\n", line);
        continue;
      }
      md5(passwd, strlen(passwd), (uint8_t*)info->passwd_hash);
      ret = 1;
      break;
    }
  }
  free(buf);
  fclose(file);
  return ret;
}

//handle the login request from client
static void login(
  csiebox_server* server, int conn_fd, csiebox_protocol_login* login) {
  int succ = 1;
  csiebox_client_info* info =
    (csiebox_client_info*)malloc(sizeof(csiebox_client_info));
  memset(info, 0, sizeof(csiebox_client_info));
  if (!get_account_info(server, (char*)login->message.body.user, &(info->account))) {
    fprintf(stderr, "cannot find account\n");
    succ = 0;
  }
  if (succ &&
      memcmp(login->message.body.passwd_hash,
             info->account.passwd_hash,
             MD5_DIGEST_LENGTH) != 0) {
    fprintf(stderr, "passwd miss match\n");
    succ = 0;
  }

  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  header.res.datalen = 0;
  if (succ) {
    if (server->client[conn_fd]) {
      free(server->client[conn_fd]);
    }
    info->conn_fd = conn_fd;
    server->client[conn_fd] = info;
    header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    header.res.client_id = info->conn_fd;
    char* homedir = get_user_homedir(server, info);
    mkdir(homedir, DIR_S_FLAG);
    free(homedir);
  } else {
    header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
    free(info);
  }
  chdir(server->arg.sync_path);
  send_message(conn_fd, &header, sizeof(header));
}

static void logout(csiebox_server* server, int conn_fd) {
  free(server->client[conn_fd]);
  server->client[conn_fd] = 0;
  close(conn_fd);
}

static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info) {
  char* ret = (char*)malloc(sizeof(char) * PATH_MAX);
  memset(ret, 0, PATH_MAX);
  sprintf(ret, "%s/%s", server->arg.path, info->account.user);
  strncpy(server->arg.sync_path, ret, strlen(ret));
  return ret;
}

