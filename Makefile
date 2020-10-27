CXX = g++
CXXFLAGS = -g

CMD_SRC = commands
CMD = noop number removetag removetag0
BIN_DIR = bin

all: npshell

npshell: shell.cpp pipe.cpp
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
	rm -rf npshell $(BIN_DIR)

