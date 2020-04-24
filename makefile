CC      := clang++
FLAGS 	:= -Wall -Werror -Wshadow -Wextra -std=c++11 -ggdb3 -DDEBUG 
LINK  	:= -lSDL2 -pthread
OBJ_DIR := ./objects
EXE   	:= game
SRC     := $(wildcard *.cpp) \

OBJECTS := $(SRC:%.cpp=$(OBJ_DIR)/%.o)

# Tested and working on mac, untested on windows and linux
ifeq ($(OS),Windows_NT)
	LINK += -lGL
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		LINK += -lGL

	endif
	ifeq ($(UNAME_S),Darwin)
		LINK += -framework OpenGL

	endif
endif

all: $(EXE)

$(EXE): $(OBJECTS)
	$(CC) -o $(EXE) $^ $(LINK)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -c $< -o $@

.PHONY: all clean release

release: FLAGS += -O3
release: all

clean:
	-@rm -rvf $(EXE) $(OBJ_DIR)/*