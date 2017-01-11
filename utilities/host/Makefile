OBJS=oscctrl.o
BIN=oscctrl

CFLAGS+=-Wall -g -O3
LDFLAGS+=-lm -lrt -lz -lc -llo -lpthread

INCLUDES+=

all: $(BIN)

%.o: %.c
	@rm -f $@
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

$(BIN): $(OBJS)
	$(CC) -o $@ -Wl,--whole-archive $(OBJS) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic

clean:
	@rm -f $(OBJS)
	@rm -f $(BIN)
