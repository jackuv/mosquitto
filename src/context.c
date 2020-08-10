/*
Copyright (c) 2009-2020 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License v1.0
and Eclipse Distribution License v1.0 which accompany this distribution.
 
The Eclipse Public License is available at
   http://www.eclipse.org/legal/epl-v10.html
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.
 
Contributors:
   Roger Light - initial implementation and documentation.
*/
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>


#include "config.h"

#include <assert.h>
#include <time.h>

#include "mosquitto_broker_internal.h"
#include "alias_mosq.h"
#include "memory_mosq.h"
#include "packet_mosq.h"
#include "property_mosq.h"
#include "time_mosq.h"
#include "util_mosq.h"
#include "will_mosq.h"

#include "uthash.h"

struct mosquitto *context__init(struct mosquitto_db *db, mosq_sock_t sock, int threadIndex)
{
	struct mosquitto *context;
	char address[1024];

	context = calloc(1, sizeof(struct mosquitto));
	if(!context) return NULL;
	
	context->pollfd_index = -1;
	mosquitto__set_state(context, mosq_cs_new);
	context->sock = sock;
	context->last_msg_in = mosquitto_time();
	context->next_msg_out = mosquitto_time() + 60;
	context->keepalive = 60; /* Default to 60s */
	context->clean_start = true;
	context->id = NULL;
	context->last_mid = 0;
	context->will = NULL;
	context->username = NULL;
	context->password = NULL;
	context->listener = NULL;
	context->acl_list = NULL;
	
	/* is_bridge records whether this client is a bridge or not. This could be
	 * done by looking at context->bridge for bridges that we create ourself,
	 * but incoming bridges need some other way of being recorded. */
	context->is_bridge = false;

	context->in_packet.payload = NULL;
	packet__cleanup(&context->in_packet);
	context->out_packet = NULL;
	context->current_out_packet = NULL;

	context->address = NULL;
	if((int)sock >= 0){
		if(!net__socket_get_address(sock, address, 1024)){
			context->address = strdup(address);
		}
		if(!context->address){
			/* getpeername and inet_ntop failed and not a bridge */
			free(context);
			return NULL;
		}
	}
	context->bridge = NULL;
	context->msgs_in.inflight_maximum = db->config->max_inflight_messages;
	context->msgs_out.inflight_maximum = db->config->max_inflight_messages;
	context->msgs_in.inflight_quota = db->config->max_inflight_messages;
	context->msgs_out.inflight_quota = db->config->max_inflight_messages;
	context->maximum_qos = 2;

	context->vayo_client_mask = db->config->vayo_client_mask;
	context->vayo_topic_mask = db->config->vayo_topic_mask;
	context->vayo_topic_mask = db->config->vayo_topic_mask;
	
#ifdef WITH_TLS
	context->ssl = NULL;
#endif

	if((int)context->sock >= 0){
		// int threadIndex = rand() % MAX_THREADS;
		// int threadIndex = getThreadIndex(db);
		context->threadIndex = threadIndex;
		context->onceHandled = 0;
		context->forceToDelete = 0;
		if(threadIndex == 0)
		{
			HASH_ADD(hh_sock0, db->contexts_by_sock0, sock, sizeof(context->sock), context);
		}
		else if(threadIndex == 1)
		{
			HASH_ADD(hh_sock1, db->contexts_by_sock1, sock, sizeof(context->sock), context);
		}
		else if(threadIndex == 2)
		{
			HASH_ADD(hh_sock2, db->contexts_by_sock2, sock, sizeof(context->sock), context);	
		}
		else if(threadIndex == 3)
		{
			HASH_ADD(hh_sock3, db->contexts_by_sock3, sock, sizeof(context->sock), context);
		}
		else if(threadIndex == 4)
		{
			HASH_ADD(hh_sock4, db->contexts_by_sock4, sock, sizeof(context->sock), context);
		}
		else if(threadIndex == 5)
		{
			HASH_ADD(hh_sock5, db->contexts_by_sock5, sock, sizeof(context->sock), context);
		}
		else if(threadIndex == 6)
		{
			HASH_ADD(hh_sock6, db->contexts_by_sock6, sock, sizeof(context->sock), context);
		}
		else if(threadIndex == 7)
		{
			HASH_ADD(hh_sock7, db->contexts_by_sock7, sock, sizeof(context->sock), context);
		}
	}
	return context;
}

/*
 * This will result in any outgoing packets going unsent. If we're disconnected
 * forcefully then it is usually an error condition and shouldn't be a problem,
 * but it will mean that CONNACK messages will never get sent for bad protocol
 * versions for example.
 */
void context__cleanup(struct mosquitto_db *db, struct mosquitto *context, bool do_free)
{
	struct mosquitto__packet *packet;
#ifdef WITH_BRIDGE
	int i;
#endif

	if(!context) return;
		
#ifdef WITH_BRIDGE
	if(context->bridge){
		for(i=0; i<db->bridge_count; i++){
			if(db->bridges[i] == context){
				db->bridges[i] = NULL;
			}
		}
		free(context->bridge->local_clientid);
		context->bridge->local_clientid = NULL;

		free(context->bridge->local_username);
		context->bridge->local_username = NULL;

		free(context->bridge->local_password);
		context->bridge->local_password = NULL;

		if(context->bridge->remote_clientid != context->id){
			free(context->bridge->remote_clientid);
		}
		context->bridge->remote_clientid = NULL;

		if(context->bridge->remote_username != context->username){
			free(context->bridge->remote_username);
		}
		context->bridge->remote_username = NULL;

		if(context->bridge->remote_password != context->password){
			free(context->bridge->remote_password);
		}
		context->bridge->remote_password = NULL;
	}
#endif

	alias__free_all(context);

	free(context->auth_method);
	context->auth_method = NULL;

	free(context->username);
	context->username = NULL;

	free(context->password);
	context->password = NULL;

	net__socket_close(db, context);
	if(do_free || context->clean_start){
		sub__clean_session(db, context);
		db__messages_delete(db, context);
	}

	free(context->address);
	context->address = NULL;

	context__send_will(db, context);

	if(context->id){
		context__remove_from_by_id(db, context);
		free(context->id);
		context->id = NULL;
	}
	packet__cleanup(&(context->in_packet));
	if(context->current_out_packet){
		packet__cleanup(context->current_out_packet);
		free(context->current_out_packet);
		context->current_out_packet = NULL;
	}
	while(context->out_packet){
		packet__cleanup(context->out_packet);
		packet = context->out_packet;
		context->out_packet = context->out_packet->next;
		free(packet);
	}
	if(do_free || context->clean_start){
		db__messages_delete(db, context);
	}
#if defined(WITH_BROKER) && defined(__GLIBC__) && defined(WITH_ADNS)
	if(context->adns){
		gai_cancel(context->adns);
		free((struct addrinfo *)context->adns->ar_request);
		free(context->adns);
	}
#endif
	if(do_free){
		free(context);
	}
	
}


void context__send_will(struct mosquitto_db *db, struct mosquitto *ctxt)
{
	if(ctxt->state != mosq_cs_disconnecting && ctxt->will){
		if(ctxt->will_delay_interval > 0){
			will_delay__add(ctxt);
			return;
		}

		if(mosquitto_acl_check(db, ctxt,
					ctxt->will->msg.topic,
					ctxt->will->msg.payloadlen,
					ctxt->will->msg.payload,
					ctxt->will->msg.qos,
					ctxt->will->msg.retain,
					MOSQ_ACL_WRITE) == MOSQ_ERR_SUCCESS){

			/* Unexpected disconnect, queue the client will. */
			db__messages_easy_queue(db, ctxt,
					ctxt->will->msg.topic,
					ctxt->will->msg.qos,
					ctxt->will->msg.payloadlen,
					ctxt->will->msg.payload,
					ctxt->will->msg.retain,
					ctxt->will->expiry_interval,
					&ctxt->will->properties);
		}
	}
	will__clear(ctxt);
}


void context__disconnect(struct mosquitto_db *db, struct mosquitto *context)
{
	if(mosquitto__get_state(context) == mosq_cs_disconnected){
		return;
	}

	vayo_mutex_lock(&db->delete_mutex);
	net__socket_close(db, context);

	context__send_will(db, context);
	if(context->session_expiry_interval == 0){
		/* Client session is due to be expired now */
#ifdef WITH_BRIDGE
		if(!context->bridge)
#endif
		{

			if(context->will_delay_interval == 0){
				/* This will be done later, after the will is published for delay>0. */
				context__add_to_disused(db, context);
			}
		}
	}else{
		session_expiry__add(db, context);
	}
	mosquitto__set_state(context, mosq_cs_disconnected);
	vayo_mutex_unlock(&db->delete_mutex);
}

int counter = 0;
void context__add_to_disused(struct mosquitto_db *db, struct mosquitto *context)
{
	if(context->state == mosq_cs_disused) return;

	mosquitto__set_state(context, mosq_cs_disused);

	if(context->id){
		context__remove_from_by_id(db, context);
		free(context->id);
		context->id = NULL;
	}

	vayo_mutex_lock(&db->delete_mutex);
	context->for_free_next = db->ll_for_free;
	db->ll_for_free = context;
	counter++;
	vayo_mutex_unlock(&db->delete_mutex);
}

void context__free_disusedOrg(struct mosquitto_db *db)
{
	struct mosquitto *context, *next;
#ifdef WITH_WEBSOCKETS
	struct mosquitto *last = NULL;
#endif
	assert(db);

	context = db->ll_for_free;
	db->ll_for_free = NULL;
	while(context){
#ifdef WITH_WEBSOCKETS
		if(context->wsi){
			/* Don't delete yet, lws hasn't finished with it */
			if(last){
				last->for_free_next = context;
			}else{
				db->ll_for_free = context;
			}
			next = context->for_free_next;
			context->for_free_next = NULL;
			last = context;
			context = next;
		}else
#endif
		{
			next = context->for_free_next;
			context__cleanup(db, context, true);
			context = next;
		}
	}
}

void context__free_disused(struct mosquitto_db *db, int threadIndex)
{
	struct mosquitto *context, *next, *prev;
#ifdef WITH_WEBSOCKETS
	struct mosquitto *last = NULL;
#endif
	assert(db);

	vayo_mutex_lock(&db->delete_mutex);
	context = db->ll_for_free;
	prev = context;
	// db->ll_for_free = NULL;

	while(context){
		#ifdef WITH_WEBSOCKETS
		if(context->wsi){
			/* Don't delete yet, lws hasn't finished with it */
			if(last){
				last->for_free_next = context;
			}else{
				db->ll_for_free = context;
			}
			next = context->for_free_next;
			context->for_free_next = NULL;
			last = context;
			context = next;
		}else
#endif
		{
			next = context->for_free_next;
			
			if(context->threadIndex == threadIndex)
			{
				context->for_free_next = NULL;
				prev->for_free_next = next;
				if(context == db->ll_for_free)
				{
					prev = next;
					db->ll_for_free = next;
				}
								
				context__cleanup(db, context, true);
				counter--;
			} else
			{
				prev = context;
			}
			context = next;
		}
	}
	vayo_mutex_unlock(&db->delete_mutex);
}


void context__remove_from_by_id(struct mosquitto_db *db, struct mosquitto *context)
{
	if(context->removed_from_by_id == false && context->id){
		vayo_mutex_lock(&db->delete_mutex);
		db->threadClients[context->threadIndex]--;
		switch (context->threadIndex)
		{
			case 0:
				HASH_DELETE(hh_id0, db->contexts_by_id0, context);
				break;
			case 1:
				HASH_DELETE(hh_id1, db->contexts_by_id1, context);
				break;
			case 2:
				HASH_DELETE(hh_id2, db->contexts_by_id2, context);
				break;
			case 3:
				HASH_DELETE(hh_id3, db->contexts_by_id3, context);
				break;
			case 4:
				HASH_DELETE(hh_id4, db->contexts_by_id4, context);
				break;
			case 5:
				HASH_DELETE(hh_id5, db->contexts_by_id5, context);
				break;
			case 6:
				HASH_DELETE(hh_id6, db->contexts_by_id6, context);
				break;
			case 7:
				HASH_DELETE(hh_id7, db->contexts_by_id7, context);
				break;
			default:
				break;
		}
		context->removed_from_by_id = true;
		vayo_mutex_unlock(&db->delete_mutex);
	}
}

