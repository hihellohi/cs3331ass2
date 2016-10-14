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
#include <vector>
#include <utility>
#include <queue>

#define UPDATE_INTERVAL 1000
#define ROUTE_UPDATE_INTERVAL 1200
#define MAX_NODES 26

struct packet{
	unsigned char id;
	unsigned char seq;
	unsigned char len;
	std::pair<unsigned char, int> data[MAX_NODES];
};

void die(std::string s){
	fprintf(stderr, "%s\n", s.c_str());
	exit(1);
}

int tryrecv(int s, void *buf, int bufsize, int usdelay, sockaddr_in *si_other, int *slen){
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

void broadcast(int s, void *buffer, int len, std::vector<unsigned short> neighbours, unsigned short exclusion){
	sockaddr_in si_other;
	memset((char*)&si_other, 0, sizeof(si_other));

	si_other.sin_family = AF_INET;

	if(!inet_aton("127.0.0.1", &si_other.sin_addr)){
		die("inet_aton");
	}

	for(int i = 0; i < (int)neighbours.size(); i++){
		if(exclusion == neighbours[i]) continue;

		si_other.sin_port = neighbours[i];
		if(sendto(s, buffer, len, 0, (sockaddr*)&si_other, sizeof(si_other)) == -1){
			die("sendto");
		}
	}
}

int main(int argc, char **argv){

	if(argc != 4){
		die("usage: lsr node port config");
	}

	// Network Initialisation

	sockaddr_in si_me, si_other;

	int s, slenm = sizeof(si_me), sleno = sizeof(si_other);

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
	link_state.seq = 0;
	link_state.id = argv[1][0] - 'A';

	int matrix[MAX_NODES][MAX_NODES];
	std::vector<unsigned short> neighbours;

	for(int i = 0; i < MAX_NODES; i++){
		memset(matrix[i], -1, sizeof(matrix[i]));
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
	unsigned char seen[26];
	bool active[26];
	memset(seen, 0, sizeof(seen));
	memset(active, 0, sizeof(active));

	// Main loop
	while(1){
		if(get_timer(&ui) >= UPDATE_INTERVAL){
			// send out own link state 

			broadcast(s, &link_state, sizeof(link_state), neighbours, 0);
			link_state.seq++;
			
			set_timer(&ui);
		}

		if(get_timer(&rui) >= ROUTE_UPDATE_INTERVAL){
			// TODO dijkstra

			std::priority_queue<
				std::pair<int, unsigned char>,
				std::vector<std::pair<int, unsigned char> >,
				std::greater<std::pair<int, unsigned char> > > pq;

			unsigned char back[26];
			unsigned int dist[26];

			memset(back, -1, sizeof(back));
			memset(dist, -1, sizeof(dist));

			dist[link_state.id] = 0;
			pq.push(std::make_pair(0, link_state.id));
			
			while(!pq.empty()){
				unsigned char cur = pq.top().second;
				int curdist = pq.top().first;
				pq.pop();

				if((unsigned int)curdist > dist[cur]) continue;
				printf("%c %d\n", cur + 'A', curdist);
				
				for(unsigned int i = 0; i < 26; i++){
					if(matrix[cur][i] != -1 && (unsigned int)(matrix[cur][i] + curdist) < dist[i]){
						back[i] = cur;
						dist[i] = matrix[cur][i] + curdist;
						pq.push(std::make_pair(dist[i], i));
					}
				}
			}
			for(int i = 0; i < 26; i++){
				if(back[i] != -1){
					printf("least-cost path to node %c: ", i + 'A');

					int cur = i;
					do{
						printf("%c", cur + 'A');
						cur = back[cur];
					}while(cur != -1);

					printf(" and the cost is %d\n", dist[cur]);
				}
			}

			set_timer(&rui);
			exit(0);
		}
		
		if(tryrecv(s, &buffer, sizeof(buffer), 100000, &si_other, &sleno) >= 0){
			printf("%c received packet from %c...", link_state.id + 'A', buffer.id + 'A');

			if(!active[buffer.id] || seen[buffer.id] - buffer.seq > 128){

				for(int i = 0; i < buffer.len; i++){
					matrix[buffer.data[i].first][buffer.id] = buffer.data[i].second;
					matrix[buffer.id][buffer.data[i].first] = buffer.data[i].second;
				}

				broadcast(s, &buffer, sizeof(buffer), neighbours, si_other.sin_port);
				seen[buffer.id] = buffer.seq;
				active[buffer.id] = true;

				printf("forwarded\n");
			}
			else{
				printf("ignored\n");
			}
			fflush(stdout);
		}
	}

	close(s);
	return 0;
}
