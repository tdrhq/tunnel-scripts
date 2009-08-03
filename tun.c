
#include <features.h>
#ifndef __USE_MISC
#define __USE_MISC
#error woah
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <unistd.h>

int tun_alloc(char *dev)
{
	struct ifreq ifr;
	int fd, err;
	
	if( (fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
		fprintf(stderr, "Oh damn shit\n");
		exit (1);
	}
	
	memset(&ifr, 0, sizeof(ifr));
	
	/* Flags: IFF_TUN   - TUN device (no Ethernet headers) 
	 *        IFF_TAP   - TAP device  
	 *
	 *        IFF_NO_PI - Do not provide packet information  
	 */ 
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	if( *dev )
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
		close(fd);
		fprintf (stderr, "error!\n");
		return err;
	}
	strcpy(dev, ifr.ifr_name);
	return fd;
}              

