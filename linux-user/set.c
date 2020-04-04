#include "set.h"

int insert(set_t *s, int t)
{
	for (int i = 0; i < s->size; i++)
	{
		if (s->element[i] == t)
		{
			return i;
		}
	}
	if (s->size >= s->max_size)
	{
		if (s->max_size != 0)
		{
			unsigned char *temp_element = malloc(s->max_size * 2);
			memset(temp_element, 0, sizeof(2 * s->max_size));
			memcpy(temp_element, s->element, s->size);
			s->max_size = s->max_size * 2;
			free(s->element);
			s->element = temp_element;
		}
		else
		{
			s->element = malloc(2);
			s->max_size = 2;
		}
	}
	s->element[s->size++] = t;
	return s->size;
}

void clear(set_t *s)
{
	s->size = 0;
	s->max_size = 0;
	free(s->element);
	s->element = (unsigned char *)0;
}

int find(set_t *s, int n)
{
	for (int i = 0; i < s->size; i++)
	{
		if (s->element[i] == n)
		{
			return i;
		}
	}
	return -1;
}

struct PageTable *find_page(PageTable *table, int page_addr)
{
	struct PageTable *temp = NULL;
	HASH_FIND_INT(table, &page_addr, temp);
	if (temp == NULL)
	{
		PageTable *entry = malloc(sizeof(PageTable));
		memset(entry, 0, sizeof(PageTable));
		entry->page_addr = page_addr;
		pthread_mutex_init(&entry->page_desc.owner_set_mutex, NULL);
		insert(&entry->page_desc.owner_set, 0);
		add_page(table, page_addr, entry);
		return entry;
	}
	return temp;
}

void add_page(PageTable *table, int page_addr, struct PageTable *p)
{
	HASH_ADD_INT(table, page_addr, p);
}

struct PageTable_s *find_page_s(PageTable_s *table, int page_addr, int idx)
{
	struct PageTable *temp = NULL;
	HASH_FIND_INT(table, &page_addr, temp);
	if (temp == NULL)
	{
		PageTable *entry = malloc(sizeof(PageTable));
		memset(entry, 0, sizeof(PageTable));
		entry->page_addr = page_addr;
		if (idx == 0)
		{
			entry->page_desc.cur_perm = 2;
		}
		else
		{
			entry->page_desc.cur_perm = 0;
		}
		add_page(table, page_addr, entry);
		return entry;
	}
	return temp;
}

void add_page_s(PageTable_s *table, int page_addr, struct PageTable_s *p)
{
	HASH_ADD_INT(table, page_addr, p);
}