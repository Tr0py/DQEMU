#ifndef SET_H
#define SET_H

#include "linux_loop.h"
#include "offload_common.h"
#include "uname.h"
#include "uthash.h"

struct set
{
	unsigned char *element;
	unsigned char size;
	unsigned char max_size;
};

typedef struct set set_t;

typedef struct req_node
{
	int idx;
	int perm;
	struct req_node *next;
} req_node;

typedef struct PageMapDesc
{
	int on_master; /* if this page is on master side */
	int requestor;
	set_t owner_set;
	pthread_mutex_t owner_set_mutex; /* We lock the mutex when start fetching for it, until receives ack */
	int mutex_holder;				 /* Not very useful unless for debugging */
	int invalid_count;				 /* How many we should tell to invalidate */
	int cur_perm;
	req_node list_head; /* to record request list */
	int flag;
	uint32_t shadow_page_addr;
	int fs_notice_count; /* for the last time use of fs page */
} PageMapDesc;

typedef struct PageTable
{
	int page_addr; //without mask and less than 26bit
	PageMapDesc page_desc;
	UT_hash_handle hh;
	/* data */
} PageTable;

struct PageTable *find_page(PageTable **table, int page_addr);

void add_page(PageTable **table, struct PageTable *p);

typedef struct PageMapDesc_server
{
	int cur_perm;
	int is_false_sharing;
	uint32_t shadow_page_addr;
} PageMapDesc_server;

typedef struct PageTable_s
{
	int page_addr; //without mask and less than 26bit
	PageMapDesc_server page_desc;
	UT_hash_handle hh;
	/* data */
} PageTable_s;

struct PageTable_s *find_page_s(PageTable_s **table, int page_addr, int idx);

void add_page_s(PageTable_s **table, struct PageTable_s *p);

int insert(set_t *s, int t);

void clear(set_t *s);

int find(set_t *s, int n);

#endif
