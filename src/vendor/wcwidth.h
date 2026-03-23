/*
 * wcwidth.h - determine the display width of a Unicode character in a terminal.
 *
 * Based on Markus Kuhn's implementation, adapted for use in beresta.
 * See: https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */

#ifndef BERESTA_WCWIDTH_H
#define BERESTA_WCWIDTH_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns the number of terminal columns needed to display the
 * given Unicode codepoint:
 *   -1  for non-printable/control characters
 *    0  for combining characters and zero-width
 *    1  for most characters (Latin, Cyrillic, etc.)
 *    2  for CJK ideographs and other wide characters
 */
int mk_wcwidth(int ucs);

#ifdef __cplusplus
}
#endif

#endif /* BERESTA_WCWIDTH_H */
