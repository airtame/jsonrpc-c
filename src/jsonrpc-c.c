/*
 * jsonrpc-c.c
 *
 *  Created on: Oct 11, 2012
 *      Author: hmng
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include "jsonrpc-c.h"

#ifdef _WIN32
#define close(socket) closesocket(socket)
#endif

static int _call_id = 0;
struct ev_loop *loop;

#ifdef _WIN32
/* Hack to get inet_ntop to work on Windows */
#ifndef inet_ntop
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#endif
#include <string.h>

const char* inet_ntop(int af, const void* src, char* dst, int cnt) {
#ifdef InetNtop
    InetNtop(af, src, dst, cnt);
#else
    static const char fmt[] = "%u.%u.%u.%u";
    char tmp[sizeof "255.255.255.255"];
    unsigned char *charsrc = (unsigned char *)src;

    if (af == AF_INET) {
        if (cnt < strlen("255.255.255.255")) {
            return (NULL);
        }
        sprintf(tmp, fmt, charsrc[0], charsrc[1], charsrc[2], charsrc[3]);
        strcpy(dst, tmp);
        return (dst);
    } else {
        errno = EAFNOSUPPORT;
        return (NULL);
    }
#if 0
    struct sockaddr_in srcaddr;
    memset(&srcaddr, 0, sizeof(struct sockaddr_in));
    memcpy(&(srcaddr.sin_addr), src, sizeof(srcaddr.sin_addr));
    srcaddr.sin_family = af;
    if (WSAAddressToString((struct sockaddr*)&srcaddr, sizeof(struct sockaddr_in), 0, dst, (LPDWORD)&cnt) != 0) {
        DWORD rv = WSAGetLastError();
        printf("WSAAdressToString(): %d\n", rv);
        return NULL;
    }
    return dst;
#endif
#endif
}
#endif

#endif

int get_unique_id() {
    // TODO: increment the call id in a way that is thread safe
    return _call_id++;
}

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

static int send_response(struct jrpc_connection * conn, char *response) {
	int fd = conn->fd;
	if (conn->debug_level > 1)
		printf("JSON Response:\n%s\n", response);
	send(fd, response, strlen(response), 0);
	send(fd, "\n", 1, 0);
	return 0;
}

static int send_error(struct jrpc_connection * conn, int code, char* message,
		cJSON * id) {
	int return_value = 0;
	cJSON *result_root = cJSON_CreateObject();
	cJSON *error_root = cJSON_CreateObject();
	cJSON_AddNumberToObject(error_root, "code", code);
	cJSON_AddStringToObject(error_root, "message", message);
	cJSON_AddItemToObject(result_root, "error", error_root);
	cJSON_AddItemToObject(result_root, "id", id);
	char * str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	free(message);
	return return_value;
}

static int send_result(struct jrpc_connection * conn, cJSON * result,
		cJSON * id) {
	int return_value = 0;
	cJSON *result_root = cJSON_CreateObject();
	if (result)
		cJSON_AddItemToObject(result_root, "result", result);
	cJSON_AddItemToObject(result_root, "id", id);

	char * str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	return return_value;
}

static int invoke_procedure(struct jrpc_server *server,
		struct jrpc_connection * conn, char *name, cJSON *params, cJSON *id) {
	cJSON *returned = NULL;
	int procedure_found = 0;
	jrpc_context ctx;
	ctx.error_code = 0;
	ctx.error_message = NULL;
	int i = server->procedure_count;
	while (i--) {
		if (!strcmp(server->procedures[i].name, name)) {
			procedure_found = 1;
			ctx.data = server->procedures[i].data;
			returned = server->procedures[i].function(&ctx, params, id);
			break;
		}
	}
	if (!procedure_found)
		return send_error(conn, JRPC_METHOD_NOT_FOUND,
				strdup("Method not found."), id);
	else {
		if (ctx.error_code)
			return send_error(conn, ctx.error_code, ctx.error_message, id);
		else
			return send_result(conn, returned, id);
	}
}

static int eval_request(struct jrpc_server *server,
		struct jrpc_connection * conn, cJSON *root) {
	cJSON *method, *params, *id;
	method = cJSON_GetObjectItem(root, "method");
	if (method != NULL && method->type == cJSON_String) {
		params = cJSON_GetObjectItem(root, "params");
		if (params == NULL|| params->type == cJSON_Array
		|| params->type == cJSON_Object) {
			id = cJSON_GetObjectItem(root, "id");
			if (id == NULL|| id->type == cJSON_String
			|| id->type == cJSON_Number) {
			//We have to copy ID because using it on the reply and deleting the response Object will also delete ID
				cJSON * id_copy = NULL;
				if (id != NULL)
					id_copy =
							(id->type == cJSON_String) ? cJSON_CreateString(
									id->valuestring) :
									cJSON_CreateNumber(id->valueint);
				if (server->debug_level)
					printf("Method Invoked: %s\n", method->valuestring);
				return invoke_procedure(server, conn, method->valuestring,
						params, id_copy);
			}
		}
	}
	send_error(conn, JRPC_INVALID_REQUEST,
			strdup("The JSON sent is not a valid Request object."), NULL);
	return -1;
}

static void close_connection(struct ev_loop *loop, ev_io *w) {
	ev_io_stop(loop, w);
	close(((struct jrpc_connection *) w)->fd);
	free(((struct jrpc_connection *) w)->buffer);
	free(((struct jrpc_connection *) w));
}

static void connection_cb(struct ev_loop *loop, ev_io *w, int revents) {
	struct jrpc_connection *conn;
	struct jrpc_server *server = (struct jrpc_server *) w->data;
	size_t bytes_read = 0;
	//get our 'subclassed' event watcher
	conn = (struct jrpc_connection *) w;
	int fd = conn->fd;
	if (conn->pos == (conn->buffer_size - 1)) {
		char * new_buffer = realloc(conn->buffer, conn->buffer_size *= 2);
		if (new_buffer == NULL) {
			perror("Memory error");
			return close_connection(loop, w);
		}
		conn->buffer = new_buffer;
		memset(conn->buffer + conn->pos, 0, conn->buffer_size - conn->pos);
	}
	// can not fill the entire buffer, string must be NULL terminated
	int max_read_size = conn->buffer_size - conn->pos - 1;
	if ((bytes_read = recv(fd, conn->buffer + conn->pos, max_read_size, 0))
			== -1) {
		perror("read");
		return close_connection(loop, w);
	}
	if (!bytes_read) {
		// client closed the sending half of the connection
		if (server->debug_level)
			printf("Client closed connection.\n");
		return close_connection(loop, w);
	} else {
		cJSON *root;
		char *end_ptr = NULL;
		conn->pos += bytes_read;

		if ((root = cJSON_Parse_Stream(conn->buffer, &end_ptr)) != NULL) {
			if (server->debug_level > 1) {
				char * str_result = cJSON_Print(root);
				printf("Valid JSON Received:\n%s\n", str_result);
				free(str_result);
			}

			if (root->type == cJSON_Object) {
				eval_request(server, conn, root);
			}
			//shift processed request, discarding it
			memmove(conn->buffer, end_ptr, strlen(end_ptr) + 2);

			conn->pos = strlen(end_ptr);
			memset(conn->buffer + conn->pos, 0,
					conn->buffer_size - conn->pos - 1);

			cJSON_Delete(root);
		} else {
			// did we parse the all buffer? If so, just wait for more.
			// else there was an error before the buffer's end
			if (end_ptr != (conn->buffer + conn->pos)) {
				if (server->debug_level) {
					printf("INVALID JSON Received:\n---\n%s\n---\n",
							conn->buffer);
				}
				send_error(conn, JRPC_PARSE_ERROR,
						strdup(
								"Parse error. Invalid JSON was received by the server."),
						NULL);
				return close_connection(loop, w);
			}
		}
	}

}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents) {
	char s[INET6_ADDRSTRLEN];
	struct jrpc_connection *connection_watcher;
	connection_watcher = malloc(sizeof(struct jrpc_connection));
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	sin_size = sizeof their_addr;
	connection_watcher->fd = accept(w->fd, (struct sockaddr *) &their_addr,
			&sin_size);
	if (connection_watcher->fd == -1) {
		perror("accept");
		free(connection_watcher);
	} else {
		if (((struct jrpc_server *) w->data)->debug_level) {
			inet_ntop(their_addr.ss_family,
					get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
			printf("server: got connection from %s\n", s);
		}
		ev_io_init(&connection_watcher->io, connection_cb,
				connection_watcher->fd, EV_READ);
		//copy pointer to struct jrpc_server
		connection_watcher->io.data = w->data;
		connection_watcher->buffer_size = 1500;
		connection_watcher->buffer = malloc(1500);
		memset(connection_watcher->buffer, 0, 1500);
		connection_watcher->pos = 0;
		//copy debug_level, struct jrpc_connection has no pointer to struct jrpc_server
		connection_watcher->debug_level =
				((struct jrpc_server *) w->data)->debug_level;
		ev_io_start(loop, &connection_watcher->io);
	}
}

int jrpc_server_init(struct jrpc_server *server, int port_number) {
    loop = EV_DEFAULT;
    return jrpc_server_init_with_ev_loop(server, port_number, loop);
}

int jrpc_server_init_with_ev_loop(struct jrpc_server *server, 
        int port_number, struct ev_loop *loop) {
	memset(server, 0, sizeof(struct jrpc_server));
	server->loop = loop;
	server->port_number = port_number;
	char * debug_level_env = getenv("JRPC_DEBUG");
	if (debug_level_env == NULL)
		server->debug_level = 0;
	else {
		server->debug_level = strtol(debug_level_env, NULL, 10);
		printf("JSONRPC-C Debug level %d\n", server->debug_level);
	}
	return __jrpc_server_start(server);
}

static int __jrpc_server_start(struct jrpc_server *server) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in sockaddr;
	int len;
	int yes = 1;
	int rv;
	char PORT[6];
	sprintf(PORT, "%d", server->port_number);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(WINSOCK_VERSION, &wsaData)) {
        printf("winsock could not be initiated\n");
        WSACleanup();
        return 0;
    }
#endif

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
				== -1) {
			perror("setsockopt");
			exit(1);
		}

#ifndef _WIN32
		// don't generate the SIGPIPE signal, but return EPIPE instead
		if (setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(int))
				== -1) {
			perror("setsockopt");
			exit(1);
		}
#endif

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		len = sizeof(sockaddr);
		if (getsockname(sockfd, (struct sockaddr *) &sockaddr, &len) == -1) {
			close(sockfd);
			perror("server: getsockname");
			continue;
		}
		server->port_number = ntohs( sockaddr.sin_port );

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, 5) == -1) {
		perror("listen");
		exit(1);
	}
	if (server->debug_level)
		printf("server: waiting for connections...\n");

	ev_io_init(&server->listen_watcher, accept_cb, sockfd, EV_READ);
	server->listen_watcher.data = server;
	ev_io_start(server->loop, &server->listen_watcher);
	return 0;
}

void jrpc_server_run(struct jrpc_server *server){
	ev_run(server->loop, 0);
}

int jrpc_server_stop(struct jrpc_server *server) {
	ev_break(server->loop, EVBREAK_ALL);
	return 0;
}

void jrpc_server_destroy(struct jrpc_server *server){
	/* Don't destroy server */
	int i;
	for (i = 0; i < server->procedure_count; i++){
		jrpc_procedure_destroy( &(server->procedures[i]) );
	}
	free(server->procedures);
#ifdef _WIN32
    WSACleanup();
#endif
}

static void jrpc_procedure_destroy(struct jrpc_procedure *procedure){
	if (procedure->name){
		free(procedure->name);
		procedure->name = NULL;
	}
	if (procedure->data){
		free(procedure->data);
		procedure->data = NULL;
	}
}

int jrpc_register_procedure(struct jrpc_server *server,
		jrpc_function function_pointer, char *name, void * data) {
	int i = server->procedure_count++;
	if (!server->procedures)
		server->procedures = malloc(sizeof(struct jrpc_procedure));
	else {
		struct jrpc_procedure * ptr = realloc(server->procedures,
				sizeof(struct jrpc_procedure) * server->procedure_count);
		if (!ptr)
			return -1;
		server->procedures = ptr;

	}
	if ((server->procedures[i].name = strdup(name)) == NULL)
		return -1;
	server->procedures[i].function = function_pointer;
	server->procedures[i].data = data;
	return 0;
}

int jrpc_deregister_procedure(struct jrpc_server *server, char *name) {
	/* Search the procedure to deregister */
	int i;
	int found = 0;
	if (server->procedures){
		for (i = 0; i < server->procedure_count; i++){
			if (found)
				server->procedures[i-1] = server->procedures[i];
			else if(!strcmp(name, server->procedures[i].name)){
				found = 1;
				jrpc_procedure_destroy( &(server->procedures[i]) );
			}
		}
		if (found){
			server->procedure_count--;
			if (server->procedure_count){
				struct jrpc_procedure * ptr = realloc(server->procedures,
					sizeof(struct jrpc_procedure) * server->procedure_count);
				if (!ptr){
					perror("realloc");
					return -1;
				}
				server->procedures = ptr;
			}else{
				server->procedures = NULL;
			}
		}
	} else {
		fprintf(stderr, "server : procedure '%s' not found\n", name);
		return -1;
	}
	return 0;
}

// CLIENT related code
int jrpc_client_init(struct jrpc_client *client) {
    return -1;
}

int jrpc_client_connect(struct jrpc_client *client, char *ip, int port) {
    struct sockaddr_in serv_addr;
    struct hostent *server;
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(WINSOCK_VERSION, &wsaData)) {
        printf("winsock could not be initiated\n");
        WSACleanup();
        return -1;
    }
#endif

    client->connection_watcher.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->connection_watcher.fd < 0) {
        if (client->debug_level) {
            printf("error connecting to server: Cannot create socket");
        }
        goto close_fd_error;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(client->connection_watcher.fd, (const struct sockaddr *)(&(serv_addr)), sizeof(serv_addr)) < 0) {
        if (client->debug_level) {
            printf("socket connect failed");
        }
        goto close_fd_error;
    }
    return 0;

    close_fd_error:
    close(client->connection_watcher.fd);

    if (client->debug_level) {
        printf("Cannot connect to %s:%d", ip, port);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return -1;
}

int jrpc_client_disconnect(struct jrpc_client *client) {
    close(client->connection_watcher.fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

static cJSON* receive_reply(struct jrpc_client *client) {
	struct jrpc_connection *conn =
	        (struct jrpc_connection *) &client->connection_watcher;
	size_t bytes_read = 0;
	int fd = conn->fd;
	// TODO: improve the way of waiting for a valid jSON reply
	while (1) {
	    if (conn->pos == (conn->buffer_size - 1)) {
	        char * new_buffer = realloc(conn->buffer, conn->buffer_size *= 2);
	        if (new_buffer == NULL) {
	            perror("Memory error");
	            jrpc_client_disconnect(client);
	            return NULL;
	        }
	        conn->buffer = new_buffer;
	        memset(conn->buffer + conn->pos, 0, conn->buffer_size - conn->pos);
	    }
	    // can not fill the entire buffer, string must be NULL terminated
	    int max_read_size = conn->buffer_size - conn->pos - 1;
	    if ((bytes_read = recv(fd, conn->buffer + conn->pos, max_read_size, 0))
	            == -1) {
	        perror("read");
	        jrpc_client_disconnect(client);
	        return NULL;
	    }
	    if (!bytes_read) {
	        // client closed the sending half of the connection
	        if (client->debug_level)
	            printf("Client closed connection.\n");
	        jrpc_client_disconnect(client);
	        return NULL;
	    } else {
	        cJSON *root;
	        char *end_ptr = NULL;
	        conn->pos += bytes_read;
	        if ((root = cJSON_Parse_Stream(conn->buffer, &end_ptr)) != NULL) {
	            if (client->debug_level > 1) {
	                char * str_result = cJSON_Print(root);
	                printf("Valid JSON Received:\n%s\n", str_result);
	                free(str_result);
	            }
	            return root;
	        } else {
	            if (end_ptr != (conn->buffer + conn->pos)) {
	                if (client->debug_level) {
	                    printf("INVALID JSON Received:\n---\n%s\n---\n",
	                            conn->buffer);
	                }
	                continue;
	            }
	        }
	    }
	}
	return NULL;
}

cJSON* jrpc_client_call(struct jrpc_client *client, char *method_name, int no_args, ...) {
    // reinit the buffers used to get the response
    client->connection_watcher.buffer_size = 1500;
    client->connection_watcher.buffer = malloc(1500);
    memset(client->connection_watcher.buffer, 0, 1500);
    client->connection_watcher.pos = 0;

    va_list argp;
    if (client == NULL || method_name == NULL) return NULL;

    cJSON *params = cJSON_CreateArray();
    va_start(argp, no_args);
    for (int i = 0; i < no_args; i++) {
        char *argv = va_arg(argp, char *);
        cJSON_AddItemToArray(params, cJSON_CreateString((const char*)argv));
    }
    va_end(argp);

    cJSON *json_call = cJSON_CreateObject();
    cJSON_AddItemToObject(json_call, "method", cJSON_CreateString(method_name));
    cJSON_AddItemToObject(json_call, "params", params);

    char call_id[10];
    sprintf(call_id, "%d", get_unique_id());
    cJSON_AddItemToObject(json_call, "id", cJSON_CreateString(call_id));

    char *call_str = cJSON_Print(json_call);
    if (client->debug_level) {
        printf("calling remote with JSON: %s", call_str);
    }

    int to_send = strlen(call_str);
    int sent = 0;
    while (sent < to_send) {
        sent = send(client->connection_watcher.fd, (void *) call_str, to_send, 0);
        if (sent == -1) goto call_error;
        to_send -= sent;
    }
    send(client->connection_watcher.fd, (void *) "\n", 1, 0);
    free(call_str);

    cJSON_Delete(json_call);
    // TODO: make it robust and threading safe, using the ev.c
    cJSON *reply= receive_reply(client);
    free(client->connection_watcher.buffer);
    return reply;

    call_error:
    printf("Error when trying to call the server");
    return NULL;
}

int jrpc_client_async_call(struct jrpc_client *client, char *method_name, rpc_reply_callback_t *reply_cb, ...) {

    return -1;
}

int jrpc_client_destroy(struct jrpc_client *client) {

    return 0;
}