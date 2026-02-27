# --- Configurazione CPLEX (Mac ARM64) ---
CPLEX_PATH    := /Applications/CPLEX_Studio2211/cplex
CPLEX_INCLUDE := $(CPLEX_PATH)/include/ilcplex
CPLEX_LIB     := $(CPLEX_PATH)/lib/arm64_osx/static_pic

# --- Configurazione Standard ---
CC      := gcc
CFLAGS  := -Wall -Wextra -O0 -g -Iinclude -I$(CPLEX_INCLUDE) -std=c11
LDFLAGS := -L$(CPLEX_LIB) -lcplex -lpthread -lm -ldl 

# --- Cartelle ---
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
DATA_DIR := data

# --- Output ---
TARGET  := $(BIN_DIR)/solver

# --- Colori per il terminale ---
CYAN   := \033[0;36m
GREEN  := \033[0;32m
YELLOW := \033[0;33m
RED    := \033[0;31m
NC     := \033[0m # No Color

# --- File ---
SOURCES := $(shell find $(SRC_DIR) -name '*.c')
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)



# --- Regole ---

# Compila ed esegue l'analisi
all: setup $(TARGET) 

setup:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR) $(DATA_DIR)

# Linker
$(TARGET): $(OBJECTS)
	@echo "$(CYAN)Linking...$(NC)"
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "$(GREEN)🟢 Build Successful: $@ $(NC)"

# Compilatore
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CC) $(CFLAGS) -c $< -o $@


# Pulizia
clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "$(RED)Repository cleaned.$(NC)"

# Comando comodo per vedere i file presenti
list:
	@echo "$(CYAN)Current instances in $(DATA_DIR):$(NC)"
	@ls $(DATA_DIR)

.PHONY: all clean check setup list