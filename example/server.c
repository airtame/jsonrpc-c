/*
 * server.c
 *
 *  Created on: Oct 9, 2012
 *      Author: hmng
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include "jsonrpc-c.h"

#define PORT 1234  // the port users will be connecting to

struct jrpc_object my_server;

//void *talking_thread(void* args) {
void cbcb(struct jrpc_object *server) {
    cJSON *result = jrpc_client_call(&server, "sayHello", 0);
    if (result) {
        char *str_res = cJSON_Print(result);
        printf("Hello result: %s\n", str_res);
        free(str_res);
        cJSON_Delete(result);
    }
}

cJSON * addTwoInts(jrpc_context *ctx, cJSON *params, cJSON *id) {
    cJSON* val1 = cJSON_GetArrayItem(params, 0);
    cJSON* val2 = cJSON_GetArrayItem(params, 1);
    int res = atoi(val1->valuestring) + atoi(val2->valuestring);
    return cJSON_CreateNumber(res);
}

cJSON * say_hello(jrpc_context * ctx, cJSON * params, cJSON *id) {
	return cJSON_CreateString("Hello!");
}

cJSON * exit_server(jrpc_context * ctx, cJSON * params, cJSON *id) {
	jrpc_server_stop(&my_server);
	return cJSON_CreateString("Bye!");
}

int main(void) {
	jrpc_server_init(&my_server, PORT);

	my_server.connection_callback = cbcb;
	jrpc_register_procedure(&my_server, say_hello, "sayHello", NULL );
	jrpc_register_procedure(&my_server, addTwoInts, "addTwoInts", NULL );
	jrpc_register_procedure(&my_server, exit_server, "exit", NULL );
	jrpc_server_run(&my_server);
	jrpc_server_destroy(&my_server);
	return 0;
}
