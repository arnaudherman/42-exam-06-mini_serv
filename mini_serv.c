#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct client { int id; char *msg; } t_client;

t_client clients[1024];
int fd_max = 0, next_id = 0;
char bufRead[424242], bufWrite[424242];
fd_set active, readyRead, readyWrite;

int extract_message(char **buf, char **msg)
{
	char *newbuf;
	int i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char *newbuf;
	int len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void broadcast(int fd) {
	int len = strlen(bufWrite);
	for (int i = 0; i <= fd_max; i++)
		if (FD_ISSET(i, &readyWrite) && i != fd)
			send(i, bufWrite, len, 0);
}

void fatal_error() { write(2, "Fatal error\n", 12); exit(1); }

int main(int argc, char **argv) {
	if (argc != 2) { write(2, "Wrong number of arguments\n", 26); exit(1); }
	
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1) fatal_error();
	
	memset(clients, 0, sizeof(clients));
	FD_ZERO(&active);
	FD_SET(server_socket, &active);
	fd_max = server_socket;
	
	struct sockaddr_in server_addr, client_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(2130706433);
	server_addr.sin_port = htons(atoi(argv[1]));
	
	if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr))) fatal_error();
	if (listen(server_socket, 128)) fatal_error();
	
	while (1) {
		readyRead = readyWrite = active;
		if (select(fd_max + 1, &readyRead, &readyWrite, NULL, NULL) < 0) continue;
		
		for (int fd = 0; fd <= fd_max; fd++) {
			if (FD_ISSET(fd, &readyRead)) {
				if (fd == server_socket) {
					socklen_t client_len = sizeof(client_addr);
					int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
					if (client_socket < 0) continue;
					if (client_socket > fd_max) fd_max = client_socket;
					clients[client_socket].id = next_id++;
					clients[client_socket].msg = calloc(1, 1);
					if (!clients[client_socket].msg) fatal_error();
					memset(bufWrite, 0, sizeof(bufWrite));
					sprintf(bufWrite, "server: client %d just arrived\n", clients[client_socket].id);
					broadcast(client_socket);
					FD_SET(client_socket, &active);
					continue;
				} else {
					int nbytes = recv(fd, bufRead, sizeof(bufRead) - 1, 0);
					if (nbytes <= 0) {
						memset(bufWrite, 0, sizeof(bufWrite));
						sprintf(bufWrite, "server: client %d just left\n", clients[fd].id);
						broadcast(fd);
						free(clients[fd].msg);
						clients[fd].msg = NULL;
						FD_CLR(fd, &active);
						close(fd);
						break;
					} else {
						bufRead[nbytes] = '\0';
						clients[fd].msg = str_join(clients[fd].msg, bufRead);
						if (!clients[fd].msg) fatal_error();
						char *new_msg_part;
						int extract_result;
						while ((extract_result = extract_message(&clients[fd].msg, &new_msg_part))) {
							if (extract_result == -1) fatal_error();
							memset(bufWrite, 0, sizeof(bufWrite));
							sprintf(bufWrite, "client %d: %s", clients[fd].id, new_msg_part);
							broadcast(fd);
							free(new_msg_part);
						}
					}
				}
			}
		}
	}
}
