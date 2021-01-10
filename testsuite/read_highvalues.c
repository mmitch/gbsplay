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

	gbs = gbs_open("highvalues.gbs");
	ASSERT(gbs != NULL);

	str = gbs_get_title(gbs);
	ASSERT_EQUAL_STRING(str, "SONGTITLEsongtitleSONGTITLEsongt");
	
	str = gbs_get_author(gbs);
	ASSERT_EQUAL_STRING(str, "AUTHORauthorAUTHORauthorAUTHORau");
	
	str = gbs_get_copyright(gbs);
	ASSERT_EQUAL_STRING(str, "COPYRIGHTcopyrightCOPYRIGHTcopyr");

	status = gbs_get_status(gbs);

	ASSERT(status != NULL);
	ASSERT_EQUAL_LONG(status->songs, 255);
	ASSERT_EQUAL_LONG(status->defaultsong, 255);
	ASSERT_EQUAL_LONG(status->subsong, 0);

	ASSERT_EQUAL_LONG(status->ch[0].mute, 0);
	ASSERT_EQUAL_LONG(status->ch[1].mute, 0);
	ASSERT_EQUAL_LONG(status->ch[2].mute, 0);
	ASSERT_EQUAL_LONG(status->ch[3].mute, 0);

	// FIXME: inconsistent use of 1 based and 0 based indices in API
	gbs_init(gbs, status->defaultsong - 1);
	ASSERT_EQUAL_LONG(status->subsong, 254);

	gbs_close(gbs);

	return 0;
}
