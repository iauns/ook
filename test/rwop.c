#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "ook.h"

static struct ookfile* zeroes;
static const char* simplefile = ".testing";
static struct ookfile* of;

static void
print_arr(const uint32_t* d, size_t n)
{
  printf("{ ");
  for(size_t i=0; i < n; ++i) {
    printf("%u ", d[i]);
  }
  printf("}\n");
}

void
print_slice(const uint32_t* d, size_t slice, const size_t nx, const size_t ny)
{
  const size_t z = slice;
  for(size_t y=0; y < ny; ++y) {
    const size_t coord = z*ny*nx + y*nx;
    print_arr(d+coord, nx);
  }
}

static uint32_t
value(size_t x, size_t y, size_t z)
{
  return 25700U + z/16 + (y*(16/2)) + x;
}

static void
setup_simple()
{
  FILE* fp = fopen(simplefile, "w+b");
  ck_assert_int_ne(fp, NULL);
  uint32_t data[16];
  for(size_t z=0; z < 16; ++z) {
    for(size_t y=0; y < 16; ++y) {
      for(size_t x=0; x < 16; ++x) {
        data[x] = value(x,y,z);
      }
      fwrite(data, sizeof(uint32_t), 16, fp);
    }
  }
  fclose(fp);
  ck_assert(ookinit(StdCIO));

  const uint64_t sz[3] = { 16, 16, 16 };
  const size_t bsize[3] = { 8, 8, 16 };
  of = ookread(simplefile, sz, bsize, OOK_U32, 1);
  ck_assert_int_ne(of, NULL);
}

static void
teardown_simple()
{
  ck_assert(ookclose(of) == 0);
  of = NULL; /* force memory being unreachable (for leak checking) */
}

static void
setup_zero()
{
  ck_assert(ookinit(StdCIO));
  const uint64_t sz[3] = { 32, 32, 32 };
  const size_t bsize[3] = { 16, 16, 32 };
  zeroes = ookread("/dev/zero", sz, bsize, OOK_U16, 1);
  ck_assert_int_ne(zeroes, NULL);
}

static void
teardown_zero()
{
  ck_assert(ookclose(zeroes) == 0);
  zeroes = NULL; /* force memory being unreachable (for leak checking) */
}

START_TEST(zero_simple)
{
  size_t bsize[3];
  ookmaxbricksize(zeroes, bsize);
  const size_t components = 1;
  const size_t bytes_brick = sizeof(uint16_t) * components *
                             bsize[0]*bsize[1]*bsize[2];
  void* data = malloc(bytes_brick);
  ookbrick(zeroes, 0, data);
  free(data);
}
END_TEST

/* tests reading all bricks from a dataset.  we should always get zeroes from
 * this dataset, so we verify that all the data are 0 after the read. */
START_TEST(zero_all_bricks)
{
  size_t bsize[3];
  ookmaxbricksize(zeroes, bsize);

  const size_t components = 1;
  const size_t bytes_brick = sizeof(uint16_t) * components *
                             bsize[0]*bsize[1]*bsize[2];
  void* data = malloc(bytes_brick);

  memset(data, 1, bytes_brick);

  ck_assert_int_eq(ookbricks(zeroes), 4U);

  for(size_t i=0; i < ookbricks(zeroes); ++i) {
		memset(data, 1, bytes_brick); /* force the read to overwrite data */
    ookbrick(zeroes, i, data);
    /* verify every element is 0. */
    for(size_t j=0; j < bsize[0]*bsize[1]*bsize[2]; ++j) {
      ck_assert_int_eq(*((uint16_t*)data), 0U);
    }
  }
  free(data);
}
END_TEST

START_TEST(zero_rw)
{
  size_t bsize[3];
  ookmaxbricksize(zeroes, bsize);

  const size_t components = 1;
  const size_t bytes_brick = sizeof(uint16_t) * components *
                             bsize[0]*bsize[1]*bsize[2];
  void* data = malloc(bytes_brick);
  ookbrick(zeroes, 0, data);
  ookwrite(zeroes, 1, data);
  free(data);
}
END_TEST

START_TEST(simple_verify)
{
  size_t bsize[3];
  ookmaxbricksize(of, bsize);
  /* bricksize is stored correctly? */
  ck_assert_int_eq(bsize[0], 8U);
  ck_assert_int_eq(bsize[1], 8U);
  ck_assert_int_eq(bsize[2], 16U);

  const size_t components = 1;
  const size_t bytes_brick = sizeof(uint32_t) * components *
                             bsize[0]*bsize[1]*bsize[2];
  uint32_t* data = malloc(bytes_brick);
  memset(data, 0, bytes_brick);
  ookbrick(of, 0, data);
  for(size_t z=0; z < bsize[2]; ++z) {
    for(size_t y=0; y < bsize[1]; ++y) {
      for(size_t x=0; x < bsize[0]; ++x) {
        ck_assert_int_eq(
          data[z*bsize[1]*bsize[0] + y*bsize[0] + x],
          value(x,y,z)
        );
      }
    }
  }
}
END_TEST

Suite*
rwop_suite()
{
  Suite* s = suite_create("rwop");
  TCase* zero = tcase_create("zero");
  tcase_add_test(zero, zero_simple);
  tcase_add_test(zero, zero_all_bricks);
  tcase_add_test(zero, zero_rw);
  TCase* simple = tcase_create("simple");
  tcase_add_test(simple, simple_verify);
  tcase_add_checked_fixture(zero, setup_zero, teardown_zero);
  tcase_add_checked_fixture(simple, setup_simple, teardown_simple);
  suite_add_tcase(s, zero);
  suite_add_tcase(s, simple);
  return s;
}
