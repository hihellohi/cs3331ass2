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
#include <utility>

#define UPDATE_INTERVAL 1000
#define ROUTE_UPDATE_INTERVAL 3000
#define MAX_NODES 26

struct packet{
	unsigned char id;
	unsigned char parity;
	unsigned char len;
	std::pair<unsigned char, int> data[MAX_NODES];
};

void die(std::string s){
	fprintf(stderr, "%s\n", s.c_str());
	exit(1);
}

int tryrecv(int s, char *buf, int bufsize, int usdelay, sockaddr_in *si_other, int *slen){
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
	n = recvfrom(s, buf, bufsize, 0, (sockaddr*)si_other, slen);

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

	sockaddr_in si_me, si_other;

	int s, slenm = sizeof(si_me), sleno;

	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
		die("socket");
	}

	memset((char*)&si_me, 0, slenm);

	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(atoi(argv[2]));
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(s, (sockaddr*)&si_me, sizeof(si_me)) == -1){
		die("bind");
	}

	// General Initialisation
	
	packet link_state;
	link_state.parity = 0;
	link_state.id = argv[1][0] - 'A';

	int matrix[MAX_NODES][MAX_NODES];
	std::vector<unsigned short> neighbours;

	for(int i = 0; i < MAX_NODES; i++){
		for(int j = 0; j < MAX_NODES; j++){
			matrix[i][j] = i == j ? 0 : -1;
		}
	}

	FILE *fin = fopen(argv[3], "r");

	fscanf(fin, "%hhu\n", &link_state.len);
	for(int i = 0; i < link_state.len; i++){
		unsigned char id_other;
		int cost, port;

		fscanf(fin, "%c %d %d\n", &id_other, &cost, &port);
		id_other -= 'A';

		neighbours.push_back(htons(port));
		matrix[link_state.id][id_other] = matrix[id_other][link_state.id] = cost;

		link_state.data[i].first = id_other;
		link_state.data[i].second = cost;
	}
	fclose(fin);

	timeval ui, rui;
	set_timer(&ui);
	set_timer(&rui);

	packet buffer;

	// Main loop
	while(1){
		if(get_timer(&ui) >= UPDATE_INTERVAL){
			// send out own link state 
			link_state.parity = !link_state.parity;

			sleno = sizeof(si_other);
			memset((char*)&si_other, 0, sizeof(si_other));

			si_other.sin_family = AF_INET;

			if(!inet_aton("127.0.0.1", &si_other.sin_addr)){
				die("inet_aton");
			}

			for(int i = 0; i < (int)neighbours.size(); i++){

				printf("%c sending to port %d\n", link_state.id + 'A', neighbours[i]);
				fflush(stdout);
				
				si_other.sin_port = neighbours[i];
				if(sendto(s, (void*)&link_state, sizeof(link_state), 0, (sockaddr*)&si_other, sleno) == -1){
					die("sendto");
				}
			}

			set_timer(&ui);
		}

		if(get_timer(&rui) >= ROUTE_UPDATE_INTERVAL){
			// TODO dijkstra
			for(int i = 0; i < MAX_NODES; i++){
				for(int j = 0; j < MAX_NODES; j++)
					printf("%2d ", matrix[i][j]);
				printf("\n");
			}

			set_timer(&rui);
			exit(0);
		}
		
		if(tryrecv(s, (char*)&buffer, sizeof(packet), 100000, &si_other, &sleno) >= 0){
			printf("%c received packet from %c\n", link_state.id + 'A', buffer.id + 'A');
			fflush(stdout);
			for(int i = 0; i < buffer.len; i++){
				matrix[buffer.data[i].first][buffer.id] = buffer.data[i].second;
				matrix[buffer.id][buffer.data[i].first] = buffer.data[i].second;
			}
		}
	}

	close(s);
	return 0;
}
