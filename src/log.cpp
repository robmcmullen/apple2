//
// Log handler
//
// by James Hammons
// (C) 2006 Underground Software
//
// JLH = James Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  -----------------------------------------------------------
// JLH  01/03/2006  Moved includes out of header file for faster compilation
//

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

// Maximum size of log file (10 MB ought to be enough for anybody)
#define MAX_LOG_SIZE		10000000

static FILE * log_stream = NULL;
static uint32_t logSize = 0;


bool InitLog(const char * path)
{
	log_stream = fopen(path, "w");

	if (log_stream == NULL)
		return false;

	return true;
}


void LogDone(void)
{
	if (log_stream)
		fclose(log_stream);
}


//
// This logger is used mainly to ensure that text gets written to the log file
// even if the program crashes. The performance hit is acceptable in this case!
//
void WriteLog(const char * text, ...)
{
	if (!log_stream)
		return;

	va_list arg;

	va_start(arg, text);
	logSize += vfprintf(log_stream, text, arg);
	va_end(arg);

	fflush(log_stream);					// Make sure that text is written!

	if (logSize > MAX_LOG_SIZE)
	{
		fclose(log_stream);
		log_stream = NULL;
	}
}

