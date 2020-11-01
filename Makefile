CXX = g++
CXXFLAGS = -g

CMD_SRC = commands
CMD = noop number removetag removetag0
BIN_DIR = bin
SHELLS = np_simple np_single_proc

all: $(SHELLS)

np_simple: task1.cpp shell.cpp pipe.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

np_single_proc: task2.cpp shell.cpp pipe.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

build_cmd: unix_cmd $(CMD)

unix_cmd: 
	mkdir -p $(BIN_DIR)
	cp /bin/ls $(BIN_DIR)
	cp /bin/cat $(BIN_DIR)

$(CMD): %: $(CMD_SRC)/%.cpp
	$(CXX) -o $(BIN_DIR)/$@ $^

.PHONY: clean
clean:
	rm -rf $(SHELLS) $(BIN_DIR)

