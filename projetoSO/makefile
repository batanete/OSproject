#compilador
CC=gcc

#flags
CFLAGS= -pthread -Wall

#nome do executavel pretendido
TARGET = projeto

all: $(TARGET)

$(TARGET): $(TARGET).c

	$(CC) $(CFLAGS)  -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET)
