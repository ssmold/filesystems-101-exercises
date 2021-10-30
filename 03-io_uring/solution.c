#include <solution.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <liburing.h>
#include <stdlib.h>

#define QUEUE_DEPTH	64
#define BLOCK_SIZE	(32*1024)

struct data {
	int read;
	off_t first_offset, offset;
	size_t first_size;
	struct iovec iov;
};

static int get_file_size(int fd, off_t *size) {
	struct stat st;

	if (fstat(fd, &st) < 0) {
		return -1;
	}

	*size = st.st_size;
	return 0;
}

static void prep_queue(struct io_uring *ring, struct io_data *data, int in, int out) {
	struct io_uring_sqe *sqe;

	sqe = io_uring_get_sqe(ring);

	if (data->read) {
		io_uring_prep_readv(sqe, in, &data->iov, 1, data->offset);
	} else {
		io_uring_prep_writev(sqe, out, &data->iov, 1, data->offset);
	}

	io_uring_sqe_set_data(sqe, data);
}

static int read_queue(struct io_uring *ring, off_t size, off_t offset, int in, int out) {
	struct io_uring_sqe *sqe;
	struct io_data *data;

	data = malloc(size + sizeof(*data));
	if (!data)
		return -errno;

	sqe = io_uring_get_sqe(ring);
	if (!sqe) {
		free(data);
		return -errno;
	}

	data->read = 1;
	data->offset = data->first_offset = offset;

	data->iov.iov_base = data + 1;
	data->iov.iov_len = size;
	data->first_size = size;

	io_uring_prep_readv(sqe, in, &data->iov, 1, offset);
	io_uring_sqe_set_data(sqe, data);
	return 0;
}

static void write_queue(struct io_uring *ring, struct io_data *data, int in, int out) {
	data->read = 0;
	data->offset = data->first_offset;

	data->iov.iov_base = data + 1;
	data->iov.iov_len = data->first_size;

	prep_queue(ring, data, in, out);
	io_uring_submit(ring);
}

int copy(int in, int out) {
	struct io_uring *ring;
	off_t in_file_size;
	int return_value;

	return_value = io_uring_queue_init(QUEUE_DEPTH, ring, 0);
	if (return_value < 0) {
		return -1;
	}

	if (get_file_size(in, &in_file_size)) {
		return 1;
	}

	unsigned long read, write;
	struct io_uring_cqe *cqe;
	off_t write_left, offset;
	int ret;

	write_left = in_file_size;
	write = read = offset = 0;

	while (in_file_size || write_left) {
		unsigned long had_reads;
		int got_comp;

		had_reads = read;
		while (in_file_size) {
			off_t this_size = in_file_size;

			if (read + write >= QUEUE_DEPTH)
				break;
			if (this_size > BLOCK_SIZE)
				this_size = BLOCK_SIZE;
			else if (!this_size)
				break;

			if (read_queue(ring, this_size, offset, in, out))
				break;

			in_file_size -= this_size;
			offset += this_size;
			read++;
		}

		if (had_reads != read) {
			return_value = io_uring_submit(ring);
			if (return_value < 0) {
				return -errno;
			}
		}

		got_comp = 0;
		while (write_left) {
			struct io_data *data;

			if (!got_comp) {
				return_value = io_uring_wait_cqe(ring, &cqe);
				got_comp = 1;
			} else {
				return_value = io_uring_peek_cqe(ring, &cqe);
				if (return_value == -EAGAIN) {
					cqe = NULL;
					return_value = 0;
				}
			}
			if (return_value < 0) {
				return -errno;
			}
			if (!cqe)
				break;

			data = io_uring_cqe_get_data(cqe);
			if (cqe->res < 0) {
				if (cqe->res == -EAGAIN) {
					prep_queue(ring, data, in, out);
					io_uring_submit(ring);
					io_uring_cqe_seen(ring, cqe);
					continue;
				}
				return -errno;
			} else if ((size_t) cqe->res != data->iov.iov_len) {

				data->iov.iov_base += cqe->res;
				data->iov.iov_len -= cqe->res;
				data->offset += cqe->res;
				prep_queue(ring, data, in, out);
				io_uring_submit(ring);
				io_uring_cqe_seen(ring, cqe);
				continue;
			}

			if (data->read) {
				write_queue(ring, data, in, out);
				write_left -= data->first_size;
				read--;
				write++;
			} else {
				free(data);
				write--;
			}
			io_uring_cqe_seen(ring, cqe);
		}
	}

	while (write) {
		struct io_data *data;

		return_value = io_uring_wait_cqe(ring, &cqe);
		if (return_value) {
			return -errno;
		}

		if (cqe->res < 0) {
			return -errno;
		}
		data = io_uring_cqe_get_data(cqe);
		free(data);
		write--;
		io_uring_cqe_seen(ring, cqe);
	}

	io_uring_queue_exit(&ring);
	return 0;
}
