
// Code copied from jack and slightly modified. -Kjetil.

#ifndef _rtgc_atomicity_atomicity_h_
#define _rtgc_atomicity_atomicity_h_

#if defined(__i386__)

#include "atomicity_i386.h"

#elif defined(__x86_64)

/* x86_64 can use rdtsc just like i[456]86 */

#include "atomicity_i386.h"

#elif defined(__powerpc__) || defined(__ppc__) /* linux and OSX use different tokens */

#include "atomicity_powerpc.h"

#else

#error "This platform has not been configured by rtgc for supporting atomicity."

//#include <config/cpu/generic/atomicity.h>

#endif /* processor selection */

#endif /* _rtgc_atomicity_atomicity_h_ */
