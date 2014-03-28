//
// System time handlers
//
// by James L. Hammons
// (C) 2005 Underground Software
//
// JLH = James L. Hammons <jlhamm@acm.org>
//
// WHO  WHEN        WHAT
// ---  ----------  ------------------------------------------------------------
// JLH  01/04/2006  Cosmetic changes (like this one ;-)
//

// STILL TO DO:
//
// - Handling for an event that occurs NOW
//

#include "timing.h"

#include "log.h"

#define EVENT_LIST_SIZE       512

// NOTE ABOUT TIMING SYSTEM DATA STRUCTURES:

// A queue won't work for this system because we can't guarantee that an event
// will go in with a time that is later than the ones already queued up. So we
// just use a simple list.

// Although if we used an insertion sort we could, but it wouldn't work for
// adjusting times...

struct Event
{
    bool valid;
    double eventTime;
    void (* timerCallback)(void);
};

//let's try +1... nope.
static Event eventList[EVENT_LIST_SIZE];
static uint32_t nextEvent;

void InitializeEventList(void)
{
    for(uint32_t i=0; i<EVENT_LIST_SIZE; i++)
        eventList[i].valid = false;
}

//We just slap the next event into the list, no checking, no nada...
void SetCallbackTime(void (* callback)(void), double time)
{
    for(uint32_t i=0; i<EVENT_LIST_SIZE; i++)
    {
        if (!eventList[i].valid)
        {
//WriteLog("SCT: Found callback slot #%u...\n", i);
            eventList[i].timerCallback = callback;
            eventList[i].eventTime = time;
            eventList[i].valid = true;

            return;
        }
    }

    WriteLog("TIMING: SetCallbackTime() failed to find an empty slot in the list!\n");
}

void RemoveCallback(void (* callback)(void))
{
    for(uint32_t i=0; i<EVENT_LIST_SIZE; i++)
    {
        if (eventList[i].valid && eventList[i].timerCallback == callback)
        {
            eventList[i].valid = false;

            return;
        }
    }
}

void AdjustCallbackTime(void (* callback)(void), double time)
{
    for(uint32_t i=0; i<EVENT_LIST_SIZE; i++)
    {
        if (eventList[i].valid && eventList[i].timerCallback == callback)
        {
            eventList[i].eventTime = time;

            return;
        }
    }
}

double GetTimeToNextEvent(void)
{
	// Find the next event. Since the events are not necessarily in order of
	// increasing time, we have to search through the list for the lowest one.

//ALSO: There's a bug here--nextEvent is getting a bogus value/getting clobbered...
//      Seems like it's getting clobbered somewhere other than here...
    double time = 0;
    bool firstTime = true;

    for(uint32_t i=0; i<EVENT_LIST_SIZE; i++)
    {
        if (eventList[i].valid)
        {
            if (firstTime)
			{
                time = eventList[i].eventTime;
				nextEvent = i;
				firstTime = false;
			}
            else
            {
                if (eventList[i].eventTime < time)
				{
                    time = eventList[i].eventTime;
					nextEvent = i;
				}
            }
        }
    }

	if (time == 0)
		WriteLog("TIMING: GetTimeToNextEvent() failed to find next event!\n");

	if (nextEvent >= EVENT_LIST_SIZE)
		WriteLog("TIMING: GetTimeToNextEvent() has bad nextEvent (=%u)!\n", nextEvent);

    return time;
}

void HandleNextEvent(void)
{
    double elapsedTime = eventList[nextEvent].eventTime;
    void (* event)(void) = eventList[nextEvent].timerCallback;

    for(uint32_t i=0; i<EVENT_LIST_SIZE; i++)
        if (eventList[i].valid)
            eventList[i].eventTime -= elapsedTime;

    eventList[nextEvent].valid = false;      // Remove event from list...

    (*event)();
}
