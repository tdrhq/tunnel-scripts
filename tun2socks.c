#define _POSIX_SOURCE /* for kill */
#define __USE_MISC

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
#include <netinet/in.h>
#include "tcpip.h"
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

void send_syn_ack (FILE *ftun, struct iphdr iphdr, struct tcphdr tcphdr) 
{
	int temp = iphdr.daddr;
	iphdr.daddr = iphdr.saddr;
	iphdr.saddr = temp;


	fwrite (&iphdr, sizeof(iphdr), 1, ftun);

	/* options? */
	char options[100];
	memset (options, 0, sizeof(options));
	fwrite (options, iphdr.ihl - sizeof(iphdr), 1, ftun);

	/* build tcphdr */
	temp = tcphdr.source;
	tcphdr.source = tcphdr.dest;
	tcphdr.dest = temp;

	tcphdr.ack_seq = tcphdr.seq + 1;
	tcphdr.ack = 1;
	

}

void read_tun (FILE *ftun)
{
	struct iphdr iphdr;
	struct tcphdr tcphdr;
	
	char ar[100000];
	char *data;
	int i;
	
	ssize_t s;
	
	s = fread (&iphdr, sizeof(iphdr), 1, ftun);
	if (s == -1) 
		perror ("read");
	
	fprintf (stderr, "Packet of size %d going through to %d! (%d, %d)\n", (int) ntohs(iphdr.tot_len),
		 (int) iphdr.daddr, (int) sizeof(iphdr), iphdr.ihl);
	
	s = fread (ar, ntohs(iphdr.tot_len) - sizeof(iphdr), 1, ftun);
	data = ar + (iphdr.ihl)*4 - sizeof(iphdr);
	
	/* good, we now got the data */
	
	/* first test, if this is not TCP, discard */
	if (iphdr.protocol != 6 && iphdr.protocol != 17) {
		fprintf (stderr, "Packet discarded\n");
		return;
	}
	
	memcpy (&tcphdr, data, sizeof(tcphdr));
	
	fprintf (stderr, "with dest port %d!\n",(int) ntohs(tcphdr.dest));
	
	int datalen = ntohs(iphdr.tot_len) - (iphdr.ihl)*4;
	fprintf(stderr, "tcp len: %d\n", (int) datalen);
	for(i = 0; i < datalen; i++) {
		printf(" %c", data[i]);
		fflush (stdout);
	}
	fprintf(stderr, "\nhere %d\n", (int)s);

	/* do some quick hacks */
}

int main() 
{
	int data;
	char stun[100] = "tun0";
	int tun = tun_alloc (stun);
	FILE *ftun = fdopen (tun, "r");

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
		read_tun (ftun);
	}
	fgetc(stdin);
}
