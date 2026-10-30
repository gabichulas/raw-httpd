CC = gcc
CFLAGS = -Wall -Wextra -Werror -fsanitize=address,undefined -g

# Definimos las rutas
SRC_DIR = src
BUILD_DIR = build
TARGET = $(BUILD_DIR)/server
SRC = $(SRC_DIR)/server.c

all: $(TARGET)

# La regla ahora depende de src/server.c y crea el ejecutable en build/
$(TARGET): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
