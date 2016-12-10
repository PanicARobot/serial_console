#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/signal.h>

#define INPUT_MAX_BUFFER 8192

int fd;
char input_buffer[INPUT_MAX_BUFFER + 1];

struct termios oldtio, newtio;

volatile int INPUT_AVAILABLE;

void signal_handler(int status) {
	INPUT_AVAILABLE = 1;
}

void cleanup(int);

int main(int argc, char **argv) {
	if(argc != 2) {
		puts("Incorrect number of arguments");
		return 1;
	}

	fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(fd < 0) {
		fprintf(stderr, "Can not open %s", argv[1]);
		return 1;
	}

	struct sigaction saio;

	saio.sa_handler = signal_handler;
	sigemptyset(&saio.sa_mask);
	saio.sa_flags = 0;
	saio.sa_restorer = NULL;
	sigaction(SIGIO, &saio, NULL);

	fcntl(fd, F_SETOWN, getpid());
	fcntl(fd, F_SETFL, FASYNC);

	char command_buffer[80];
	int command_length = 0;

	signal(SIGTERM, cleanup);
	signal(SIGABRT, cleanup);

	while(1) {
		char key;
		if(fread(&key, 1, 1, stdin) == 1) {
			if(key == 0x1B) break;
			if(key == 0x0A) {
				if(command_length > 0) {
					int k = write(fd, command_buffer, command_length);
					command_length = 0;
				}
			}
			else {
				command_buffer[command_length++] = key;
			}
		}

		if(INPUT_AVAILABLE == 1) {
			int input_length = read(fd, input_buffer, INPUT_MAX_BUFFER);
			input_buffer[input_length] = '\0';
			printf("%s", input_buffer);

			INPUT_AVAILABLE = 0;
		}
	}

	cleanup(0);
}

void cleanup(int s) {
	close(fd);

	fflush(stdout);
}
