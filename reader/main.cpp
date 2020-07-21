#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ioctl.h>
#include<asm/ioctl.h>
#include<asm/termbits.h>

/* Turns out that reading binary data from a serial port with no mangling is very hard. */

int main(int argc, char *argv[]) {
	if(argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: %s <serial port> [baud rate]\n", argv[0]);
		exit(1);
	}

	int baud = 1000000;
	if(argc >= 3) {
		errno = 0;
		baud = strtol(argv[2], nullptr, 0);
		if(errno != 0) {
			fprintf(stderr, "baud %s: %s\n", argv[2], strerror(errno));
			exit(1);
		} else if(baud == 0) {
			fprintf(stderr, "invalid baud: %s\n", argv[2]);
			exit(1);
		}
	}
	
	int fd = open(argv[1], O_RDWR | O_NOCTTY);
	if(fd < 0) {
		perror(argv[1]);
		exit(-1);
	}

	struct termios2 tio;
	memset(&tio, 0, sizeof(tio));
	
	tio.c_iflag = INPCK; // NO ICANON!!!!!1!!
	tio.c_cflag = CS8 | CREAD | HUPCL | CRTSCTS | BOTHER;
	tio.c_ispeed = baud;
	tio.c_ospeed = baud;
	tio.c_cc[VTIME] = 1;
	tio.c_cc[VMIN] = 0;
	
	if(ioctl(fd, TCSETS2, &tio) < 0) {
		fprintf(stderr, "failed to set terminal settings: %s\n", strerror(errno));
		close(fd);
		exit(-1);
	}

	char buffer[256];
	ssize_t r;

	// swallow
	while((r = read(fd, (void*) buffer, sizeof(buffer))) > 0) {}
	if(r < 0) {
		perror("swallow");
		close(fd);
		exit(-1);
	}

	tio.c_cc[VTIME] = 0;
	tio.c_cc[VMIN] = 1;
	if(ioctl(fd, TCSETS2, &tio) < 0) {
		fprintf(stderr, "failed to set terminal settings: %s\n", strerror(errno));
		close(fd);
		exit(-1);
	}
	
	while(1) {
		ssize_t r = read(fd, (void*) buffer, sizeof(buffer));
		if(r <= 0) {
			perror("read");
			close(fd);
			exit(-1);
		}
		ssize_t w = write(STDOUT_FILENO, (void*) buffer, r);
		if(w < r) {
			perror("write");
			close(fd);
			exit(-1);
		}
	}
}
