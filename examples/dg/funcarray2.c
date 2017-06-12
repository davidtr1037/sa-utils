#include <assert.h>

int glob;

void setglob(void)
{
	glob = 8;
}

void (*funcarray[10])(void) = {setglob};

void call(void (**funcarray)(void), int idx)
{
	funcarray[idx]();
}

int main(int argc, char *argv[], char *envp[])
{
	call(funcarray, 0);
	assert(glob == 8);
	return 0;
}
