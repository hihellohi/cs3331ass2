#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <string>

void die(std::string s){
	fprintf(stderr, "%s\n", s.c_str());
	exit(1);
}

int tryrecv(int s, char *buf, int bufsize, sockaddr_in *si_target, int usdelay){
	//-2 timeout -1 error else returns length of output

	timeval timeout;
	memset((char*)&timeout, 0, sizeof(timeout));
	timeout.tv_usec = usdelay;
	
	fd_set readfds;

	FD_ZERO(&readfds);
	FD_SET(s, &readfds);

	int n = select(s+1, &readfds, NULL, NULL, &timeout);

	if(n == 0){
		return -2;
	}else if(n == -1){
		die("select");
	}

	memset(buf, 0, bufsize);
	n = recvfrom(s, buf, bufsize, 0, NULL, NULL);

	return n;
}

int main(int argc, char **argv){

	if(argc != 4){
		die("usage: lsr node port config");
	}

	// Initialisation

	sockaddr_in si_me;

	int s, slen = sizeof(si_me);

	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
		die("socket");
	}

	memset((char*)&si_me, 0, slen);

	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(atoi(argv[2]));

	if(!inet_aton("127.0.0.1", &si_me.sin_addr)){
		die("inet_aton");
	}
	
	if(bind(s, (sockaddr*)&si_me, sizeof(si_me)) == -1){
		die("bind");
	}

	close(s);
	return 0;
}
