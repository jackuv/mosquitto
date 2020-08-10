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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "memory_mosq.h"

#ifdef REAL_WITH_MEMORY_TRACKING
#  if defined(__APPLE__)
#    include <malloc/malloc.h>
#    define malloc_usable_size malloc_size
#  elif defined(__FreeBSD__)
#    include <malloc_np.h>
#  else
#    include <malloc.h>
#  endif
#endif

#ifdef REAL_WITH_MEMORY_TRACKING
static unsigned long memcount = 0;
static unsigned long max_memcount = 0;
#endif

#ifdef WITH_BROKER
static size_t mem_limit = 0;
void memory__set_limit(size_t lim)
{
	mem_limit = lim;
}
#endif

void *mosquitto__callocA(size_t nmemb, size_t size)
{
	void *mem;
#ifdef REAL_WITH_MEMORY_TRACKING
	if(mem_limit && memcount + size > mem_limit){
		return NULL;
	}
#endif
	mem = calloc(nmemb, size);

#ifdef REAL_WITH_MEMORY_TRACKING
	if(mem){
		memcount += malloc_usable_size(mem);
		if(memcount > max_memcount){
			max_memcount = memcount;
		}
	}
#endif

	return mem;
}

void mosquitto__freeA(void *mem)
{
#ifdef REAL_WITH_MEMORY_TRACKING
	if(!mem){
		return;
	}
	memcount -= malloc_usable_size(mem);
#endif
	free(mem);
}

void *mosquitto__mallocA(size_t size)
{
	void *mem;

#ifdef REAL_WITH_MEMORY_TRACKING
	if(mem_limit && memcount + size > mem_limit){
		return NULL;
	}
#endif

	mem = malloc(size);

#ifdef REAL_WITH_MEMORY_TRACKING
	if(mem){
		memcount += malloc_usable_size(mem);
		if(memcount > max_memcount){
			max_memcount = memcount;
		}
	}
#endif

	return mem;
}

#ifdef REAL_WITH_MEMORY_TRACKING
unsigned long mosquitto__memory_used(void)
{
	return memcount;
}

unsigned long mosquitto__max_memory_used(void)
{
	return max_memcount;
}
#endif

void *mosquitto__reallocA(void *ptr, size_t size)
{
	void *mem;
#ifdef REAL_WITH_MEMORY_TRACKING
	if(mem_limit && memcount + size > mem_limit){
		return NULL;
	}
	if(ptr){
		memcount -= malloc_usable_size(ptr);
	}
#endif
	mem = realloc(ptr, size);

#ifdef REAL_WITH_MEMORY_TRACKING
	if(mem){
		memcount += malloc_usable_size(mem);
		if(memcount > max_memcount){
			max_memcount = memcount;
		}
	}
#endif

	return mem;
}

char *mosquitto__strdupA(const char *s)
{
	char *str;
#ifdef REAL_WITH_MEMORY_TRACKING
	if(mem_limit && memcount + strlen(s) > mem_limit){
		return NULL;
	}
#endif
	str = strdup(s);

#ifdef REAL_WITH_MEMORY_TRACKING
	if(str){
		memcount += malloc_usable_size(str);
		if(memcount > max_memcount){
			max_memcount = memcount;
		}
	}
#endif

	return str;
}

int split (const char *txt, char delim, char ***tokens)
{
    int *tklen, *t, count = 1;
    char **arr, *p = (char *) txt;

    while (*p != '\0') if (*p++ == delim) count += 1;
    t = tklen = calloc (count, sizeof (int));
    for (p = (char *) txt; *p != '\0'; p++) *p == delim ? *t++ : (*t)++;
    *tokens = arr = malloc (count * sizeof (char *));
    t = tklen;
    p = *arr++ = calloc (*(t++) + 1, sizeof (char *));
    while (*txt != '\0')
    {
        if (*txt == delim)
        {
            p = *arr++ = calloc (*(t++) + 1, sizeof (char *));
            txt++;
        }
        else *p++ = *txt++;
    }
    free (tklen);
    return count;
}

int vayo__strend(const char *s, const char *t)
{
	size_t ls = strlen(s); // find length of s
	size_t lt = strlen(t); // find length of t
	if (ls >= lt)  // check if t can fit in s
	{
		// point s to where t should start and compare the strings from there
		return (0 == memcmp(t, s + (ls - lt), lt));
	}
	return 0; // t was longer than s
}

char *vayo__strndup(const char *s, size_t n) {
	char *p = memchr(s, '\0', n);
	if (p != NULL)
		n = p - s;
	p = malloc(n + 1);
	if (p != NULL) {
		memcpy(p, s, n);
		p[n] = '\0';
	}
	return p;
}

char *vayo__topic_with_id(const char *topic, const char * id, int* len){
	*len = strlen(topic) + strlen(id) + 1;
	char* result = (char*)malloc(*len + 1);
	if (!result)
		return NULL;
	snprintf(result, *len + 1, "%s/%s", topic, id);
	result[*len] = '\0';
	return result;
}

int vayo_startsWith(const char *pre, const char *str)
{
	size_t lenpre = strlen(pre),
		lenstr = strlen(str);
	return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

char *vayo_strdup_without_id(const char *topic, const char* client_id_part, const char* vayo_topic_mask) {
	if (!topic || !vayo_topic_mask)
		return NULL;
	
	int len = strlen(topic);
	if (len < 2)
		return NULL;

	if (!vayo_startsWith(vayo_topic_mask, topic))
		return NULL;

	int i;
	for(i=len-1; i>=0; i--){
		if(topic[i] == '/')
			break;
	}
	
	if (i == 0 || i == len-1 || !vayo_startsWith(client_id_part, &topic[i+1]))
		return NULL;
			
	char* p = vayo__strndup(topic, i);
	if (!p)
		return NULL;

	return p;
}

char* vayo_concat(const char *s1, const char *s2)
{
	char *result = malloc(strlen(s1) + strlen(s2) + 1);
	if (!result)
		return NULL;

	strcpy(result, s1);
	strcat(result, s2);
	return result;
}

char* vayo_path_combine(const char *s1, const char *s2)
{
	char *result = malloc(strlen(s1) + strlen(s2) + 2);
	if (!result)
		return NULL;

	strcpy(result, s1);
	strcat(result, "/");
	strcat(result, s2);
	return result;
}
