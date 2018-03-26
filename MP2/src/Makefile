TARGET=port_register csiebox_client csiebox_server inotify_test
INCLUDE=-I../include
BIN=../bin
FLAG=-g
GCC=g++ -std=c++11
GC=gcc
#for c++
#GCC=g++
PORT_REGISTER_OBJS=port_register.o connect.o hash.o
CSIEBOX_SERVER_OBJS=csiebox_server_main.o csiebox_server.o connect.o csiebox_common.o
CSIEBOX_CLIENT_OBJS=csiebox_client_main.o csiebox_client.o connect.o csiebox_common.o
INOTIFY_TEST_OBJS=inotify_test.o
LIB=-lm -lbsd
.PHONY: clean
all: $(TARGET)

%.o: %.cpp
	@echo "making: " $@ 
	@$(GCC) $< -c $(FLAG) $(INCLUDE) -O2
%.o: %.c
	@echo "making: " $@
	@$(GC) $< -c $(FLAG) $(INCLUDE) -O2
clean:
	rm -rf *.o
	@for bin in $(TARGET); do \
		echo "rm -rf $(BIN)/$$bin"; \
		rm -rf $(BIN)/$$bin; \
	done
port_register: $(PORT_REGISTER_OBJS)
	@echo "making: " $@
	@$(GCC) -o $(BIN)/$@ $(FLAG) $(INCLUDE) $(PORT_REGISTER_OBJS) -O2
csiebox_client: $(CSIEBOX_CLIENT_OBJS)
	@echo "making: " $@
	@$(GCC) -o $(BIN)/$@ $(FLAG) $(INCLUDE) $(CSIEBOX_CLIENT_OBJS) $(LIB) -O2
csiebox_server: $(CSIEBOX_SERVER_OBJS)
	@echo "making: " $@
	@$(GCC) -o $(BIN)/$@ $(FLAG) $(INCLUDE) $(CSIEBOX_SERVER_OBJS) $(LIB) -O2
inotify_test: $(INOTIFY_TEST_OBJS)
	@echo "making: " $@
	@$(GCC) -o $(BIN)/$@ $(FLAG) $(INCLUDE) $(INOTIFY_TEST_OBJS) $(LIB) -O2

