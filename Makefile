CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -O1 -Isrc -I/opt/homebrew/opt/openssl@3/include
LDFLAGS := -L/opt/homebrew/opt/openssl@3/lib
LDLIBS := -lpthread -lssl -lcrypto

BIN := bin
BUILD := build
CORE := $(BUILD)/vault.o $(BUILD)/protocol.o $(BUILD)/memory_simulator.o $(BUILD)/scheduler.o $(BUILD)/job_queue.o

.PHONY: all clean test run-server run-client

all: $(BIN)/sentinelvault_server $(BIN)/sentinelvault_client $(BIN)/memory_demo $(BIN)/scheduler_demo $(BIN)/process_demo $(BIN)/permission_demo
	@echo "Build successful"

$(BIN) $(BUILD):
	mkdir -p $@

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/sentinelvault_server: $(BUILD)/server_main.o $(BUILD)/server.o $(CORE) | $(BIN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/sentinelvault_client: $(BUILD)/client.o | $(BIN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/memory_demo: $(BUILD)/memory_main.o $(BUILD)/memory_simulator.o | $(BIN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/scheduler_demo: $(BUILD)/scheduler_main.o $(BUILD)/scheduler.o $(BUILD)/job_queue.o | $(BIN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/process_demo: $(BUILD)/process_main.o $(BUILD)/process_demo.o | $(BIN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/permission_demo: $(BUILD)/file_permission_main.o $(BUILD)/file_permission_demo.o | $(BIN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/test_memory: tests/test_memory.c $(BUILD)/memory_simulator.o | $(BIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/test_vault: tests/test_vault.c $(BUILD)/vault.o | $(BIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/test_protocol: tests/test_protocol.c $(CORE) | $(BIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN)/test_process_file: tests/test_process_file.c $(BUILD)/process_demo.o $(BUILD)/file_permission_demo.o | $(BIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: all $(BIN)/test_memory $(BIN)/test_vault $(BIN)/test_protocol $(BIN)/test_process_file
	./$(BIN)/test_memory
	./$(BIN)/test_vault
	./$(BIN)/test_protocol
	./$(BIN)/test_process_file

run-server: all
	./$(BIN)/sentinelvault_server 9090

run-client: all
	./$(BIN)/sentinelvault_client 127.0.0.1 9090

clean:
	rm -rf $(BIN) $(BUILD)
