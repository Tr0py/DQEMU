#ifndef SET_H
#define SET_H



struct set
{
	unsigned element[32];
	unsigned size;
};


typedef struct set set_t;


int insert(set_t *s, int t);

void clear(set_t *s);

int find(set_t *s, int n);

#endif 
