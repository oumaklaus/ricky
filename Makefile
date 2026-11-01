CC      = cc
CFLAGS  = -std=gnu11 -O2 -Wall -Wextra -g
SRC     = src/main.c src/lexer.c src/parser.c src/codegen.c
OBJ     = $(SRC:.c=.o)
BIN     = ricky

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c src/ricky.h
	$(CC) $(CFLAGS) -c $< -o $@

examples: $(BIN)
	@for f in examples/*.ricky; do \
		echo "=== $$f ==="; \
		./$(BIN) "$$f" -o /tmp/ricky_demo && /tmp/ricky_demo; \
	done

test: $(BIN)
	./tests/run-tests.sh

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all examples test clean
