
ifeq ($(DEBUG), )
    CFLAGS=-Wall -O3
else
    CFLAGS=-Wall -O0 -g -ggdb -DDEBUG=1
endif
LDFLAGS=

all: slowdown speed_test

slowdown: slowdown.c
	gcc $(CFLAGS) $(LDFLAGS) slowdown.c -o $@

speed_test: speed_test.c
	gcc $(CFLAGS) $(LDFLAGS) speed_test.c -o $@

$(BUILD_DIR):
	/bin/mkdir -v $(BUILD_DIR)

clean:
	rm -f slowdown speed_test
