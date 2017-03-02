CC     = cc
CFLAGS = -g -Wall

EXEC    = myfs
INC_DIR = -I include
SRC_DIR = src
OBJ_DIR = build
EXE_DIR = bin
TARGET  = $(EXE_DIR)/$(EXEC)

SRCS := $(foreach s_dir, $(SRC_DIR), $(wildcard $(s_dir)/*.c))
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS)) 

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(EXE_DIR)
	$(CC) $(CFLAGS) $(INC_DIR) -o $(TARGET) $(OBJS)
	@cp diskimage $(EXE_DIR)/diskimage

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC_DIR) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(EXE_DIR)

