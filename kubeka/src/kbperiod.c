#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>

#include "kbutil.h"
#include "kbperiod.h"

enum period_unit_t {
   unit_UNKNOWN = 0,
   unit_SEC = 1,
   unit_MIN = 60,
   unit_HOUR = 60 * 60,
   unit_DAY = 24 * 60 * 60,
};

static enum period_unit_t parse_unit (const char *src)
{
   if (src[1] == 0) {
      return strchr ("smhd", src[0]) ? true : false;
   }

   static const struct {
      const char *suffix;
      enum period_unit_t unit;
   } suffixes[] = {
      { "sec",       unit_SEC    },
      { "secs",      unit_SEC    },
      { "second",    unit_SEC    },
      { "seconds",   unit_SEC    },
      { "min",       unit_MIN    },
      { "mins",      unit_MIN    },
      { "minute",    unit_MIN    },
      { "minutes",   unit_MIN    },
      { "hr",        unit_HOUR   },
      { "hrs",       unit_HOUR   },
      { "hour",      unit_HOUR   },
      { "hours",     unit_HOUR   },
      { "day",       unit_DAY    },
      { "days",      unit_DAY    },
   };

   for (size_t i=0; i<sizeof suffixes/sizeof suffixes[0]; i++) {
      if ((strcmp (src, suffixes[i].suffix)) == 0) {
         return suffixes[i].unit;
      }
   }

   return unit_UNKNOWN;
}

struct kbperiod_t {
   enum period_unit_t unit;
   uint32_t nunits;
   uint64_t exp_time;
};

kbperiod_t *kbperiod_parse (const char *src)
{
   kbperiod_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      KBIERROR ("OOM allocating period struct\n");
      return NULL;
   }

   const char *unit = src;
   size_t ndigits = 0;
   while ((isdigit (*unit))) {
      unit++;
      ndigits++;
   }

   if (!ndigits) {
      KBXERROR ("No digits found in value $<PERIODIC>: [%s]\n", src);
      free (ret);
      return NULL;
   }

   if ((ret->unit = parse_unit (unit)) == unit_UNKNOWN) {
      KBXERROR ("Unrecognised PERIODIC unit: [%s]\n", unit);
      free (ret);
      return NULL;
   }

   if ((sscanf (src, "%" PRIu32, &ret->nunits)) != 1) {
      KBXERROR ("Failed to scan number from PERIODIC value [%s]\n", src);
      free (ret);
      return NULL;
   }

   uint64_t now = (uint64_t)time (NULL);
   ret->exp_time = now + (ret->unit * ret->nunits);

   return ret;
}

void kbperiod_del (kbperiod_t *kbp)
{
   if (kbp) {
      free (kbp);
   }
}


uint64_t kbperiod_remaining (kbperiod_t *kbp)
{
   if (!kbp) {
      return 0;
   }
   uint64_t now = (uint64_t)time (NULL);
   if (kbp->exp_time > now) {
      return 0;
   }

   return now - kbp->exp_time;
}

void kbperiod_reset (kbperiod_t *kbp)
{
   if (!kbp)
      return;

   uint64_t now = (uint64_t)time (NULL);
   kbp->exp_time = now + (kbp->unit * kbp->nunits);
}

