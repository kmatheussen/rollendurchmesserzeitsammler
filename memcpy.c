
#if 0
#define USE_DYNSIMD
#include "/hom/kjetism/jack/jack/intsimd.h"

void
x86_sse_copyf (float *dest, const float *src, int length)
{
	int i, n1, n2, si3;
	pv4sf m128p_src = (pv4sf) src;
	pv4sf m128p_dest = (pv4sf) dest;

	n1 = (length >> 5);
	n2 = ((length & 0x1f) >> 2);
	si3 = (length & ~0x3);
	for (i = 0; i < n1; i++)
	{
		asm volatile ("movaps %0, %%xmm0\n\t"
			: : "m" (*m128p_src++) : "xmm0", "memory");
		asm volatile ("movaps %0, %%xmm1\n\t"
			: : "m" (*m128p_src++) : "xmm1", "memory");
		asm volatile ("movaps %0, %%xmm2\n\t"
			: : "m" (*m128p_src++) : "xmm2", "memory");
		asm volatile ("movaps %0, %%xmm3\n\t"
			: : "m" (*m128p_src++) : "xmm3", "memory");
		asm volatile ("movaps %0, %%xmm4\n\t"
			: : "m" (*m128p_src++) : "xmm4", "memory");
		asm volatile ("movaps %0, %%xmm5\n\t"
			: : "m" (*m128p_src++) : "xmm5", "memory");
		asm volatile ("movaps %0, %%xmm6\n\t"
			: : "m" (*m128p_src++) : "xmm6", "memory");
		asm volatile ("movaps %0, %%xmm7\n\t"
			: : "m" (*m128p_src++) : "xmm7", "memory");

		asm volatile ("movaps %%xmm0, %0\n\t"
			: "=m" (*m128p_dest++) : : "xmm0", "memory");
		asm volatile ("movaps %%xmm1, %0\n\t"
			: "=m" (*m128p_dest++) : : "xmm1", "memory");
		asm volatile ("movaps %%xmm2, %0\n\t"
			: "=m" (*m128p_dest++) : : "xmm2", "memory");
		asm volatile ("movaps %%xmm3, %0\n\t"
			: "=m" (*m128p_dest++) : : "xmm3", "memory");
		asm volatile ("movaps %%xmm4, %0\n\t"
			: "=m" (*m128p_dest++) : : "xmm4", "memory");
		asm volatile ("movaps %%xmm5, %0\n\t"
			: "=m" (*m128p_dest++) : : "xmm5", "memory");
		asm volatile ("movaps %%xmm6, %0\n\t"
			: "=m" (*m128p_dest++) : : "xmm6", "memory");
		asm volatile ("movaps %%xmm7, %0\n\t"
			: "=m" (*m128p_dest++) : : "xmm7", "memory");
	}
	for (i = 0; i < n2; i++)
	{
		asm volatile (
			"movaps %1, %%xmm0\n\t" \
			"movaps %%xmm0, %0\n\t"
			: "=m" (*m128p_dest++)
			: "m" (*m128p_src++)
			: "xmm0", "memory");
	}
	for (i = si3; i < length; i++)
	{
		asm volatile (
			"movss %1, %%xmm0\n\t" \
			"movss %%xmm0, %0\n\t"
			: "=m" (dest[i])
			: "m" (src[i])
			: "xmm0", "memory");
	}
}

static void sse_memcpy(void *dest,void *src,size_t size){
  x86_sse_copyf(dest,src,size/sizeof(float));
}
#endif


#if 0
// memcpy_sse from http://www.gamedev.net/community/forums/topic.asp?topic_id=474646

#include <xmmintrin.h>
typedef unsigned char uchar;

void* memcpy_sse( void *voidpDest, void * voidpSrc, size_t nBytes )
{
  const uchar* pDest=voidpDest;
  const uchar* pSrc=voidpSrc;

	assert( nBytes >= (15 + 64) );
	void* pDestOrg = voidpDest;

	uint nAlignDest = (16 - (uintptr_t)pDest) & 15;
	memcpy( voidpDest, voidpSrc, nAlignDest );
	pDest += nAlignDest;
	pSrc  += nAlignDest;
	nBytes -= nAlignDest;

	uint nLoops = nBytes >> 6; // no. of loops to copy 64 bytes
	nBytes -= nLoops << 6;
	if( ((uintptr_t)pSrc & 15) == 0 )
	{
	  int i;
		for(i = nLoops; i > 0; --i )
		{
			__m128 tmp0 = _mm_load_ps( (float*)(pSrc + 0 ) );
			__m128 tmp1 = _mm_load_ps( (float*)(pSrc + 16) );
			__m128 tmp2 = _mm_load_ps( (float*)(pSrc + 32) );
			__m128 tmp3 = _mm_load_ps( (float*)(pSrc + 48) );
			_mm_store_ps( (float*)(pDest + 0 ), tmp0 );
			_mm_store_ps( (float*)(pDest + 16), tmp1 );
			_mm_store_ps( (float*)(pDest + 32), tmp2 );
			_mm_store_ps( (float*)(pDest + 48), tmp3 );
			pSrc  += 64;
			pDest += 64;
		}
	}
	else
	{
	  int i;
		for(i = nLoops; i > 0; --i )
		{
			__m128 tmp0 = _mm_loadu_ps( (float*)(pSrc + 0 ) );
			__m128 tmp1 = _mm_loadu_ps( (float*)(pSrc + 16) );
			__m128 tmp2 = _mm_loadu_ps( (float*)(pSrc + 32) );
			__m128 tmp3 = _mm_loadu_ps( (float*)(pSrc + 48) );
			_mm_store_ps( (float*)(pDest + 0 ), tmp0 );
			_mm_store_ps( (float*)(pDest + 16), tmp1 );
			_mm_store_ps( (float*)(pDest + 32), tmp2 );
			_mm_store_ps( (float*)(pDest + 48), tmp3 );
			pSrc  += 64;
			pDest += 64;
		}
	}
	memcpy( (char*)pDest, pSrc, nBytes );
	return pDestOrg;
}
#endif

static void *memcpy3(void * b, const void * a, size_t n){
    char *s1 = b;
    const char *s2 = a;
    for(; 0<n; --n)*s1++ = *s2++;
    return b;
}

static void *memcpy4(void * b, const void * a, size_t n){
    int *s1 = b;
    const int *s2 = a;
    int len=n/sizeof(int);
    for(; 0<len; --len)
      *s1++ = *s2++;
    return b;
}

static void *memcpy5(void * b, const void * a, size_t n){
  double *s1 = b;
  const double *s2 = a;
  int len=n/sizeof(double);
  for(; 0<len; --len)
    *s1++ = *s2++;
  return b;
}

static void memcpy6(int * __restrict dest, const int  * __restrict src, size_t size){
  int i;
  const int len=size/sizeof(int);
  for(i=0;i<len;i++){
    dest[i]=src[i];
  }
}

static void *memcpy7(void * __restrict b, const void * __restrict a, size_t n){
  long double *s1 = b;
  const long double *s2 = a;
  int len=n/sizeof(long double);
  for(; 0<len; --len)
    *s1++ = *s2++;
  return b;
}

// http://software.intel.com/en-us/articles/memcpy-performance/
static void *memcpy8(void * __restrict b, const void * __restrict a, size_t n){
    char *s1 = b;
    const char *s2 = a;
    for(; 0<n; --n)*s1++ = *s2++;
    return b;
}

static void *memcpy9(void * __restrict b, const void * __restrict a, size_t n){
  double *s1 = b;
  const double *s2 = a;
  int len=n/sizeof(double);
  for(; 0<len; --len)
    *s1++ = *s2++;
  return b;
}

static void *memcpy10(void * __restrict b, const void * __restrict a, size_t n){
  int *s1 = b;
  const int *s2 = a;
  int len=n/sizeof(int);
  for(; 0<len; --len)
    *s1++ = *s2++;
  return b;
}

// from memtest source
static void *memcpy11(void * __restrict b, const void * __restrict a, size_t n){
  ulong dst;
  ulong wlen;

  int   len = n;
  ulong src = (ulong)b;

  dst = src + len;
  wlen = len / 4;  /* Length is bytes */

  fprintf(stderr,"memcpy11. src: %d, dst: %d, len: %d\n",src,dst,len);

  asm __volatile__ (
                    "movl %0,%%esi\n\t"             \
                    "movl %1,%%edi\n\t"             \
                    "movl %2,%%ecx\n\t"             \
                    "cld\n\t"                       \
                    "rep\n\t"                                           \
                    "movsl\n\t"                                         \
                    :: "g" (src), "g" (dst), "g" (wlen)
                    : "esi", "edi", "ecx"
                    );
  return b;
}

#if 0
#define LITTLE_ENDIAN 1
#define INDEXED_COPY 1
//#define MEMCPY_64BIT 1
#include "temp/memcpy.c"
#endif

