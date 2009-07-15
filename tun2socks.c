#define _POSIX_SOURCE /* for kill */

#include <assert.h>
#include <errno.h>
#include <mqueue.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <mqueue.h>
#include <signal.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <mqueue.h>

mqd_t mq = 1; 
char mq_name [128];


int tun_alloc (char*);

/**
 * cleanup code. 
 */
void controller_cleanup () 
{
	int status; 

	fprintf (stderr, "Pid of cleanup process %d\n", getpid());

	/* kill each child */
	kill (0, SIGKILL);

	mq_close (mq); 
	mq_unlink (mq_name);
	exit(0); 
}

void message_queue (int fd)
{
	/* send packets to the interface */

	char  message [100000] = "<>";
	struct mq_attr  qattr; 
	unsigned int priority; 
	mq_getattr (mq, &qattr); 
	
	for (;;){
		int ret = mq_receive (mq, message, qattr.mq_msgsize, &priority); 
		if ( ret == -1 ) { 
			perror ("Could not receive message");
			return;
		}
		write (fd, message, ret);
	}
}

int main() 
{
	int data;
	char stun[100] = "tun0";
	int tun = tun_alloc (stun);

	sprintf (mq_name, "/mqq_%d", getpid());
	mq = mq_open (mq_name,  O_RDONLY|O_CREAT|O_EXCL, S_IRWXU, NULL);
	signal (SIGINT, controller_cleanup); 

	if (fork () == 0) {
		signal (SIGINT, NULL);
		message_queue (tun);
	}

	/* let's read these */
	fprintf(stderr, "listening on %s\n", stun);

	while (1) {
		struct iphdr iphdr;
		char ar[100000];
		char *data;
		int i;
		
		ssize_t s;

		s = read (tun, &iphdr, sizeof(iphdr));
		if (s == -1) 
			perror ("read");
		
		s = read (tun, ar, iphdr.tot_len - sizeof(iphdr));
		data = ar + iphdr.ihl - sizeof(iphdr);

		/* good, we now got the data */

		/* first test, if this is not TCP, discard */
		if (iphdr.protocol != 6) continue;


		for(i = 0; i < s; i++) {
			printf("%c", ar[i]);
			fflush (stdout);
		}
		fprintf(stderr, "here %d\n", (int)s);
	}
	fgetc(stdin);
}
