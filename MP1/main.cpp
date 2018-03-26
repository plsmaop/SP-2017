#include "./list_file/list_file.h" // struct FileNames list_file(const char *directory_path)
#include "./md5/md5.h" // std::string md5(std::string)

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include <unistd.h>

void MD5(const char* dir, std::vector<std::string>& file_vector, std::vector<std::string>& MD5_vector) {
    for(auto i : file_vector) {
        /*char file_path[1000];
        sprintf(file_path, "%s/%s", dir, i.c_str());
        FILE* file = fopen(file_path, "rb");
        fseek(file, 0 , SEEK_END);
        size_t size = ftell(file);
        rewind(file);
        char* buf = new char[size];
        if(size < 8) fscanf(file, "%s", buf);
        else if(size != fread(buf, 1, size, file)) {
            fprintf(stderr, "open %s err\n", i.c_str());
            exit(1);
        }
        std::string tmp = buf;*/
        std::string file_md5 = md5(dir, i);
        MD5_vector.push_back(i);
        MD5_vector.push_back(file_md5);
        // std::cout << size << " " << strlen(buf) <<"\n";
        // delete[] buf;
        //fclose(file);
    }
}

FILE* Open(const char* dir = "", int mode = 1) {
    char path[300];
    // mode 3 write record
    if(mode == 3) {
        sprintf(path, "%s/.loser_record", dir);
        FILE* record = fopen(path, "w+");
        return record;
    }
    // mode 2 read config
    if(mode == 2) {
        sprintf(path, "%s/.loser_config", dir);
        FILE* config = fopen(path, "r");
        return config;
    }
    // default mode read record
    sprintf(path, "%s/.loser_record", dir);
    FILE* record = fopen(path, "r+");
    return record;
}

std::vector<std::string> List_File(const char* dir){
    std::vector<std::string> file_vector;
    // list file
    struct FileNames file_names = list_file(dir);
    if(file_names.length == -1) {
        fprintf(stderr, "list_file error\n");
        exit(1);
    }
    // store all filenames
    for(int i = 0; i < file_names.length; ++i) { 
        if(strcmp(file_names.names[i], ".loser_record") && strcmp(file_names.names[i], ".") && strcmp(file_names.names[i], "..")) {
            std::string tmp = file_names.names[i];
            file_vector.push_back(tmp);
        }
    }
    std::sort(file_vector.begin(), file_vector.end()); 
    free_file_names(file_names);
    return file_vector;
}

void fetch_record(FILE* record, unsigned long long number, std::string& record_string, int mode = 1) {
    char cmp = '#';
    // for compare
    if(mode == 2) cmp = ')';
    fseek(record, -1, SEEK_END);
    for(unsigned long long i = 0; i < number; ++i) {
        if(ftell(record) == 0) break;
        char* buffer = new char[1];
	    std::string commit_tmp = "";
	    // for each commit
        while ((fread(buffer,1, 1, record)) > 0) {
            fseek(record, -2, SEEK_CUR);
            //std::string tmp = buffer;
            commit_tmp += buffer[0];
            // 倒著印出來
            if(buffer[0] == cmp) {
                // 扣掉換行
                fseek(record, -1, SEEK_CUR);
                if(i != 0) record_string += '\n';
                for (std::string::reverse_iterator rit=commit_tmp.rbegin(); rit!=commit_tmp.rend(); ++rit)
                    record_string += *rit;
                break;
            }
        }
        // delete[] buffer;
    }
}

void Compare(FILE* record, std::vector<std::string>& MD5_vector, int mode) {
    std::unordered_map<std::string, std::string> MD5_map;
    std::vector<std::string> last_commit_vector;
    std::string last_commit = "";
    fetch_record(record, 1, last_commit);

    // get commit count
    size_t begin_pos = last_commit.find(' ', 2) + 1;
    size_t end_pos = last_commit.find('\n', begin_pos);
    std::string count;
    count.assign(last_commit.begin() + begin_pos, last_commit.begin() + end_pos);
    int commit_count = std::stoi(count) + 1;
    
    // std::cout << commit_count << "\n";
    begin_pos = last_commit.find(')', end_pos) + 1;
    end_pos = last_commit.find(' ', begin_pos);
    std::string file;
    std::string md5;
    // mapping md5 to file
    while(true) {
        file.assign(last_commit.begin() + begin_pos + 1, last_commit.begin() + end_pos);
        begin_pos = last_commit.find('\n', end_pos);
        md5.assign (last_commit.begin() + end_pos + 1, last_commit.begin() + begin_pos);
        MD5_map[md5] = file;
        last_commit_vector.push_back(file);
        last_commit_vector.push_back(md5);
        // std::cout << file << " " << md5 << "\n";
        end_pos = last_commit.find(' ', begin_pos);
        if(end_pos == std::string::npos) break;
    }
    // compare
    std::string new_file = "[new_file]\n";
    std::string modified = "[modified]\n";
    std::string copied = "[copied]\n";
    md5 = "(MD5)\n";

    unsigned int i = 0, j = 0;
    bool isChange = false;
    while(i < last_commit_vector.size() && j < MD5_vector.size()) {
        // new file or copied 
        if(last_commit_vector[i] != MD5_vector[j]) {
            isChange = true;
            std::string file_name = MD5_map[MD5_vector[j+1]];
            // new file
            if(file_name == "") {
                new_file = new_file + MD5_vector[j] + "\n";
            }
            // copied
            else if(file_name != MD5_vector[j]) {
                copied = copied + file_name +  " => " + MD5_vector[j] + "\n";
            }
            md5 = md5 +  MD5_vector[j] + " " + MD5_vector[j+1] + "\n";
            j = j + 2;
            continue;
        }
        // modified
        else if(last_commit_vector[i] == MD5_vector[j]) {
            // md5 changed
            if(last_commit_vector[i+1] != MD5_vector[j+1]) {
                // std::cout << "new: " <<MD5_vector[j]<< " " << MD5_vector[j+1] << "\n";
                // std::cout << "old: " <<last_commit_vector[i] << " " << last_commit_vector[i+1] << "\n";
                modified = modified + MD5_vector[j] + "\n";
                md5 = md5 +  MD5_vector[j] + " " + MD5_vector[j+1] + "\n";
                isChange = true;
            }
            // same file
            else md5 = md5 + last_commit_vector[i] + " " + last_commit_vector[i+1] + "\n" ;
            i = i + 2;
            j = j + 2;
        }
        // last commit 檢查完了，但新的狀態還有剩
        if(i == last_commit_vector.size()) {
            while(j < MD5_vector.size()) {
                isChange = true;
                std::string file_name = MD5_map[MD5_vector[j+1]];
                if(file_name == "") {
                    new_file = new_file + MD5_vector[j] + "\n";
                }
                // copied
                else if(file_name != MD5_vector[j]) {
                    copied = copied + file_name +  " => " + MD5_vector[j] + "\n";
                }
                md5 = md5 +  MD5_vector[j] + " " + MD5_vector[j+1] + "\n";
                j = j + 2;
            }
            break;
        }
    }
    // mode 1 : status
    if(mode == 1) std::cout << new_file << modified << copied;
    // mode 2 : commit
    else if(mode == 2 && isChange) {
        fseek(record, 0 ,SEEK_END);
        std::string write_commit = "\n# commit " + std::to_string(commit_count) + "\n"
            + new_file + modified + copied + md5;
        fwrite(write_commit.c_str(), 1, write_commit.size(), record);
    }
    /*else {
        fprintf(stderr, "compare mode error\n");
        exit(1);
    }*/
}

void Status(const char* dir) {
    FILE* record = Open(dir);
    std::vector<std::string> file_vector = List_File(dir);
    // print status
    if(record == NULL) {
        std::cout << "[new_file]\n";
        for(auto i : file_vector) std::cout << i << "\n";
        std::cout << "[modified]\n[copied]\n";
    }
    else {
        std::vector<std::string> MD5_vector;
        MD5(dir, file_vector, MD5_vector);
        Compare(record, MD5_vector, 1);
    }
    //fclose(record);
}

void Commit(const char* dir) {
    std::vector<std::string> file_vector = List_File(dir);
    if(file_vector.size() == 0) exit(0);
    FILE* record = Open(dir); // 2 for r+
    std::vector<std::string> MD5_vector;
    MD5(dir, file_vector, MD5_vector);
    // .loser_record 不存在
    if(record == NULL) {
        record = Open(dir, 3);
        std::string tmp = "# commit 1\n[new_file]\n";
        for(auto i : file_vector) tmp = tmp + i + "\n";
        tmp += "[modified]\n[copied]\n(MD5)\n";
        for(unsigned int i = 0; i < MD5_vector.size(); i = i+2) tmp = tmp + MD5_vector[i] + " " + MD5_vector[i+1] + "\n";
        fwrite(tmp.c_str(), 1, tmp.size(), record);
        //fclose(record);
    }
    // .loser_record 存在
    else Compare(record, MD5_vector, 2);
}

void Log(unsigned long long number, const char* dir) {
    if(number == 0) return;
    FILE* record = Open(dir);
    if(record == NULL) {
        // fprintf(stderr, "record doesn't exist\n");
        exit(0);
    }
    std::string log_output = "";
    fetch_record(record, number, log_output);
    std::cout << log_output;
    //fclose(record);
}

void Action(std::string action_type, const char* dir, unsigned long long n = 0) {
    if(action_type == "status") Status(dir);
    else if(action_type == "commit") Commit(dir);
    else if (action_type == "log") Log(n, dir);
    else std:: cout << "無 " << action_type << " 子指令\n";
}



std::string Update_action(std::string action, const char* dir) {
    if(action == "commit" || action == "status") return action;
    FILE* config = Open(dir, 2);
    if(config == NULL) return action;
    fseek(config, 0, SEEK_END);
    size_t size = ftell(config);
    rewind(config);
    char* buf = new char[size];
    if(fread(buf, 1, size, config) != size) {
        fprintf(stderr, "read config error\n");
        exit(1);
    }
    std::string tmp = "";
    while(*buf) {
        if(*buf == ' ') {
            buf+=3;
            if(tmp == action) {
                switch(*buf) {
                    case 's':
                        return "status";
                    case 'c':
                        return "commit";
                    case 'l':
                        return "log";
                    default:
                        break;
                }
            }
            while(*buf != '\n') ++buf;
            ++buf;
            tmp = "";
            continue;
        }
        tmp += *buf;
        ++buf;
    }
    return action;
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cout << "用法：\n./loser status <目錄>\n./loser commit <目錄>\n./loser log <數量> <目錄>\n";
        exit(0);
    }
    // start
    std::string action = argv[1];
    if(argc == 3) {
        const char* dir = argv[2];
        Action(Update_action(argv[1], dir), dir);
    }
    // log
    else if(argc == 4) {
        std::string n = argv[2];
        unsigned long long number = std::stoll(n);
        const char* dir = argv[3];
        Action(Update_action(argv[1], dir), dir, number);
    }
    else {
        fprintf(stderr, "what is wrong with the fucking parameter???\n");
        exit(1);
    }
}