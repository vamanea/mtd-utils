LIBUBI_PATH=../../ubi-utils/new-utils/
LIBUBI_SRC_PATH=../../ubi-utils/new-utils/src/
LIBUBI_HEADER_PATH=../../ubi-utils/new-utils/include
UBI_HEADERS_PATH=../../include/
UBIUTILS_PATH=../../ubi-utils/new-utils/

CC := $(CROSS)gcc

TESTS=io_update volrefcnt integ io_paral io_read io_basic \
          mkvol_basic mkvol_bad mkvol_paral rsvol

HELPER_NAMES=ubiupdatevol
HELPERS=$(addprefix helpers/, $(HELPER_NAMES))

# Because of implicite rules we use make treats .o files as intermediate, thus
# it removes the. If you want to prevent the removal, uncomment the below
#.SECONDARY: $(addsuffix .o, $(TESTS)) $(addsuffix .o, $(HELPERS))

CFLAGS += -Wall -I$(LIBUBI_HEADER_PATH) -I $(UBI_HEADERS_PATH) -L. -O2

all: ubi-utils libubi $(TESTS) $(HELPERS)

# Compile ubilib with the udevsettle hack
libubi: $(LIBUBI_SRC_PATH)/libubi.c  $(LIBUBI_HEADER_PATH)/libubi.h  $(LIBUBI_SRC_PATH)/libubi_int.h
	$(CC) $(CFLAGS) -I $(LIBUBI_SRC_PATH) -I../../include -DUDEV_SETTLE_HACK -c $(LIBUBI_SRC_PATH)/libubi.c -o libubi.o
	ar cr libubi.a libubi.o

# The below cancels existing implicite rule to make programs from .c files,
# in order to force make using our rule defined below
%: %.c

# The below is the rule to get an .o file from a .c file
%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

# And the below is the rule to get final test executable from its .o and common.o
%: %.o common.o
	$(CC) $(CFLAGS) $^ -lubi -o $@

# *paral tests require libpthread, thus the below rule for them
%paral: %paral.o common.o
	$(CC) $(CFLAGS) $^ -lubi -lpthread -o $@

ubi-utils:
	make -C $(UBIUTILS_PATH)

clean:
	rm -f $(TESTS) $(addsuffix .o, $(TESTS)) libubi.* $(HELPERS) $(addsuffix .o, $(HELPERS))
