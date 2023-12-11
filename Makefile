MAKEFLAGS+=-r -j

BUILD=build

SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:%=$(BUILD)/%.o)

BINARY=$(BUILD)/run

CFLAGS=-g -Wall -Wextra -O3 -ffast-math -Iextern
LDFLAGS=-lm

all: $(BINARY)

format:
	clang-format -i src/*.c src/*.h

$(BINARY): $(OBJECTS)
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $< $(CFLAGS) -c -MMD -MP -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD)

.PHONY: all clean format