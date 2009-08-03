
typedef void (*io_callback) (int fd, void* userdata);

void io_loop_add_fd (int fd, io_callback cb, void* _userdata);

void io_loop_remove_fd (int fd);

void io_loop_start ();
