
#ifndef _MINIMAL_INCREMENR_CBF_H
#define _MINIMAL_INCREMENR_CBF_H

#ifdef __cplusplus
extern "C" {
#endif


/** ***************************************************************************
 * Structure to keep track of one minimal increment CBF.  Caller needs to
 * allocate this and pass it to the functions below. First call for
 * every struct must be to minimalIncrementCBF_init().
 *
 */
struct minimalIncrementCBF
{
  // These fields are part of the public interface of this structure.
  // Client code may read these values if desired. Client code MUST NOT
  // modify any of these.
  int entries;
  double error;
  // int bits;
  // int bytes;
  int hashes;
  int counter_num;

  // Fields below are private to the implementation. These may go away or
  // change incompatibly at any moment. Client code MUST NOT access or rely
  // on these.
  double bpe;
  unsigned int * bf;
  int ready;
};


/** ***************************************************************************
 * Initialize the minimal Increment CBF for use.
 *
 *
 * Optimal number of bits is:
 *     bits = (entries * ln(error)) / ln(2)^2
 *
 * Optimal number of hash functions is:
 *     hashes = bpe * ln(2)
 *
 * Parameters:
 * -----------
 *     bloom   - Pointer to an allocated struct bloom (see above).
 *     entries - The expected number of entries which will be inserted.
 *               Must be at least 1000 (in practice, likely much larger).
 *     error   - Probability of collision (as long as entries are not
 *               exceeded).
 *
 * Return:
 * -------
 *     0 - on success
 *     1 - on failure
 *
 */
int minimalIncrementCBF_init(struct minimalIncrementCBF * CBF, int entries, double error);


/** ***************************************************************************
 * Deprecated, use minimalIncrementCBF_init()
 *
 */
int minimalIncrementCBF_init_size(struct minimalIncrementCBF * CBF, int entries, double error,
                    unsigned int cache_size);


// /** ***************************************************************************
//  * Check if the given element is in the bloom filter. Remember this may
//  * return false positive if a collision occurred.
//  *
//  * Parameters:
//  * -----------
//  *     bloom  - Pointer to an allocated struct bloom (see above).
//  *     buffer - Pointer to buffer containing element to check.
//  *     len    - Size of 'buffer'.
//  *
//  * Return:
//  * -------
//  *     >= 0 - element is present (or false positive due to collision)
//  *    -1 - bloom not initialized
//  *
//  */
int minimalIncrementCBF_estimate(struct minimalIncrementCBF * CBF, const void * buffer, int len);


/** ***************************************************************************
 * Add the given element to the bloom filter.
 * The return code indicates if the element (or a collision) was already in,
 * so for the common check+add use case, no need to call check separately.
 *
 */
int minimalIncrementCBF_add(struct minimalIncrementCBF * CBF, const void * buffer, int len);


/** ***************************************************************************
 * Print (to stdout) info about this bloom filter. Debugging aid.
 *
 */
void minimalIncrementCBF_print(struct minimalIncrementCBF * CBF);


/** ***************************************************************************
 * Deallocate internal storage.
 *
 * Upon return, the bloom struct is no longer usable. You may call bloom_init
 * again on the same struct to reinitialize it again.
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *
 * Return: none
 *
 */
void minimalIncrementCBF_free(struct minimalIncrementCBF * CBF);

/** ***************************************************************************
 * Decay internal storage.
 *
 * Decay all elements (by 2).
 *
 * Parameters:
 * -----------
 *     bloom  - Pointer to an allocated struct bloom (see above).
 *
 * Return:
 *     0 - on success
 *     1 - on failure
 *
 */
int minimalIncrementCBF_decay(struct minimalIncrementCBF * bloom);


/** ***************************************************************************
 * Returns version string compiled into library.
 *
 * Return: version string
 *
 */
const char * minimalIncrementCBF_version(void);

#ifdef __cplusplus
}
#endif

#endif