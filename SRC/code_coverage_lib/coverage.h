#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h> 

/*Number of counters used for each gcc version*/
#if __GNUC__ == 6
#define GCOV_COUNTERS 10
#elif __GNUC__ == 5 && __GNUC_MINOR__ >= 1
#define GCOV_COUNTERS 10
#elif __GNUC__ == 4 && __GNUC_MINOR__ >= 9
#define GCOV_COUNTERS 9
#else
#define GCOV_COUNTERS   8
#endif

/* For a log2 scale histogram with each range split into 4
   linear sub-ranges, there will be at most 64 (max gcov_type bit size) - 1 log2
   ranges since the lowest 2 log2 values share the lowest 4 linear
   sub-range (values 0 - 3).  This is 252 total entries (63*4).  */
#define GCOV_HISTOGRAM_SIZE 252

/* Used for testing expressions */
#define gcov_nonruntime_assert(EXPR) ((void)(0 && (EXPR)))

/* Optimum number of gcov_unsigned_t's read from or written to disk.  */
#define GCOV_BLOCK_SIZE 1 << 10

/* Realocating memory */
#define xrealloc(V,S) realloc (V,S)
#define XRESIZEVAR(T, P, S) ((T *) xrealloc ((P), (S)))

/* Number of arc counters */
#define GCOV_COUNTER_ARCS   0

/* Counters which can be summaried.  */
#define GCOV_COUNTERS_SUMMABLE  (GCOV_COUNTER_ARCS + 1)

/* Version of gcov.... value is random, not used */
#define GCOV_VERSION ((gcov_unsigned_t)0x89abcdef)

/* Convert a magic or version number to a 4 character string.  */
#define GCOV_UNSIGNED2STRING(ARRAY,VALUE) \
  ((ARRAY)[0] = (char)((VALUE) >> 24),    \
   (ARRAY)[1] = (char)((VALUE) >> 16),    \
   (ARRAY)[2] = (char)((VALUE) >> 8),   \
   (ARRAY)[3] = (char)((VALUE) >> 0))

/* Used for testing given file path */
#define IS_DOS_DIR_SEPARATOR(c) IS_DIR_SEPARATOR_1 (1, c)
#define IS_DIR_SEPARATOR(c) IS_DOS_DIR_SEPARATOR (c)
#define IS_DIR_SEPARATOR_1(dos_based, c)        \
  (((c) == '/')               \
   || (((c) == '\\') && (dos_based)))
#define HAS_DRIVE_SPEC_1(dos_based, f)      \
  ((f)[0] && ((f)[1] == ':') && (dos_based))
#define IS_ABSOLUTE_PATH_1(dos_based, f)     \
  (IS_DIR_SEPARATOR_1 (dos_based, (f)[0])    \
   || HAS_DRIVE_SPEC_1 (dos_based, f))
#define IS_DOS_ABSOLUTE_PATH(f) IS_ABSOLUTE_PATH_1 (1, f)
#define IS_ABSOLUTE_PATH(f) IS_DOS_ABSOLUTE_PATH (f)

/* File magic. Must not be palindromes.  */
#define GCOV_DATA_MAGIC ((gcov_unsigned_t)0x67636461) /* "gcda" */
#define GCOV_NOTE_MAGIC ((gcov_unsigned_t)0x67636e6f) /* "gcno" */

/* The record tags.  Values [1..3f] are for tags which may be in either
   file.  Values [41..9f] for those in the note file and [a1..ff] for
   the data file.  The tag value zero is used as an explicit end of
   file marker -- it is not required to be present.  */
#define GCOV_TAG_FUNCTION  ((gcov_unsigned_t)0x01000000)
#define GCOV_TAG_FUNCTION_LENGTH (3)
#define GCOV_TAG_COUNTER_LENGTH(NUM) ((NUM) * 2)
#define GCOV_TAG_COUNTER_BASE    ((gcov_unsigned_t)0x01a10000)
#define GCOV_TAG_FOR_COUNTER(COUNT)       \
  (GCOV_TAG_COUNTER_BASE + ((gcov_unsigned_t)(COUNT) << 17))
#define GCOV_TAG_SUMMARY_LENGTH(NUM)  \
        (1 + GCOV_COUNTERS_SUMMABLE * (10 + 3 * 2) + (NUM) * 5)
#define GCOV_TAG_OBJECT_SUMMARY  ((gcov_unsigned_t)0xa1000000) /* Obsolete */
#define GCOV_TAG_PROGRAM_SUMMARY ((gcov_unsigned_t)0xa3000000)

/* How many unsigned ints are required to hold a bit vector of non-zero
   histogram entries when the histogram is written to the gcov file.
   This is essentially a ceiling divide by 32 bits.  */
#define GCOV_HISTOGRAM_BITVECTOR_SIZE (GCOV_HISTOGRAM_SIZE + 31) / 32

/* Definition of some types used */
#define HOST_WIDE_INT long
#define CHAR_BIT 8

/*Error handler*/
#define gcov_error(...) fatal_error (input_location, __VA_ARGS__)

/* Definition of some types used */
typedef unsigned gcov_unsigned_t;
typedef int64_t gcov_type;
typedef unsigned gcov_position_t;

/* Type of function used for merging old and new data */
typedef void (*gcov_merge_fn) (gcov_type *, gcov_unsigned_t);

/* Structure used for each bucket of the log2 histogram of counter values.  */
typedef struct
{
  /* Number of counters whose profile count falls within the bucket.  */
  gcov_unsigned_t num_counters;
  /* Smallest profile count included in this bucket.  */
  gcov_type min_value;
  /* Cumulative value of the profile counts in this bucket.  */
  gcov_type cum_value;
} gcov_bucket_type;

/* Cumulative counter data.  */
struct gcov_ctr_summary
{
  gcov_unsigned_t num;    /* number of counters.  */
  gcov_unsigned_t runs;   /* number of program runs */
  gcov_type sum_all;    /* sum of all counters accumulated.  */
  gcov_type run_max;    /* maximum value on a single run.  */
  gcov_type sum_max;      /* sum of individual run max values.  */
  gcov_bucket_type histogram[GCOV_HISTOGRAM_SIZE]; /* histogram of
                                                      counter values.  */
};

/* Object & program summary record.  */
struct gcov_summary
{
  gcov_unsigned_t checksum; /* checksum of program */
  struct gcov_ctr_summary ctrs[GCOV_COUNTERS_SUMMABLE];
};

/* Information about counters for a single function.  */
struct gcov_ctr_info
{
  gcov_unsigned_t num;    /* number of counters.  */
  gcov_type *values;    /* their values.  */
};

/* Information about a single function.  This uses the trailing array
   idiom. The number of counters is determined from the merge pointer
   array in gcov_info.  The key is used to detect which of a set of
   comdat functions was selected -- it points to the gcov_info object
   of the object file containing the selected comdat function.  */
struct gcov_fn_info
{
  const struct gcov_info *key;    /* comdat key */
  gcov_unsigned_t ident;    /* unique ident of function */
  gcov_unsigned_t lineno_checksum;  /* function lineo_checksum */
  gcov_unsigned_t cfg_checksum;   /* function cfg checksum */
  struct gcov_ctr_info ctrs[1];   /* instrumented counters */
};

/* Information about a single object file.  */
struct gcov_info
{
  gcov_unsigned_t version;  /* expected version number */
  struct gcov_info *next; /* link to next, used by libgcov */

  gcov_unsigned_t stamp;  /* uniquifying time stamp */
  const char *filename;   /* output file name */

  gcov_merge_fn merge[GCOV_COUNTERS];  /* merge functions (null for
            unused) */
  
  unsigned n_functions;   /* number of functions */

  const struct gcov_fn_info **functions;
};

/* Variable that contains information about file (pointer, offset, mode ... )*/
struct gcov_var
{
  FILE *file;
  gcov_position_t start;  /* Position of first byte of block */
  unsigned offset;    /* Read/write position within the block.  */
  unsigned length;    /* Read limit in the block.  */
  unsigned overread;    /* Number of words overread.  */
  int error;      /* < 0 overflow, > 0 disk error.  */
  int mode;                 /* < 0 writing, > 0 reading */
#if IN_LIBGCOV
  /* Holds one block plus 4 bytes, thus all coverage reads & writes
     fit within this buffer and we always can transfer GCOV_BLOCK_SIZE
     to and from the disk. libgcov never backtracks and only writes 4
     or 8 byte objects.  */
  gcov_unsigned_t buffer[GCOV_BLOCK_SIZE + 1];
#else
  int endian;     /* Swap endianness.  */
  /* Holds a variable length block, as the compiler can write
     strings and needs to backtrack.  */
  size_t alloc;
  gcov_unsigned_t *buffer;
#endif
} gcov_var;


/*Functions*/


/* Allocates memory needed for gcov_var.buffer */
static void gcov_allocate (unsigned length);

/* Allocate space to write BYTES bytes to the gcov file. Return a
   pointer to those bytes, or NULL on failure.  */
static gcov_unsigned_t *gcov_write_words (unsigned words);

/* Write counter VALUE to coverage file.  Sets error flag
   appropriately.  */
void gcov_write_counter (gcov_type value);

/* Check if VERSION of the info block PTR matches libgcov one.
   Return 1 on success, or zero in case of versions mismatch.
   If FILENAME is not NULL, its value used for reporting purposes
   instead of value from the info block.  */
static int gcov_version (struct gcov_info *ptr, gcov_unsigned_t version, const char *filename);

/* Register a new object file module.  */
void __gcov_init (struct gcov_info *info);

/* The merge function that just sums the counters.  */
void __gcov_merge_add (gcov_type *counters, unsigned n_counters);

/* Save the current position in the gcov file.  */
/* We need to expose this function when compiling for gcov-tool.  */
gcov_position_t gcov_position (void);

/* Write a tag TAG and length LENGTH.  */
void gcov_write_tag_length (gcov_unsigned_t tag, gcov_unsigned_t length);

/* Write unsigned VALUE to coverage file.  Sets error flag
   appropriately.  */
void gcov_write_unsigned (gcov_unsigned_t value);

/* Write a summary structure to the gcov file.  Return nonzero on
   overflow.  */
void gcov_write_summary (gcov_unsigned_t tag, const struct gcov_summary *summary);

/* Write out the current block, if needs be.  */
void gcov_write_block (unsigned size);

/* Move to a given position in a gcov file.  */
void gcov_seek (gcov_position_t base);

/* Return a pointer to read BYTES bytes from the gcov file. Returns
   NULL on failure (read past EOF).  */
static const gcov_unsigned_t * gcov_read_words (unsigned words);

/* Return the right value according to endian */
static inline gcov_unsigned_t from_file (gcov_unsigned_t value);

/* Read unsigned value from a coverage file. Sets error flag on file
   error, overflow flag on overflow */
gcov_unsigned_t gcov_read_unsigned (void);

/* Return nonzero if the error flag is set.  */
/* We need to expose this function when compiling for gcov-tool.  */
int gcov_is_error (void);

/* Move to beginning of file and initialize for writing.  */
void gcov_rewrite (void);

/* Close the current gcov file. Flushes data to disk. Returns nonzero
   on failure or error flag set.  */
int gcov_close (void);

/* Read counter value from a coverage file. Sets error flag on file
   error, overflow flag on overflow */
gcov_type gcov_read_counter (void);

/* Return the number of set bits in X.  */
int popcount_hwi (unsigned HOST_WIDE_INT x);

/* Reads given object or program summary record.  */
void gcov_read_summary (struct gcov_summary *summary);


/* Dump the coverage counts. We merge with existing counts when
   possible, to avoid growing the .da files ad infinitum. We use this
   program's checksum to make sure we only accumulate whole program
   statistics to the correct summary. An object file might be embedded
   in two separate programs, and we must keep the two program
   summaries separate.  */
void drew_coverage();