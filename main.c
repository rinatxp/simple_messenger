#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <ctype.h>
#include <pthread.h>
#include <termios.h>


#define BUF 512
#define PORT 10000
#define IP_HDR_LEN sizeof(struct iphdr)
#define UDP_HDR_LEN sizeof(struct udphdr)
#define MES_REQUEST "GO_CHAT?"
#define MES_CONFIRM "GO"


void choose_action();
void init_sock(struct sockaddr_in* sock, char* ip);
void connecting();
void waiting();
void getip(char* ip);
void writeip(char* ip);
int isvalidip(char* ip);
int init_packet(struct sockaddr_in* sock, char* mes);
void send_pack(char* mes);
char* get_pack();
void chat();
void get_messages(int* cnt);
int getch();



int sock_recv, sock_send;
struct sockaddr_in server, to;
char buf[BUF], mes[BUF];


int main(int argc, char* argv[])
{
	char my_ip[16];
	getip(my_ip);

	init_sock(&server, my_ip);


	if ((sock_recv = socket(PF_INET, SOCK_RAW, IPPROTO_UDP)) < 0)
	{
		perror("socket_recv ");
		exit(-1);
	}
	if (bind(sock_recv, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind ");
		exit(-1);
	}


	int one = 1;
	if ((sock_send = socket(PF_INET, SOCK_RAW, IPPROTO_UDP)) < 0)
	{
		perror("sock_send ");
		exit(-1);
	}
	if (setsockopt(sock_send, IPPROTO_IP, IP_HDRINCL, &one, sizeof(int)) < 0)
	{
		perror("setsockopt ");
		exit(-1);
	}


	choose_action();

	return 0;
}


void choose_action()
{
	printf("\n1 - connect\n2- wait\n");

	while (1)
	{
		char c;
		(void)scanf("%c%*c", &c);

		if (c == '1')
		{
			printf("Connecting..\n");
			connecting();
		}
		else if (c == '2')
		{
			printf("Waiting..\n");
			waiting();
		}
		else
		{
			printf("Invalid command\n");
		}
	}
}

void connecting()
{
	char ip[16];
	writeip(ip);

	init_sock(&to, ip);

	send_pack(MES_REQUEST);
	if (!strcmp(MES_CONFIRM, get_pack()))
	{
		printf("\nSuccessful connected\n");
		chat();
	}
}

void waiting()
{
	char* payload = get_pack();

	struct iphdr* iph = (struct iphdr*)buf;
	struct udphdr* udph = (struct udphdr*)(buf + IP_HDR_LEN);

	if (ntohs(udph->dest) == PORT && ntohs(udph->source) == PORT)
	{
		struct in_addr ad;
		ad.s_addr = iph->saddr;

		printf("%s tries connecting..\n", inet_ntoa(ad));

		if (!strcmp(payload, MES_REQUEST))
		{
			init_sock(&to, inet_ntoa(ad));
			send_pack(MES_CONFIRM);
			printf("\nSuccessful connected\n\n");

			chat();
		}
	}
}

char* get_pack()
{
	int fromlen = sizeof(struct sockaddr_in);

	int bytes = recvfrom(sock_recv, (void*)buf, sizeof(buf), 0,
		(struct sockaddr*)&to, (socklen_t*)&fromlen);

	if (bytes < 0)
	{
		perror("recvfrom ");
		exit(-1);
	}

	buf[bytes] = '\0';
	return &buf[IP_HDR_LEN + UDP_HDR_LEN];
}

void chat()
{
	int cnt = 0;
	pthread_t thread;

	if (pthread_create(&thread, NULL, get_messages, &cnt) != 0)
	{
		perror("pthread_create ");
		exit(-1);
	}


	char to_begin[] = "\33[2K\033[AYou: ";
	char full[600];

	while (1)
	{
		char kl = getch();

		if (kl == '\n' && cnt != 0)
		{
			mes[cnt++] = '\n';
			mes[cnt++] = '\0';
			send_pack(&mes[0]);

			memset(full, '\0', 600);
			strcat(full, to_begin);
			strcat(full, mes);
			printf("%s", full);

			cnt = 0;
			memset(mes, '\0', BUF);
		}
		else if (kl != '\n')
		{
			mes[cnt++] = kl;
		}
	}
}

void get_messages(int* cnt)
{
	while (1)
	{
		char* new_mes = get_pack();

		if (new_mes[0] == '\n')
		{
			continue;
		}

		if (*cnt != 0)
		{
			strcat(new_mes, mes);
			printf("\33[2K\b\b\b\b\b\bFriend: %s", new_mes);
			fflush(stdout);
		}
		else
		{
			printf("Friend: %s", new_mes);
		}
	}
}

void send_pack(char* mes)
{
	int size = init_packet(&to, mes);

	int bytes = sendto(sock_send, (void*)buf, size, 0,
		(struct sockaddr*)&to, (socklen_t)sizeof(struct sockaddr_in));

	if (bytes < 0)
	{
		perror("sendto ");
		exit(-1);
	}
}

int init_packet(struct sockaddr_in* sock, char* mes)
{
	struct iphdr* iph = (struct iphdr*)buf;
	struct udphdr* udph = (struct udphdr*)(buf + IP_HDR_LEN);
	char* data = buf + IP_HDR_LEN + UDP_HDR_LEN;
	strcpy(data, mes);

	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = IP_HDR_LEN + UDP_HDR_LEN + strlen(data) + sizeof(int);
	iph->frag_off = 0;
	iph->ttl = 255;
	iph->protocol = IPPROTO_UDP;
	iph->saddr = server.sin_addr.s_addr;
	iph->daddr = sock->sin_addr.s_addr;

	udph->source = server.sin_port;
	udph->dest = sock->sin_port;
	udph->len = htons(UDP_HDR_LEN + strlen(data));
	udph->check = 0;

	return iph->tot_len;
}

void init_sock(struct sockaddr_in* sock, char* ip)
{
	memset(sock, 0, sizeof(struct sockaddr_in));
	sock->sin_addr.s_addr = inet_addr(ip);
	sock->sin_family = AF_INET;
	sock->sin_port = htons((ushort)PORT);
}

void getip(char* ip)
{
	struct ifaddrs* iff, * ifn;

	if (getifaddrs(&iff) < 0)
	{
		perror("getifaddrs ");
		exit(-1);
	}

	for (ifn = iff; ifn != NULL; ifn = ifn->ifa_next)
	{
		strcpy(ip, inet_ntoa(((struct sockaddr_in*)ifn->ifa_addr)->sin_addr));

		if (ip[1] != '.' && strncmp(ip, "127", 3) != 0)
		{
			return;
		}
	}
}

void writeip(char* ip)
{
	printf("\nWrite IP:\n");

	scanf("%s", ip);
	ip[15] = '\0';

	while (!isvalidip(ip))
	{
		printf("Invalid address, try again\n");
		scanf("%s", ip);
		ip[15] = '\0';
	}

	return;
}

int isvalidip(char* ip)
{
	int dots = 0, digits = 0, numbers = 0;

	for (; *ip; ip++)
	{
		if (isdigit(*ip) && digits == 0)
		{
			numbers++;
			digits++;
		}
		else if (isdigit(*ip) && digits < 3)
		{
			digits++;
		}
		else if (*ip == '.')
		{
			if (numbers == 0)
			{
				return 0;
			}
			else
			{
				char num[4] = "";
				strncpy(num, (ip - digits), digits);

				int int_num = atoi(num);
				if (int_num < 0 || int_num>254)
				{
					return 0;
				}
			}

			dots++;
			digits = 0;
		}
		else
		{
			return 0;
		}
	}

	if (dots == 3 && numbers == 4) {
		return 1;
	}
	else return 0;
}

int getch(void)
{
	struct termios oldattr, newattr;
	int ch;
	tcgetattr(STDIN_FILENO, &oldattr);
	newattr = oldattr;
	newattr.c_lflag &= ~(ICANON);
	tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
	return ch;
}
