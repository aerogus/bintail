#include <string>
#include <iostream>
#include <fstream>

#include <cstdlib>

#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno> // C++ version of errno.h

using namespace std;

// buffer size
const unsigned int BUFSZ = 1 * 1024;

/*
 * usage() - display help on how to run the program
 */
void usage (void) {
	cerr << "Usage: bintail <filename>" << endl;
	exit(1);
}

/*
 * ioerror() - invoked upon fatal I/O error while running I/O functions
 */
void ioerror (const std::string &s, const std::string &m) {
	if (s.empty())
		cerr << "bintail: I/O error: " << m << endl;
	else
		cerr << "bintail: I/O error while tailing '" << s << "': " << m << endl;
	exit(1);
}

/*
 * badfile() - invoked upon fatal error opening file
 */
void badfile (const std::string &s, const std::string &m) {
	cerr << "bintail: could not open file '" << s << "': " << m << endl;
	exit(1);
}

/*
 * fsize(fd) - determine size of file associated with fd
 */
off_t fsize (int fd) {
	// fstat64 will store its info here
	struct stat sb;

	// measure current file size
	if (fstat(fd, &sb) != 0)
		ioerror("", strerror(errno));

	// return st_size field
	return sb.st_size;
}

/*
 * ltell(fd) - return current I/O position within file 'fd'
 */
off_t ltell (int fd) {
	// result value
	off_t pos = 0;

	// measure current file size
	pos = lseek(fd, 0, SEEK_CUR);
	if (errno != 0)
		ioerror("", strerror(errno));

	// return st_size field
	return pos;
}

/*
 * absseek() - seek to requested byte offset within the file
 */
off_t absseek (int fd, off_t howfar) {
	// determine where the edge of the file is
	off_t edge  = fsize(fd);

	// position to which to seek
	off_t where = howfar;

	// check for overflow (and clip if request would overrun edge)
	if (where < 0)
		where = 0;
	if (where > edge)
		where = edge;

	// DEBUG:
	// cerr << "bintail: absseek(" << where << "): seeking to offset " << where << endl;

	// seek
	return lseek(fd, where, SEEK_SET);
}

/*
 * relseek() - seek to position relative to current position; 'clip' the
 *             request to file limits to prevent overrun
 */
off_t relseek (int fd, off_t howfar) {
	// determine current file location
	off_t now   = ltell(fd);

	// determine where the edge of the file is
	off_t edge  = fsize(fd);

	// compute new location
	off_t where = now + howfar;
	
	// check for overflow (and clip if request would overrun edge)
	if (where < 0)
		where = 0;
	if (where > edge)
		where = edge;

	// DEBUG:
	// cerr << "bintail: relseek(" << now << " -> " << howfar << "): seeking to offset " << where << endl;

	// seek
	return lseek(fd, where, SEEK_SET);
}

/*
 * reopen(...) - reopen a file descriptor, preserving location
 */
int reopen (int fd, const std::string &fn, mode_t md) {
	off_t pos = ltell(fd);
	close(fd);
	fd = open(fn.c_str(), md);
	if (fd < 0)
		badfile(fn, strerror(errno));
	lseek(fd, pos, SEEK_SET);
	return fd;
} 

/*
 * main() - entry point
 */
int main (int argc, char **argv) {
	// make sure enough args were given
	if ((argc < 2) || (argc > 3))
		usage();
	
	// open the input file
	int input = open(argv[1], O_RDONLY);
	if (input < 0)
		badfile(argv[1], strerror(errno));

	// flush C++ streams
	cerr.flush();
	cout.flush();

	// setup vars
	fd_set rdset;
	int n = 1, written = 0;
	unsigned char buffer[BUFSZ];
	off_t lastsize = 0; // last reported size of the file
	struct stat sb;

	// read starting offset, if given
	off_t pos = 0; // starting offset (careful!)
	if (argc == 3)
		pos = atol(argv[2]);
	else
		pos = 0;

	// seek to starting offset
	pos = lseek(input, pos, SEEK_SET);

	// initialize 'sb'
	sb.st_size = 0;

	// do initial 'stat' to determine file size
	if (fstat(input, &sb) != 0) {
		cerr << "bintail: fstat() failed (" << strerror(errno) << "), tailing may not work." << endl;
		usleep(500000);
	}

	// forever
	while (true) {
		while (sb.st_size == lastsize) {
			// call fstat to measure the file's size
			if (fstat(input, &sb) != 0)
				cerr << "bintail: fstat() failed (" << strerror(errno) << "), tailing may not work." << endl;

			// sleep for half a second
			usleep(500000);
		}

		// save the new file size
		lastsize = sb.st_size;

		// save current position, reopen the file
		input = reopen(input, argv[1], O_RDONLY);

		// while data is available, copy from file to stdout
		while (true) {
			// setup file descriptor sets
			FD_ZERO(&rdset);
			FD_SET(input, &rdset); // the file
			FD_SET(0,     &rdset); // stdin
			n = input + 1;

			// call select(), to wait for data
			n = select(n, &rdset, 0, 0, 0);

			// do we have an input event?
			if (n > 0) {
				// if input data from file...
				if (FD_ISSET(input, &rdset)) {
					n = read(input, buffer, sizeof(buffer));

					// if bytes were read
					if (n > 0) {
						// loop until all bytes written
						while (n > 0) {
							// attempt to write ALL read bytes to stdout
							written = write(1, buffer, n);

							if (written > 0) {
								n -= written;
							} else if (written < 0) {
								ioerror(argv[1], strerror(errno));
							}
						}
					} else if (n < 0) {
						ioerror(argv[1], strerror(errno));
					} else if (n == 0) {
						break;
					}
				}
			}
		}
	}
}
