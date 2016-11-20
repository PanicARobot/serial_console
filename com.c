#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/signal.h>

#define DATABITS 8
#define STOPBITS 0
#define PARITYON 0
#define PARITY 0

#define INPUT_MAX_BUFFER 255

long parse_baud_rate(char *str) {
	long number;
	if(sscanf(str, "%ld", &number) != 1)
		return -1;
	return number;
}

volatile int INPUT_AVAILABLE;

void signal_handler(int status) {
	INPUT_AVAILABLE = 1;
}

int main(int argc, char **argv) {
	if(argc != 3) {
		puts("Incorrect number of arguments");
		return 1;
	}

	FILE *input = fopen("/dev/tty", "r");
	FILE *output = fopen("/dev/tty", "w");

	int fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(fd < 0) {
		fprintf(stderr, "Can not open %s", argv[1]);
		return 1;
	}

	long baud_rate = parse_baud_rate(argv[2]);

	struct sigaction saio;

	saio.sa_handler = signal_handler;
	sigemptyset(&saio.sa_mask);
	saio.sa_flags = 0;
	saio.sa_restorer = NULL;
	sigaction(SIGIO, &saio, NULL);

	fcntl(fd, F_SETOWN, getpid());
	fcntl(fd, F_SETFL, FASYNC);

	struct termios oldtio, newtio;

	tcgetattr(fd, &oldtio);
	newtio.c_cflag = baud_rate | CRTSCTS | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	char command_buffer[80];
	int command_length = 0;

	char input_buffer[INPUT_MAX_BUFFER];

	while(1) {
		char key;
		if(fread(&key, 1, 1, input) == 1) {
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
			for(int i = 0; i < input_length; ++i)
				fputc((int) input_buffer[i], output);

			INPUT_AVAILABLE = 0;
		}
	}
}
