#include <stdlib.h>

#include "../libgbs.h"
#include "assert.h"

int main(int argc, char **argv)
{
	struct gbs *gbs;
	const struct gbs_status *status;
	const char *str;

	UNUSED(argv);

	ASSERT(argc == 1);

	gbs = gbs_open("lowvalues.gbs");
	ASSERT(gbs != 0);

	str = gbs_get_title(gbs);
	ASSERT_EQUAL_STRING(str, "");
	
	str = gbs_get_author(gbs);
	ASSERT_EQUAL_STRING(str, "");
	
	str = gbs_get_copyright(gbs);
	ASSERT_EQUAL_STRING(str, "");

	gbs_init(gbs, 0);

	status = gbs_get_status(gbs);
	ASSERT(status != NULL);
	ASSERT_EQUAL_INT(status->songs, 1);
	ASSERT_EQUAL_INT(status->defaultsong, 1);
	ASSERT_EQUAL_INT(status->subsong, 0);

	ASSERT_EQUAL_LONG(status->ch[0].mute, 0);
	ASSERT_EQUAL_LONG(status->ch[1].mute, 0);
	ASSERT_EQUAL_LONG(status->ch[2].mute, 0);
	ASSERT_EQUAL_LONG(status->ch[3].mute, 0);

	gbs_close(gbs);

	return 0;
}
