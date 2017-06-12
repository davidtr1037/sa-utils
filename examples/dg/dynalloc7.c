#include <stdlib.h>
#include <assert.h>

void foo(int **result)
{
	int *i = malloc(sizeof *i);
    *result = i;
}

int main(int argc, char *argv[], char *envp[])
{
	int *a = NULL;
    foo(&a);
	*a = 3;
	assert(*a == 3);

	return 0;
}
