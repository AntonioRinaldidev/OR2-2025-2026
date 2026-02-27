# --- Configurazione ---
CC      := gcc
CFLAGS  := -Wall -Wextra -O0 -g -Iinclude -fsanitize=address -std=c11
LDFLAGS := -lm -fsanitize=address

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
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)



# --- Regole ---

# Compila ed esegue l'analisi
all: setup $(TARGET) check

setup:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR) $(DATA_DIR)

# Linker
$(TARGET): $(OBJECTS)
	@echo "$(CYAN)Linking...$(NC)"
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "$(GREEN)🟢 Build Successful: $@ $(NC)"

# Compilatore
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CC) $(CFLAGS) -c $< -o $@

# Esecuzione con controllo Leak e scelta istanza
check: $(TARGET)
	@echo "$(CYAN)Running Analysis (AddressSanitizer)...$(NC)"
	@echo "$(CYAN)Testing instance: $(DATA_DIR)/$(INSTANCE)$(NC)"
	@ASAN_OPTIONS=detect_leaks=1 ./$(TARGET) $(DATA_DIR)/$(INSTANCE)

# Pulizia
clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "$(RED)Repository cleaned.$(NC)"

# Comando comodo per vedere i file presenti
list:
	@echo "$(CYAN)Current instances in $(DATA_DIR):$(NC)"
	@ls $(DATA_DIR)

.PHONY: all clean check setup list