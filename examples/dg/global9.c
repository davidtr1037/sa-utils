#include <stdio.h>
#include <assert.h>

int glob;

int *setglob(void)
{
	glob = 23;
    return NULL;
}

void foo(int *(f)(void), int **result)
{
	*result = f();
}

int main(void)
{
	int *p;
	foo(setglob, &p);
	assert(glob == 23);
	return 0;
}
