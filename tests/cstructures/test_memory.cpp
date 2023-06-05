#include "gmock/gmock.h"
#include "cstructures/memory.h"

#define NAME memory

using namespace testing;

TEST(NAME, malloc_free)
{
	void* p = MALLOC(16);
	EXPECT_THAT(p, NotNull());
	FREE(p);
}

TEST(NAME, realloc_free)
{
	void* p = REALLOC(NULL, 16);
	EXPECT_THAT(p, NotNull());
	FREE(p);
}

TEST(NAME, realloc_realloc_free)
{
	void* p = REALLOC(NULL, 2);
	EXPECT_THAT(p, NotNull());
	p = REALLOC(p, 4);
	EXPECT_THAT(p, NotNull());
	FREE(p);
}
