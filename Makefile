# Compiler and flags
CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lavformat -lavcodec -lavutil -lm

# Target for the media info reader
TARGET_INFO = media_info_reader
SRC_INFO = media_info_reader.c

# Target for the simple decoder
TARGET_DECODER = simple_decoder
SRC_DECODER = simple_decoder.c

# Target for the simple encoder
TARGET_ENCODER = simple_encoder
SRC_ENCODER = simple_encoder.c

# Default target
all: $(TARGET_ENCODER)

# Rule to build the encoder
$(TARGET_ENCODER): $(SRC_ENCODER)
	$(CC) $(CFLAGS) -o $(TARGET_ENCODER) $(SRC_ENCODER) $(LDFLAGS)

# Rule to build the decoder
decoder: $(TARGET_DECODER)

$(TARGET_DECODER): $(SRC_DECODER)
	$(CC) $(CFLAGS) -o $(TARGET_DECODER) $(SRC_DECODER) $(LDFLAGS)

# Rule to build the info reader
info: $(TARGET_INFO)

$(TARGET_INFO): $(SRC_INFO)
	$(CC) $(CFLAGS) -o $(TARGET_INFO) $(SRC_INFO) $(LDFLAGS)

# Clean up build files
clean:
	rm -f $(TARGET_INFO) $(TARGET_DECODER) $(TARGET_ENCODER) *.o *.pgm test.mp4

.PHONY: all clean info decoder