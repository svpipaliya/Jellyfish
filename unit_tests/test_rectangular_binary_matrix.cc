#include <gtest/gtest.h>
#include <unit_tests/test_main.hpp>
#include <stdexcept>
#include <stdlib.h>
#include <jellyfish/rectangular_binary_matrix.hpp>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_INT128
#include <jellyfish/int128.hpp>
#endif

namespace {
using jellyfish::RectangularBinaryMatrix;

static bool allocate_matrix(unsigned int r, unsigned c) {
  RectangularBinaryMatrix m(r, c);
  return m.is_zero();
}

TEST(RectangularBinaryMatrix, InitSizes) {
  RectangularBinaryMatrix m(5, 60);

  EXPECT_EQ(5u, m.r());
  EXPECT_EQ(60u, m.c());
  EXPECT_TRUE(m.is_zero());

  EXPECT_THROW(allocate_matrix(100, 100) == true, std::out_of_range);
  EXPECT_THROW(allocate_matrix(0, 100) == true, std::out_of_range);
  EXPECT_THROW(allocate_matrix(10, 0) == true, std::out_of_range);
  EXPECT_THROW(allocate_matrix(10, 6) == true, std::out_of_range);
}

TEST(RectangularBinaryMatrix, Copy) {
  RectangularBinaryMatrix m1(5, 55);
  m1.randomize(random_bits);

  RectangularBinaryMatrix m2(m1);
  RectangularBinaryMatrix m3(6, 66);
  RectangularBinaryMatrix m4(5, 55);

  EXPECT_TRUE(!m1.is_zero());
  EXPECT_TRUE(m1 == m2);
  EXPECT_TRUE(!(m1 == m3));
  EXPECT_TRUE(!(m1 == m4));
  m4 = m1;
  EXPECT_TRUE(m1 == m4);
}

TEST(RectangularBinaryMatrix, InitRaw) {
  const unsigned int nb_col = 80;
  uint64_t raw[nb_col];
  for(unsigned int i = 0; i < nb_col; ++i)
    raw[i] = random_bits();
  const RectangularBinaryMatrix m(raw, 19, nb_col);
  EXPECT_EQ(19u, m.r());
  EXPECT_EQ(80u, m.c());
  const uint64_t mask = ((uint64_t)1 << 19) - 1;
  for(unsigned int i = 0; i < nb_col; ++i)
    EXPECT_EQ(raw[i] & mask, m[i]);
}

TEST(RectangularBinaryMatrix, LowIdentity) {
  RectangularBinaryMatrix m(30, 100);
  EXPECT_TRUE(!m.is_low_identity());

  m.init_low_identity();
  EXPECT_EQ((uint64_t)1, m[m.c() - 1]);
  for(unsigned int i = m.c() - 1; i > m.c() - m.r(); --i)
    EXPECT_EQ(m[i] << 1, m[i-1]);
  for(unsigned int i = 0; i < m.c() - m.r(); ++i)
    EXPECT_EQ((uint64_t)0, m[i]);
  EXPECT_TRUE(m.is_low_identity());

  m.randomize(random_bits);
  EXPECT_TRUE(!m.is_low_identity()); // This could fail with some VERY low probability
}

/******************************
 * Matrix Vector product
 ******************************/
class MatrixVectorProd : public ::testing::Test {
public:
  RectangularBinaryMatrix mo, me, mw, mf;
  MatrixVectorProd() : mo(51, 101), me(50, 100), mw(30, 64), mf(64,64) {
    mo.randomize(random_bits);
    me.randomize(random_bits);
    mw.randomize(random_bits);
    mf.randomize(random_bits);
  }
};

TEST_F(MatrixVectorProd, Checks) {
  EXPECT_EQ((unsigned int)51, mo.r());
  EXPECT_EQ((unsigned int)101, mo.c());
  EXPECT_EQ((unsigned int)50, me.r());
  EXPECT_EQ((unsigned int)100, me.c());
  EXPECT_EQ((unsigned int)30, mw.r());
  EXPECT_EQ((unsigned int)64, mw.c());
  EXPECT_EQ((unsigned int)64, mf.r());
  EXPECT_EQ((unsigned int)64, mf.c());

  EXPECT_FALSE(mo.is_zero());
  EXPECT_FALSE(me.is_zero());
  EXPECT_FALSE(mw.is_zero());
  EXPECT_FALSE(mf.is_zero());
}

TEST_F(MatrixVectorProd, AllOnes) {
  uint64_t v[2], res = 0;
  v[0] = v[1] = (uint64_t)-1;
  for(unsigned int i = 0; i < mo.c(); ++i)
    res ^= mo[i];
  EXPECT_EQ(res, mo.times_loop(v));
  res = 0;
  for(unsigned int i = 0; i < me.c(); ++i)
    res ^= me[i];
  EXPECT_EQ(res, me.times_loop(v));
  res = 0;
  for(unsigned int i = 0; i < mw.c(); ++i)
    res ^= mw[i];
  EXPECT_EQ(res, mw.times_loop(v));
}
TEST_F(MatrixVectorProd, EveryOtherOnes) {
  uint64_t v[2], res = 0;
  v[0] = 0xaaaaaaaaaaaaaaaaUL;
  v[1] = 0xaaaaaaaaaaaaaaaaUL;
  for(unsigned int i = 1; i < mo.c(); i += 2)
    res ^= mo[i];
  EXPECT_EQ(res, mo.times_loop(v));
  res = 0;
  for(unsigned int i = 0; i < me.c(); i += 2)
    res ^= me[i];
  EXPECT_EQ(res, me.times_loop(v));
}

#if HAVE_SSE || HAVE_INT128
TEST_F(MatrixVectorProd, Optimizations) {
  static const int nb_tests = 100;
  for(int i = 0; i < nb_tests; ++i) {
    unsigned int r = 2 * (random() % 31 + 1);
    unsigned int c = 2 * (random() % 100) + r;

    RectangularBinaryMatrix m(r, c);
    m.randomize(random_bits);

    const unsigned int nb_words = c / 64 + (c % 64 != 0);
    uint64_t v[nb_words];
    for(unsigned int j = 0; j < nb_words; ++j)
      v[j] = random_bits();

    uint64_t res = m.times_loop((uint64_t*)v);
#ifdef HAVE_SSE
    EXPECT_EQ(res, m.times_sse((uint64_t*)v));
#endif
#ifdef HAVE_INT128
    EXPECT_EQ(res, m.times_128((uint64_t*)v));
#endif
  }
}
#endif // HAVE_SSE || HAVE_INT128

/******************************
 * Matrix product and inverse
 ******************************/
TEST(PseudoProduct, Dimensions) {
  RectangularBinaryMatrix m(30, 100), m1(32, 100), m2(30, 98);

  EXPECT_THROW(m.pseudo_multiplication(m1), std::domain_error);
  EXPECT_THROW(m.pseudo_multiplication(m2), std::domain_error);
}

TEST(PseudoProduct, Identity) {
  RectangularBinaryMatrix m(30, 100), i(30, 100);
  i.init_low_identity();
  m.randomize(random_bits);

  EXPECT_TRUE(i.pseudo_multiplication(m) == m);
}

TEST(PseudoProduct, Parity) {
  const unsigned int col_sizes[6] = { 50, 70, 126, 130, 64, 128 };
  const unsigned int nb_rows = 30;

  for(unsigned int k = 0; k < sizeof(col_sizes) / sizeof(unsigned int); ++k) {
    const unsigned int nb_cols = col_sizes[k];
    uint64_t *cols = new uint64_t[nb_cols];
    RectangularBinaryMatrix p(nb_rows, nb_cols);

    for(unsigned int j = 18; j < 19; ++j) {
      const uint64_t bits = ((uint64_t)1 << j) - 1;
      unsigned int i;
      for(i = 0; i < nb_cols; ++i)
        cols[i] = bits;
      RectangularBinaryMatrix m(cols, nb_rows, nb_cols);

      p = m.pseudo_multiplication(m);
      for(i = 0; i < nb_cols - nb_rows; ++i)
        EXPECT_EQ(__builtin_parity(bits) ? (uint64_t)0 : bits, p[i]);
      for( ; i < nb_cols; ++i)
        EXPECT_EQ(__builtin_parity(bits) ? bits : (uint64_t)0, p[i]);
    }
    delete [] cols;
  }
}

TEST(PseudoProduct, Inverse) {
  int full_rank = 0, singular = 0;
  for(unsigned int i = 0; i < 500; ++i) {
    unsigned int r = 2 * (random() % 31 + 1);
    unsigned int c = 2 * (random() % 100) + r;
    RectangularBinaryMatrix m(r, c);
    m.randomize(random_bits);
    RectangularBinaryMatrix s(m);
    unsigned int rank = m.pseudo_rank();
    if(rank != c) {
      ++singular;
      EXPECT_THROW(m.pseudo_inverse(), std::domain_error);
    } else {
      ++full_rank;
      RectangularBinaryMatrix inv = m.pseudo_inverse();
      RectangularBinaryMatrix i = inv.pseudo_multiplication(m);
      EXPECT_TRUE(i.is_low_identity());
    }
    EXPECT_TRUE(s == m);
  }
  EXPECT_EQ(500, full_rank + singular);
  EXPECT_NE(0, full_rank);
}

TEST(PseudoProduct, Rank) {
  RectangularBinaryMatrix m(50, 100);
  for(unsigned int i = 0; i < 10; ++i) {
    m.randomize(random_bits);
    RectangularBinaryMatrix s(m);
    unsigned int rank = m.pseudo_rank();
    EXPECT_TRUE(rank >= 0 && rank <= m.c());
    EXPECT_TRUE(s == m);
  }
}

TEST(PseudoProduct, InitRandom) {
  RectangularBinaryMatrix m(50, 100);
  for(unsigned int i = 0; i < 10; ++i) {
    RectangularBinaryMatrix im(m.randomize_pseudo_inverse(random_bits));
    EXPECT_EQ((unsigned int)m.c(), m.pseudo_rank());
    EXPECT_EQ((unsigned int)m.c(), im.pseudo_rank());
    EXPECT_TRUE((m.pseudo_multiplication(im)).is_low_identity());
  }
}

static const int speed_loop = 100000000;
TEST(MatrixProductSpeed, Loop) {
  RectangularBinaryMatrix m(50, 100);
  const unsigned int nb_words = m.c() / 64 + (m.c() % 64 != 0);
  uint64_t v[nb_words];
  for(unsigned int j = 0; j < nb_words; ++j)
    v[j] = random_bits();

  uint64_t res = 0;
  for(int i = 0; i < speed_loop; ++i)
    res ^= m.times_loop((uint64_t*)v);
}

#ifdef HAVE_SSE
TEST(MatrixProductSpeed, SSE) {
  RectangularBinaryMatrix m(50, 100);
  const unsigned int nb_words = m.c() / 64 + (m.c() % 64 != 0);
  uint64_t v[nb_words];
  for(unsigned int j = 0; j < nb_words; ++j)
    v[j] = random_bits();

  uint64_t res = 0;
  for(int i = 0; i < speed_loop; ++i)
    res ^= m.times_sse((uint64_t*)v);
}
#endif

#ifdef HAVE_INT128
TEST(MatrixProductSpeed, U128) {
  RectangularBinaryMatrix m(50, 100);
  const unsigned int nb_words = m.c() / 64 + (m.c() % 64 != 0);
  uint64_t v[nb_words];
  for(unsigned int j = 0; j < nb_words; ++j)
    v[j] = random_bits();

  uint64_t res = 0;
  for(int i = 0; i < speed_loop; ++i)
    res ^= m.times_128((uint64_t*)v);
}
#endif

}
