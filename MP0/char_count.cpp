#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace std;
bool string_table[95];
inline int readchar() {
    const int N = 510000000;
    static char buf[N];
    static char *p = buf, *end = buf;
    if(p == end) {
        if((end = buf + fread(buf, 1, N, stdin)) == buf) return EOF;
        p = buf;
    }
    return *p++;
}

void check_file(FILE* input) {
    long long count = 0;
    fseek (input, 0, SEEK_END);
    long long input_size;
    input_size = ftell(input);
    rewind(input);
    char* buffer = new char[sizeof(char)*input_size];
    size_t in = fread(buffer, 1, input_size, input);
    if(in != input_size) {
        fprintf(stderr, "input error\n");
        exit(1);
    }
    while(*buffer) {
        if(*buffer == '\n') {
            printf("%lld\n", count);
            count = 0;
        }
        else {
            int tmp = *buffer;
            tmp = tmp - 32;
            if(string_table[tmp]) ++count;
        }
        ++buffer;
    }
}

void check_input() {
    int tmp;
    long long count = 0;
    vector<long long> ans;
    while( (tmp = readchar()) != EOF) {
        if(tmp == 10) {
            ans.push_back(count);
            count = 0;
        }
        else {
            if(string_table[tmp-32]) ++count;
        }
    }
    for(auto i: ans) printf("%lld\n", i);
}

int main(int argc, char *argv[]) {
    string STRINGSET;
    STRINGSET = argv[1];
    for(auto i: STRINGSET) {
        int tmp = i;
        tmp = tmp - 32;
        string_table[tmp] = true;
    }
    if(argv[2]) {
        FILE* input = fopen(argv[2], "r");
        if(input == NULL) {
            fprintf(stderr, "error\n");
            exit(1);
        }
        else check_file(input);
        fclose(input);
    }
    else check_input();
    return 0;
}