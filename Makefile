# --- Configurazione CPLEX (Mac ARM64) ---
CPLEX_PATH    := /Applications/CPLEX_Studio2211/cplex
CPLEX_INCLUDE := $(CPLEX_PATH)/include/ilcplex
CPLEX_LIB     := $(CPLEX_PATH)/lib/arm64_osx/static_pic

# --- Configurazione Standard ---
# Default optimization flags (used if you just type 'make')
OPT_FLAGS ?= -O0 -g

CC      := gcc
# Note the use of '=' instead of ':=' so the flags can update dynamically
CFLAGS  = -Wall -Wextra $(OPT_FLAGS) -Iinclude -I$(CPLEX_INCLUDE) -std=c11 -MMD -MP
LDFLAGS = -L$(CPLEX_LIB) -lcplex -lpthread -lm -ldl 

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
	@rm -rf *.png
	@echo "$(RED)Repository cleaned.$(NC)"

# Comando comodo per vedere i file presenti
list:
	@echo "$(CYAN)Current instances in $(DATA_DIR):$(NC)"
	@ls $(DATA_DIR)

# --- Profili di Build Aggiuntivi ---

release: OPT_FLAGS := -O3 -DNDEBUG
release: all
	@echo "$(GREEN)🟢 Release build complete (Optimized for speed).$(NC)"

debug: OPT_FLAGS := -O0 -g
debug: all
	@echo "$(YELLOW)🟡 Debug build complete.$(NC)"

asan: OPT_FLAGS := -O1 -g -fsanitize=address -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address
asan: all
	@echo "$(RED)🔴 AddressSanitizer build complete. Run the program to check for memory leaks.$(NC)"

run: $(TARGET)
	@echo "$(CYAN)Running quick test...$(NC)"
	./$(TARGET) -file $(DATA_DIR)/a280.tsp -2opt -verbose 2

.PHONY: all clean check setup list release debug asan run

# --- Tracciamento Dipendenze ---

-include $(OBJECTS:.o=.d)