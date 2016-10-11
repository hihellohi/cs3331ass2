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
#include <map>
#include <vector>

#define UPDATE_INTERVAL 1000
#define ROUTE_UPDATE_INTERVAL 1000
#define MAX_NODES 26

struct state{
	unsigned char id;

}

void die(std::string s){
	fprintf(stderr, "%s\n", s.c_str());
	exit(1);
}

int tryrecv(int s, char *buf, int bufsize, int usdelay){
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

void set_timer(timeval *tv){
	gettimeofday(tv, 0);
}

int get_timer(timeval *tv){
	timeval tmp;
	gettimeofday(&tmp, 0);
	int now = tmp.tv_sec * 1000 + tmp.tv_usec / 1000;
	int before = tv->tv_sec * 1000 + tv->tv_usec / 1000;
	int out = now - before;
	if(out < 0){
		out += 24 * 3600000;
	}
	return out;
}

int main(int argc, char **argv){

	if(argc != 4){
		die("usage: lsr node port config");
	}

	// Network Initialisation

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

	// General Initialisation

	unsigned char id_me = argv[1][0] - 'A';
	int ports[MAX_NODES], matrix[MAX_NODES][MAX_NODES];
	std::vector<unsigned char> neighbours;

	memset(ports, -1, MAX_NODES);

	for(int i = 0; i < MAX_NODES; i++){
		for(int j = 0; j < MAX_NODES; j++){
			matrix[i][j] = i == j ? 0 : -1;
		}
	}

	FILE *fin = fopen(argv[3], "r");
	int n;

	fscanf(fin, "%d\n", &n);
	for(int i = 0; i < n; i++){
		unsigned char id_other;
		int cost, port;

		fscanf(fin, "%c %d %d\n", &id_other, &cost, &port);
		id_other -= 'A';
		neighbours.push_back(id_other);
		matrix[id_me][id_other] = matrix[id_other][id_me] = cost;
		ports[id_other] = cost;
	}
	fclose(fin);

	timeval ui, rui;
	set_timer(&ui);
	set_timer(&rui);

	// Main loop
	while(1){
		if(get_timer(&ui) >= UPDATE_INTERVAL){
			// TODO send out own link state 
			set_timer(&ui);
		}

		if(get_timer(&rui) >= ROUTE_UPDATE_INTERVAL){
			// TODO dijkstra
			set_timer(&rui);
		}
		
	}

	close(s);
	return 0;
}
