#include "coverage.h"

/* Checksum of a program */
static gcov_unsigned_t gcov_crc32;

/* Length of gcda or gcno file name, it helps allocation */
static size_t gcov_max_filename = 0;

/* List of gcov_info structures that contain information about a single object file. */
static struct gcov_info *gcov_list;

/* Version of gcov... it is written in gcda and gcno files */
static gcov_unsigned_t version_gcov;

/* Number of words for writing in gcda */
static unsigned word_number = 0;

/* Allocates memory needed for gcov_var.buffer */
static void gcov_allocate (unsigned length) 
{
  /* Get how much is already allocated */
  size_t new_size = gcov_var.alloc;

  if (!new_size)
    /* If nothing has been  allocated before just allocate optimal number for start */
    new_size = GCOV_BLOCK_SIZE;

  /* Now calculate new size */
  new_size += length;
  new_size *= 2;
  gcov_var.alloc = new_size;

  /* Resize gcov_var.buffer to new_size */
  gcov_var.buffer = XRESIZEVAR (gcov_unsigned_t, gcov_var.buffer, new_size << 2);
}

/* Allocate space to write BYTES bytes to the gcov file. Return a
   pointer to those bytes, or NULL on failure.  */
static gcov_unsigned_t *
gcov_write_words (unsigned words)
{
  /* Increase number of words that should be written*/
	word_number += words;

  	gcov_unsigned_t *result;
	gcov_nonruntime_assert (gcov_var.mode < 0);

  /* If not allocated enough... */
  if (gcov_var.offset + words > gcov_var.alloc)
    gcov_allocate (gcov_var.offset + words);

  /* Gcov_var.buffer contains everything that should be in gcda file*/
  result = &gcov_var.buffer[gcov_var.offset];

  /* POsition on the proper place in file */
  gcov_var.offset += words;

  return result;
}
/* Write counter VALUE to coverage file.  Sets error flag
   appropriately.  */
void gcov_write_counter (gcov_type value)
{
  /* Counter is a 64bit number, so it is presented by two words */
  gcov_unsigned_t *buffer = gcov_write_words (2);

  /* Lower 32bits */
  buffer[0] = (gcov_unsigned_t) value;

  if (sizeof (value) > sizeof (gcov_unsigned_t))
    /* If it is bigger than 32bit number then: set upper 32bits */
    buffer[1] = (gcov_unsigned_t) (value >> 32);
  else
    /* Else just set 0 on every upper bit */
    buffer[1] = 0;
}

/* Check if VERSION of the info block PTR matches libgcov one.
   Return 1 on success, or zero in case of versions mismatch.
   If FILENAME is not NULL, its value used for reporting purposes
   instead of value from the info block.  */
static int gcov_version (struct gcov_info *ptr, gcov_unsigned_t version, const char *filename)
{
  if (version != GCOV_VERSION)
    {
      char v[4], e[4];
      GCOV_UNSIGNED2STRING (v, version);
      GCOV_UNSIGNED2STRING (e, GCOV_VERSION);      
      fprintf (stderr,
         "profiling:%s:Version mismatch - expected %.4s got %.4s\n",
         filename? filename : ptr->filename, e, v);
      return 0;
    }

  return 1;
}

/* Register a new object file module.  */
void __gcov_init (struct gcov_info *info)
{
  /* If info-> version is set on 0 that means we have allready done this so we do anything */
  if (!info->version) 
    return;

  /* Check version*/
  if (gcov_version (info, GCOV_VERSION , 0))
    {
  	const char *ptr = info->filename;
    gcov_unsigned_t crc32 = gcov_crc32;
    size_t filename_length =  strlen(info->filename);

    /* Refresh the longest file name information */
	if (filename_length > gcov_max_filename)
        gcov_max_filename = filename_length;
      
    do
    	{
    	unsigned ix;
    	gcov_unsigned_t value = *ptr << 24;

    	for (ix = 8; ix--; value <<= 1)
      		{
        	gcov_unsigned_t feedback;
        	feedback = (value ^ crc32) & 0x80000000 ? 0x04c11db7 : 0;
        	crc32 <<= 1;
        	crc32 ^= feedback;
      		}
  		}
      while (*ptr++);
      
      gcov_crc32 = crc32;
      info->next = gcov_list;
      gcov_list = info;
    }
    version_gcov = info->version;

    /* Set version on 0 so we dont enter here twice */
  	info->version = 0;
}

/* Write a tag TAG and length LENGTH.  */
void gcov_write_tag_length (gcov_unsigned_t tag, gcov_unsigned_t length)
{
  gcov_unsigned_t *buffer = gcov_write_words (2);
  buffer[0] = tag;
  buffer[1] = length;
}

/* Write unsigned VALUE to coverage file.  Sets error flag
   appropriately.  */
void gcov_write_unsigned (gcov_unsigned_t value)
{
  gcov_unsigned_t *buffer = gcov_write_words (1);
  buffer[0] = value;
}

/* Write a summary structure to the gcov file.  Return nonzero on
   overflow.  */
void gcov_write_summary (gcov_unsigned_t tag, const struct gcov_summary *summary)
{
  unsigned ix, h_ix, bv_ix, h_cnt = 0;
  const struct gcov_ctr_summary *csum;
  unsigned histo_bitvector[GCOV_HISTOGRAM_BITVECTOR_SIZE];

  /* Count number of non-zero histogram entries, and fill in a bit vector
     of non-zero indices. The histogram is only currently computed for arc
     counters.  */
  for (bv_ix = 0; bv_ix < GCOV_HISTOGRAM_BITVECTOR_SIZE; bv_ix++)
    histo_bitvector[bv_ix] = 0;
  csum = &summary->ctrs[GCOV_COUNTER_ARCS];
  for (h_ix = 0; h_ix < GCOV_HISTOGRAM_SIZE; h_ix++)
    {
      if (csum->histogram[h_ix].num_counters > 0)
        {
          histo_bitvector[h_ix / 32] |= 1 << (h_ix % 32);
          h_cnt++;
        }
    }
  gcov_write_tag_length (tag, GCOV_TAG_SUMMARY_LENGTH (h_cnt));
  gcov_write_unsigned (summary->checksum);
  for (csum = summary->ctrs, ix = GCOV_COUNTERS_SUMMABLE; ix--; csum++)
    {
      gcov_write_unsigned (csum->num);
      gcov_write_unsigned (csum->runs);
      gcov_write_counter (csum->sum_all);
      gcov_write_counter (csum->run_max);
      gcov_write_counter (csum->sum_max);
      if (ix != GCOV_COUNTER_ARCS)
        {
          for (bv_ix = 0; bv_ix < GCOV_HISTOGRAM_BITVECTOR_SIZE; bv_ix++)
            gcov_write_unsigned (0);
          continue;
        }
      for (bv_ix = 0; bv_ix < GCOV_HISTOGRAM_BITVECTOR_SIZE; bv_ix++)
        gcov_write_unsigned (histo_bitvector[bv_ix]);
      for (h_ix = 0; h_ix < GCOV_HISTOGRAM_SIZE; h_ix++)
        {
          if (!csum->histogram[h_ix].num_counters)
            continue;
          gcov_write_unsigned (csum->histogram[h_ix].num_counters);
          gcov_write_counter (csum->histogram[h_ix].min_value);
          gcov_write_counter (csum->histogram[h_ix].cum_value);
        }
    }
}

/* Write out the current block, if needs be.  */
void gcov_write_block (unsigned size)
{
  /* Write content of gcov_var.buffer in gcov_var.file, amount is size*2 */
  if (fwrite (gcov_var.buffer, size << 2, 1, gcov_var.file) != 1)
    /*If it did not work set error flag*/
    gcov_var.error = 1;
  /* Set aproppriate start and offset */
  gcov_var.start += size;
  gcov_var.offset -= size;
}

/* Move to a given position in a gcov file.  */
void gcov_seek (gcov_position_t base)
{
  if (gcov_var.offset)
    gcov_write_block (gcov_var.offset);

  fseek (gcov_var.file, base << 2, SEEK_SET);
  gcov_var.start = ftell (gcov_var.file) >> 2;
}

/* Return a pointer to read BYTES bytes from the gcov file. Returns
   NULL on failure (read past EOF).  */
static const gcov_unsigned_t * gcov_read_words (unsigned words)
{
  const gcov_unsigned_t *result;
  unsigned excess = gcov_var.length - gcov_var.offset;

  gcov_nonruntime_assert (gcov_var.mode > 0);
  if (excess < words)
    {
      gcov_var.start += gcov_var.offset;
      if (excess)
  {
#if IN_LIBGCOV
    memcpy (gcov_var.buffer, gcov_var.buffer + gcov_var.offset, 4);
#else
    memmove (gcov_var.buffer, gcov_var.buffer + gcov_var.offset,
       excess * 4);
#endif
  }
      gcov_var.offset = 0;
      gcov_var.length = excess;
#if IN_LIBGCOV
      excess = GCOV_BLOCK_SIZE;
#else
      if (gcov_var.length + words > gcov_var.alloc)
  gcov_allocate (gcov_var.length + words);
      excess = gcov_var.alloc - gcov_var.length;
#endif
      excess = fread (gcov_var.buffer + gcov_var.length,
          1, excess << 2, gcov_var.file) >> 2;
      gcov_var.length += excess;
      if (gcov_var.length < words)
  {
    gcov_var.overread += words - gcov_var.length;
    gcov_var.length = 0;
    return 0;
  }
    }
  result = &gcov_var.buffer[gcov_var.offset];
  gcov_var.offset += words;
  return result;
}

/* Return the right value according to endian */
static inline gcov_unsigned_t from_file (gcov_unsigned_t value)
{
#if !IN_LIBGCOV
  if (gcov_var.endian)
    {
      value = (value >> 16) | (value << 16);
      value = ((value & 0xff00ff) << 8) | ((value >> 8) & 0xff00ff);
    }
#endif
  return value;
}

/* Read unsigned value from a coverage file. Sets error flag on file
   error, overflow flag on overflow */
gcov_unsigned_t gcov_read_unsigned (void)
{
  /* Read one word from file */
  gcov_unsigned_t value;
  const gcov_unsigned_t *buffer = gcov_read_words (1);

  if (!buffer)
    return 0;

  /* Check endian*/
  value = from_file (buffer[0]);

  return value;
}

/* Return nonzero if the error flag is set.  */
/* We need to expose this function when compiling for gcov-tool.  */
int gcov_is_error (void)
{
  return gcov_var.file ? gcov_var.error : 1;
}

/* Save the current position in the gcov file.  */
/* We need to expose this function when compiling for gcov-tool.  */
gcov_position_t gcov_position (void)
{
  gcov_nonruntime_assert (gcov_var.mode > 0);

  return gcov_var.start + gcov_var.offset;
}

/* Move to beginning of file and initialize for writing.  */
void gcov_rewrite (void)
{
  gcov_var.mode = -1; 
  gcov_var.start = 0;
  gcov_var.offset = 0;
  fseek (gcov_var.file, 0L, SEEK_SET);
}

/* Close the current gcov file. Flushes data to disk. Returns nonzero
   on failure or error flag set.  */
int gcov_close (void)
{
  if (gcov_var.file)
    {
#if !IN_GCOV
      if (gcov_var.offset && gcov_var.mode < 0)
  gcov_write_block (gcov_var.offset);
#endif
      fclose (gcov_var.file);
      gcov_var.file = 0;
      gcov_var.length = 0;
    }
#if !IN_LIBGCOV
  free (gcov_var.buffer);
  gcov_var.alloc = 0;
  gcov_var.buffer = 0;
#endif
  gcov_var.mode = 0;
  return gcov_var.error;
}

/* Read counter value from a coverage file. Sets error flag on file
   error, overflow flag on overflow */
gcov_type gcov_read_counter (void)
{
  /* Read two words since counter is 64bit value */
  gcov_type value;
  const gcov_unsigned_t *buffer = gcov_read_words (2);

  if (!buffer)
    return 0;

  /* Check endian on lower bits */
  value = from_file (buffer[0]);

  if (sizeof (value) > sizeof (gcov_unsigned_t))
    /* Check edian on upper bits */
    value |= ((gcov_type) from_file (buffer[1])) << 32;
  else if (buffer[1])
    gcov_var.error = -1;

  return value;
}

/* The merge function that just sums the counters.  */
void __gcov_merge_add (gcov_type *counters, unsigned n_counters){
  for (; n_counters; counters++, n_counters--)
    *counters += gcov_read_counter ();
}

/* Return the number of set bits in X.  */
int popcount_hwi (unsigned HOST_WIDE_INT x)
{
  int i, ret = 0;
  size_t bits = sizeof (x) * CHAR_BIT;

  for (i = 0; i < bits; i += 1)
    {
      ret += x & 1;
      x >>= 1;
    }

  return ret;
}

/* Reads given object or program summary record.  */
void gcov_read_summary (struct gcov_summary *summary)
{
  unsigned ix, h_ix, bv_ix, h_cnt = 0;
  struct gcov_ctr_summary *csum;
  unsigned histo_bitvector[GCOV_HISTOGRAM_BITVECTOR_SIZE];
  unsigned cur_bitvector;

  summary->checksum = gcov_read_unsigned ();
  for (csum = summary->ctrs, ix = GCOV_COUNTERS_SUMMABLE; ix--; csum++)
    {
      csum->num = gcov_read_unsigned ();
      csum->runs = gcov_read_unsigned ();
      csum->sum_all = gcov_read_counter ();
      csum->run_max = gcov_read_counter ();
      csum->sum_max = gcov_read_counter ();
      memset (csum->histogram, 0,
              sizeof (gcov_bucket_type) * GCOV_HISTOGRAM_SIZE);
      for (bv_ix = 0; bv_ix < GCOV_HISTOGRAM_BITVECTOR_SIZE; bv_ix++)
        {
          histo_bitvector[bv_ix] = gcov_read_unsigned ();
#if IN_LIBGCOV
          /* When building libgcov we don't include system.h, which includes
             hwint.h (where popcount_hwi is declared). However, libgcov.a
             is built by the bootstrapped compiler and therefore the builtins
             are always available.  */
          h_cnt += __builtin_popcount (histo_bitvector[bv_ix]);
#else
          h_cnt += popcount_hwi (histo_bitvector[bv_ix]);
#endif
        }
      bv_ix = 0;
      h_ix = 0;
      cur_bitvector = 0;
      while (h_cnt--)
        {
          /* Find the index corresponding to the next entry we will read in.
             First find the next non-zero bitvector and re-initialize
             the histogram index accordingly, then right shift and increment
             the index until we find a set bit.  */
          while (!cur_bitvector)
            {
              h_ix = bv_ix * 32;
              if (bv_ix >= GCOV_HISTOGRAM_BITVECTOR_SIZE)
                fprintf (stderr, "corrupted profile info: summary histogram "
                            "bitvector is corrupt");
              cur_bitvector = histo_bitvector[bv_ix++];
            }
          while (!(cur_bitvector & 0x1))
            {
              h_ix++;
              cur_bitvector >>= 1;
            }
          if (h_ix >= GCOV_HISTOGRAM_SIZE)
             fprintf (stderr,"corrupted profile info: summary histogram "
                        "index is corrupt");

          csum->histogram[h_ix].num_counters = gcov_read_unsigned ();
          csum->histogram[h_ix].min_value = gcov_read_counter ();
          csum->histogram[h_ix].cum_value = gcov_read_counter ();
          /* Shift off the index we are done with and increment to the
             corresponding next histogram entry.  */
          cur_bitvector >>= 1;
          h_ix++;
        }
    }
}

/* Dump the coverage counts. We merge with existing counts when
   possible, to avoid growing the .da files ad infinitum. We use this
   program's checksum to make sure we only accumulate whole program
   statistics to the correct summary. An object file might be embedded
   in two separate programs, and we must keep the two program
   summaries separate.  */
void drew_coverage(){

	printf("Testing... \n");

  struct gcov_info *gi_ptr;
  struct gcov_summary this_program;
  struct gcov_summary all;
  struct gcov_ctr_summary *cs_ptr;
  const struct gcov_ctr_info *ci_ptr;
  unsigned t_ix, r_ix;
  gcov_unsigned_t c_num;
  const char *gcov_prefix;
  int gcov_prefix_strip = 0;
  size_t prefix_length;
  char *gi_filename, *gi_filename_up;

  memset (&all, 0, sizeof (all));

  /* Find the totals for this execution.  */
  memset (&this_program, 0, sizeof (this_program));

/* OVO ZA SAD NE!
  /* For all object files in gcov info list... */
/*  for (gi_ptr = gcov_list; gi_ptr; gi_ptr = gi_ptr->next) {
    /* For every function... */
/*   for (r_ix = 0; r_ix < gi_ptr->n_functions; r_ix++){ 
          ci_ptr = gi_ptr->functions[r_ix]->ctrs;
          /* For every counter of that function... */
/*      for (t_ix = 0; t_ix < ci_ptr->num; t_ix++) {
            /* Refresh the value of counter */
 /*           cs_ptr = &this_program.ctrs[t_ix];
            cs_ptr->num += ci_ptr->num;
/*            for (c_num = 0; c_num < ci_ptr->num; c_num++)
              {
                /* and refresh data in summaries... */
/*                cs_ptr->sum_all += ci_ptr->values[c_num];
                if (cs_ptr->run_max < ci_ptr->values[c_num])
              cs_ptr->run_max = ci_ptr->values[c_num];
                }
              ci_ptr++;         
      }
    }
  }

*/


  /* Get file name relocation prefix.  Non-absolute values are ignored. */
  gcov_prefix = getenv("GCOV_PREFIX");

  if (gcov_prefix && IS_ABSOLUTE_PATH (gcov_prefix))
    {
      /* Check if the level of dirs to strip off specified. */
      char *tmp = getenv("GCOV_PREFIX_STRIP");
      if (tmp)
        {
          gcov_prefix_strip = atoi (tmp);
          /* Do not consider negative values. */
          if (gcov_prefix_strip < 0)
            gcov_prefix_strip = 0;
        }
      
      prefix_length = strlen(gcov_prefix);

      /* Remove an unnecessary trailing '/' */
      if (IS_DIR_SEPARATOR (gcov_prefix[prefix_length - 1]))
  prefix_length--;
    }
  else
    prefix_length = 0;

  /* Allocate and initialize the filename scratch space.*/  
  gi_filename = (char *) alloca (prefix_length + gcov_max_filename + 1);
  if (prefix_length)
    memcpy (gi_filename, gcov_prefix, prefix_length);
  gi_filename_up = gi_filename + prefix_length;
  
  /* Now merge each file.  */
  for (gi_ptr = gcov_list; gi_ptr; gi_ptr = gi_ptr->next){

    	
      struct gcov_summary this_object;
      struct gcov_summary object, program;
      gcov_type *values[GCOV_COUNTERS];
      const struct gcov_fn_info *fi_ptr;
      unsigned fi_stride;
      unsigned c_ix, f_ix, n_counts;
      struct gcov_ctr_summary *cs_obj, *cs_tobj, *cs_prg, *cs_tprg, *cs_all;
      int error = 0;
      gcov_unsigned_t tag, length;
      gcov_position_t summary_pos = 0;
      gcov_position_t eof_pos = 0;

      memset (&this_object, 0, sizeof (this_object));
      memset (&object, 0, sizeof (object));
      
      /* Build relocated filename, stripping off leading 
         directories from the initial filename if requested. */
      if (gcov_prefix_strip > 0)
        {
          int level = 0;
          const char *fname = gi_ptr->filename;
          const char *s;

          /* Skip selected directory levels. */
    for (s = fname + 1; (*s != '\0') && (level < gcov_prefix_strip); s++)
      if (IS_DIR_SEPARATOR(*s))
        {
    fname = s;
    level++;
        };

      /* Update complete filename with stripped original. */
          strcpy (gi_filename_up, fname);
        }
      else
        strcpy (gi_filename_up, gi_ptr->filename);

      /* Totals for this object file.  */


/* OVO ZA SAD NE
    /* For every function... */
  /*  for (r_ix = 0; r_ix < gi_ptr->n_functions; r_ix++){ 
          ci_ptr = gi_ptr->functions[r_ix]->ctrs;
          /* For every counter of that function... */
  /*    for (t_ix = 0; t_ix < ci_ptr->num; t_ix++) {        
            /* Refresh the value of counter */
   /*         cs_ptr = &this_object.ctrs[t_ix];
            cs_ptr->num += ci_ptr->num;
   /*         for (c_num = 0; c_num < ci_ptr->num; c_num++)
              {
                /* and refresh data in summaries... */ 
     /*           cs_ptr->sum_all += ci_ptr->values[c_num];
    /*            if (cs_ptr->run_max < ci_ptr->values[c_num])
              cs_ptr->run_max = ci_ptr->values[c_num];
                }
              ci_ptr++;              
      }
    }
    
*/
      c_ix = 0;
      /* Write in VALUES values of all counters as an array of arrays */
      for (t_ix = 0; t_ix < gi_ptr->n_functions; t_ix++){
      	values[c_ix] = gi_ptr->functions[t_ix]->ctrs->values;
      	c_ix++;
  	}
      /* Calculate the function_info stride. This depends on the
       number of counter types being measured.  */
      fi_stride = sizeof (struct gcov_fn_info) + c_ix * sizeof (unsigned);
      if (__alignof__ (struct gcov_fn_info) > sizeof (unsigned))
  {
    fi_stride += __alignof__ (struct gcov_fn_info) - 1;
    fi_stride &= ~(__alignof__ (struct gcov_fn_info) - 1);
  }

/* Flag for errors while oppening gcda file */
int my_err = 1;

const int mode = 0;
#if GCOV_LOCKED
  struct flock s_flock;
  int fd;

  s_flock.l_whence = SEEK_SET;
  s_flock.l_start = 0;
  s_flock.l_len = 0; /* Until EOF.*/ 
  s_flock.l_pid = getpid ();
#endif

  gcov_nonruntime_assert (!gcov_var.file);
  gcov_var.start = 0;
  gcov_var.offset = gcov_var.length = 0;
  gcov_var.overread = -1u;
  gcov_var.error = 0;
#if !IN_LIBGCOV
  gcov_var.endian = 0;
#endif
#if GCOV_LOCKED
  if (mode > 0)
    {
      /* Read-only mode - acquire a read-lock. */ 
      s_flock.l_type = F_RDLCK;
      /* pass mode (ignored) for compatibility */
      fd = open (gi_filename, O_RDONLY, S_IRUSR | S_IWUSR);
    }
  else if (mode < 0)
     {
       /* Write mode - acquire a write-lock.  */
       s_flock.l_type = F_WRLCK;
      fd = open (gi_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    }
  else /* mode == 0 */
    {
      /* Read-Write mode - acquire a write-lock. */
      s_flock.l_type = F_WRLCK;
      fd = open (gi_filename, O_RDWR | O_CREAT, 0666);
    }
  if (fd < 0)
    my_err = 0;

  while (fcntl (fd, F_SETLKW, &s_flock) && errno == EINTR)
    continue;

  gcov_var.file = fdopen (fd, (mode > 0) ? "rb" : "r+b");

  if (!gcov_var.file)
    {
      close (fd);
    my_err = 0;
    }

  if (mode > 0)
    gcov_var.mode = 1;
  else if (mode == 0)
    {
      struct stat st;

      if (fstat (fd, &st) < 0)
	{
	  fclose (gcov_var.file);
	  gcov_var.file = 0;
    my_err = 0;
	}
      if (st.st_size != 0)
	gcov_var.mode = 1;
      else
	gcov_var.mode = mode * 2 + 1;
    }
  else
    gcov_var.mode = mode * 2 + 1;
#else
  if (mode >= 0)
    gcov_var.file = fopen (gi_filename, (mode > 0) ? "rb" : "r+b");

  if (gcov_var.file)
    gcov_var.mode = 1;
  else if (mode <= 0)
    {
      gcov_var.file = fopen (gi_filename, "w+b");
      if (gcov_var.file)
	gcov_var.mode = mode * 2 + 1;
    }
  if (!gcov_var.file)
    my_err = 0;
#endif

  setbuf (gcov_var.file, (char *)0);

  /* Read (magic number) from gcda file*/
 tag = gcov_read_unsigned ();
      if (tag)
  {
    /* Check if right tag for gcda file is read*/
    if (tag != GCOV_DATA_MAGIC)
      {
        fprintf (stderr, "profiling:%s:Not a gcov data file\n",
           gi_filename);
        goto read_fatal;
      }

  /* Read (version) from gcda file*/
    length = gcov_read_unsigned ();
        
    /* Check if right version of gcov is read*/
    if (!gcov_version (gi_ptr, GCOV_VERSION, gi_filename))
      goto read_fatal;

    /* Read (timestamp) from gcda file*/
    length = gcov_read_unsigned ();

   /* Check if right timestamp is read*/
    if (length != gi_ptr->stamp)
      goto rewrite;
    
    /* For every function... */
    for (f_ix = 0; f_ix < gi_ptr->n_functions; f_ix++)
      {
        /* Read ( function tag ang lenght) from gcda file*/
        tag = gcov_read_unsigned ();
        length = gcov_read_unsigned ();

        /* Check if right tag, lenght, identification and checksums is read*/
        if (tag != GCOV_TAG_FUNCTION
      || length != GCOV_TAG_FUNCTION_LENGTH
      || gcov_read_unsigned () != gi_ptr->functions[f_ix]->ident
      || gcov_read_unsigned () != gi_ptr->functions[f_ix]->lineno_checksum
      || gcov_read_unsigned () != gi_ptr->functions[f_ix]->cfg_checksum)
    {
    read_mismatch:;
      fprintf (stderr, "profiling:%s:Merge mismatch for %s\n",
         gi_filename,
         f_ix + 1 ? "function" : "summaries");
      goto read_fatal;
    }

      /* Merge functions... */
        c_ix = 0;
      gcov_merge_fn merge;
      
      n_counts = gi_ptr->functions[f_ix]->ctrs->num ;
      merge = gi_ptr->merge[c_ix] ;
        
      /* Read (counter tag ang lenght) from gcda file*/
      tag = gcov_read_unsigned ();
      length = gcov_read_unsigned ();


      /* Check if right tag and lenght is read*/
      if (tag != GCOV_TAG_FOR_COUNTER (0)
          || length != GCOV_TAG_COUNTER_LENGTH (n_counts))
        goto read_mismatch;

      /* Merge data... */
      (*merge) (values[c_ix], n_counts);
      values[c_ix] += n_counts;
      c_ix++;
        if ((error = gcov_is_error ()))
    goto read_error;
      }

    f_ix = ~0u;

    while (1)
      {
        int is_program;
        
        eof_pos = gcov_position ();

      /* Read (program summary tag) from gcda file*/
        tag = gcov_read_unsigned ();
        if (!tag)
    break;
        /* Read (program summary tag lenght) from gcda file*/
        length = gcov_read_unsigned ();
        is_program = tag == GCOV_TAG_PROGRAM_SUMMARY;

        /* Check if right tag and lenght is read*/
        if (length != GCOV_TAG_SUMMARY_LENGTH(0) 
      || (!is_program && tag != GCOV_TAG_OBJECT_SUMMARY))
    goto read_mismatch;
        gcov_read_summary (is_program ? &program : &object);
        if ((error = gcov_is_error ()))
    goto read_error;
        if (is_program && program.checksum == gcov_crc32)
    {
      summary_pos = eof_pos;
      goto rewrite;
    }
      }

  }
      goto rewrite;
      
    read_error:;
      fprintf (stderr, error < 0 ? "profiling:%s:Overflow merging\n"
         : "profiling:%s:Error merging\n", gi_filename);
        
    read_fatal:;
      gcov_close ();
      continue;

    rewrite:;
      gcov_rewrite ();
      if (!summary_pos)
  memset (&program, 0, sizeof (program));

      f_ix = ~0u;

  /* Calculate new information for summaries... */
  for (t_ix = 0; t_ix < GCOV_COUNTERS_SUMMABLE; t_ix++)
  {
    cs_obj = &object.ctrs[t_ix];
    cs_tobj = &this_object.ctrs[t_ix];
    cs_prg = &program.ctrs[t_ix];
    cs_tprg = &this_program.ctrs[t_ix];
    cs_all = &all.ctrs[t_ix];

    if ((1 << t_ix))
      {
        if (!cs_obj->runs++)
    cs_obj->num = cs_tobj->num;
        else if (cs_obj->num != cs_tobj->num)
    goto read_mismatch;
        cs_obj->sum_all += cs_tobj->sum_all;
        if (cs_obj->run_max < cs_tobj->run_max)
    cs_obj->run_max = cs_tobj->run_max;
        cs_obj->sum_max += cs_tobj->run_max;
        
        if (!cs_prg->runs++)
    cs_prg->num = cs_tprg->num;
        else if (cs_prg->num != cs_tprg->num)
    goto read_mismatch;
        cs_prg->sum_all += cs_tprg->sum_all;
        if (cs_prg->run_max < cs_tprg->run_max)
    cs_prg->run_max = cs_tprg->run_max;
        cs_prg->sum_max += cs_tprg->run_max;
      }
    else if (cs_obj->num || cs_prg->num)
      goto read_mismatch;
    
    if (!cs_all->runs && cs_prg->runs)
      memcpy (cs_all, cs_prg, sizeof (*cs_all));
    else if (!all.checksum
       && (cs_all->runs == cs_prg->runs)
       && memcmp (cs_all, cs_prg, sizeof (*cs_all)))
      {
        all.checksum = ~0u;
      }
  }
      c_ix = 0;
      for (t_ix = 0; t_ix < gi_ptr->n_functions; t_ix++)
  if ((1 << t_ix))
    {
      values[c_ix] = gi_ptr->functions[t_ix]->ctrs->values;
      c_ix++;
    }

      program.checksum = gcov_crc32;

      /* Write out the data. */
      gcov_write_tag_length (GCOV_DATA_MAGIC, version_gcov);
      gcov_write_unsigned (gi_ptr->stamp);
    

      /* Write execution counts for each function. */ 
      for (f_ix = 0; f_ix < gi_ptr->n_functions; f_ix++)
      {
      	
          /* Announce function.  */
          gcov_write_tag_length (GCOV_TAG_FUNCTION, GCOV_TAG_FUNCTION_LENGTH);
          gcov_write_unsigned (gi_ptr->functions[f_ix]->ident);
          gcov_write_unsigned (gi_ptr->functions[f_ix]->lineno_checksum);

          gcov_write_unsigned (gi_ptr->functions[f_ix]->cfg_checksum);


               gcov_write_tag_length (GCOV_TAG_FOR_COUNTER (0),
                  GCOV_TAG_COUNTER_LENGTH (gi_ptr->functions[f_ix]->ctrs->num));

          for (t_ix = 0; t_ix < gi_ptr->functions[f_ix]->ctrs->num; t_ix++){
              gcov_write_counter (gi_ptr->functions[f_ix]->ctrs->values[t_ix]);
             }


          }

      /* Object file summary.  */
      gcov_write_summary (GCOV_TAG_OBJECT_SUMMARY, &object);

      /* Program summary. */
      gcov_write_summary (GCOV_TAG_PROGRAM_SUMMARY, &program);


/* Write to gcda file word_number of words... */
gcov_write_block ((word_number));

}

/* Free memory */
free(gcov_var.buffer);
fclose(gcov_var.file);

}
