//
// LOG.H
//

#ifndef __LOG_H__
#define __LOG_H__

// Make this header work with either C or C++

#ifdef __cplusplus
extern "C" {
#endif

bool InitLog(const char *);
void LogDone(void);
void WriteLog(const char * text, ...);

#ifdef __cplusplus
}
#endif

#endif	// __LOG_H__
