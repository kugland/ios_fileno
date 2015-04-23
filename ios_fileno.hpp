#ifndef UB9A03A1A_6B38_4DF7_ACEB_A488EF6A6CB6
#define UB9A03A1A_6B38_4DF7_ACEB_A488EF6A6CB6

#include <ios>

/**
 * Gets the file descriptor of an ios stream.
 *
 * If the stream is not associated to any file descriptor
 * (e.g., string streams), returns -1 and sets errno to EBADF.
 *
 * @param  stream   stream whose fd one wants to get
 * @return          fd associated to the stream, or -1 if none.
 */
int ios_fileno(const std::ios& stream);

#if !(defined(__GLIBCXX__) || defined(__GLIBCPP__)) || (defined(_GLIBCPP_USE_WCHAR_T) || defined(_GLIBCXX_USE_WCHAR_T))

/**
 * Gets the file descriptor of an ios stream.
 *
 * If the stream is not associated to any file descriptor
 * (e.g., string streams), returns -1 and sets errno to EBADF.
 *
 * @param  stream   stream whose fd one wants to get
 * @return          fd associated to the stream, or -1 if none.
 */
int ios_fileno(const std::wios& stream);

#endif

#endif // UB9A03A1A_6B38_4DF7_ACEB_A488EF6A6CB6
