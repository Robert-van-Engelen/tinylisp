#ifndef __TEST_UTIL_H
#define __TEST_UTIL_H
extern void save_iostreams(void);
extern void reset_iostreams(void);
extern void stdin_from_str(const char *input_str);
extern void stderr_to_str(char *buf, int buf_len);

#endif  // __TEST_UTIL_H
