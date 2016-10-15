#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <cassert>

#include <string>
#include <vector>
#include <map>
#include <utility>
#include <queue>
#include <stack>

#define UPDATE_INTERVAL_RAW 1000
#define ROUTE_UPDATE_INTERVAL_RAW 30000
#define MULTIPLIER 10
#define UPDATE_INTERVAL UPDATE_INTERVAL_RAW / MULTIPLIER
#define ROUTE_UPDATE_INTERVAL ROUTE_UPDATE_INTERVAL_RAW / MULTIPLIER 

#define MAX_NODES 26
#define INFINITY 1.0/0.0

struct Header{
	unsigned char id;
	unsigned char seq;
	int len;
};

struct Packet{
	Header header;
	std::pair<unsigned char, double> data[MAX_NODES];
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

void broadcast(int s, Packet *buffer, std::map<unsigned short, unsigned char> neighbours, unsigned short exclusion){
	sockaddr_in si_other;
	memset((char*)&si_other, 0, sizeof(si_other));

	si_other.sin_family = AF_INET;

	if(!inet_aton("127.0.0.1", &si_other.sin_addr)){
		die("inet_aton");
	}

	int len = sizeof(Header) + (sizeof(std::pair<unsigned char, double>) * buffer->header.len);
	assert(sizeof(Header) + (sizeof(std::pair<unsigned char, double>) * MAX_NODES) == sizeof(Packet));

	for(std::map<unsigned short, unsigned char>::iterator i = neighbours.begin(); i != neighbours.end(); i++){
		if(exclusion == i->first) continue;

		si_other.sin_port = i->first;
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
	
	Packet link_state;
	link_state.header.seq = 0;
	link_state.header.id = argv[1][0] - 'A';

	double matrix[MAX_NODES][MAX_NODES];
	std::map<unsigned short, unsigned char> neighbours;
	std::vector<timeval> timeouts;
	std::vector<unsigned short> keys;

	for(int i = 0; i < MAX_NODES; i++)
	for(int j = 0; j < MAX_NODES; j++)
		matrix[i][j] = INFINITY;

	FILE *fin = fopen(argv[3], "r");

	fscanf(fin, "%d\n", &link_state.header.len);
	for(int i = 0; i < link_state.header.len; i++){
		unsigned char id_other;
		double cost;
		unsigned short port;

		fscanf(fin, "%c %lf %hu\n", &id_other, &cost, &port);
		id_other -= 'A';

		//neighbours.push_back(htons(port));
		port = htons(port);
		neighbours[port] = i;
		keys.push_back(port);
		matrix[link_state.header.id][id_other] = matrix[id_other][link_state.header.id] = cost;
		
		timeval tmp;
		set_timer(&tmp);
		timeouts.push_back(tmp);

		link_state.data[i].first = id_other;
		link_state.data[i].second = cost;
	}
	fclose(fin);

	timeval ui, rui;
	set_timer(&ui);
	set_timer(&rui);

	Packet buffer;
	unsigned char seen[MAX_NODES];
	bool active[MAX_NODES];
	memset(seen, 0, sizeof(seen));
	memset(active, 0, sizeof(active));

	// Main loop
	while(1){
		if(get_timer(&ui) >= UPDATE_INTERVAL){
			// send out own link state 

			broadcast(s, &link_state, neighbours, 0);
			link_state.header.seq++;
			
			set_timer(&ui);
		}

		if(get_timer(&rui) >= ROUTE_UPDATE_INTERVAL){
			// TODO dijkstra

			std::priority_queue<
				std::pair<double, unsigned char>,
				std::vector<std::pair<double, unsigned char> >,
				std::greater<std::pair<double, unsigned char> > > pq;

			unsigned char back[MAX_NODES];
			double dist[MAX_NODES];

			memset(back, -1, sizeof(back));
			for(int i = 0; i < MAX_NODES; i++){
				dist[i] = INFINITY;
			}

			dist[link_state.header.id] = 0.0;
			pq.push(std::make_pair(0.0, link_state.header.id));
			
			while(!pq.empty()){
				unsigned char cur = pq.top().second;
				double curdist = pq.top().first;
				pq.pop();

				if(curdist > dist[cur]) continue;
				
				for(unsigned int i = 0; i < MAX_NODES; i++){
					if((matrix[cur][i] + curdist) < dist[i]){
						back[i] = cur;
						dist[i] = matrix[cur][i] + curdist;
						pq.push(std::make_pair(dist[i], i));
					}
				}
			}
			for(int i = 0; i < MAX_NODES; i++){
				if(back[i] != 255){
					printf("least-cost path to node %c: ", i + 'A');

					int cur = i;
					std::stack<unsigned char> revr;
					do{
						revr.push(cur);
						cur = back[cur];
					}while(cur != 255);

					while(!revr.empty()){
						printf("%c", revr.top() + 'A');
						revr.pop();
					}

					printf(" and the cost is %f\n", dist[i]);
				}
			}

			set_timer(&rui);
			//exit(0);
		}
		
		if(tryrecv(s, &buffer, sizeof(buffer), 10000, &si_other, &sleno) >= 0){
			//printf("%c received packet from %c seq #%d...", link_state.header.id+'A', buffer.header.id+'A', buffer.header.seq);

			if(!active[buffer.header.id] || (unsigned char)(seen[buffer.header.id] - buffer.header.seq) > 128){

				for(int i = 0; i < buffer.header.len; i++){
					matrix[buffer.data[i].first][buffer.header.id] = buffer.data[i].second;
					matrix[buffer.header.id][buffer.data[i].first] = buffer.data[i].second;
				}

				broadcast(s, &buffer, neighbours, si_other.sin_port);
				seen[buffer.header.id] = buffer.header.seq;
				active[buffer.header.id] = true;

				set_timer(&timeouts[neighbours[si_other.sin_port]]);
			}
			fflush(stdout);
		}

		for(int i = 0; i < link_state.header.len; i++){
			unsigned char curid = link_state.data[i].first;
			if(get_timer(&timeouts[i]) > UPDATE_INTERVAL * 5 && active[curid]){	

				printf("node %c died!\n", curid + 'A');
				link_state.data[i].second = INFINITY;
				active[curid] = false;

				for(int j = 0; j < MAX_NODES; j++){
					matrix[curid][j] = matrix[j][curid] = INFINITY;
				}

				neighbours.erase(keys[i]);

//				for(std::map<unsigned short, unsigned char>::iterator it = neighbours.begin(); it != neighbours.end(); it++){
//					if(it->second == curid){
//						neighbours.erase(it);
//						break;
//					}
//				}
			}
		}
	}

	close(s);
	return 0;
}
