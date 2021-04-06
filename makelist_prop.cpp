/*
	Merge two separate data files from dsink and ecollector to 
	a single file. Format is similar to dsink
*/
#define _FILE_OFFSET_BITS 64
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "epecur.h"

/****************************************************************************************************************************************
 *							class PropCycle									*
 ****************************************************************************************************************************************/
#define MAXHITS	1000

class PropCycle {
private:
	char *buf;
	int size;
	RecordHeaderStruct *head;
	int rptr;
	void Realloc(int newsize);
	void StoreHit(int num, int wire);
public:
	PropCycle(void);
	~PropCycle(void);
	inline int GetCycleNumber(void) { return head->number;};
	int GetNumberOfTriggers(void);
	int Read(FILE *f);
};

PropCycle::PropCycle(void)
{
	buf = NULL;
	size = 0;
	head = NULL;
	rptr = 0;
}

PropCycle::~PropCycle(void)
{
	if (buf) free(buf);
}

int PropCycle::GetNumberOfTriggers(void)
{
	int i;
	EventHeaderStruct *evt;
	int trig;

	trig = 0;
	for (i=rptr; i < head->len; i += evt->len) {
		evt = (EventHeaderStruct *)&buf[i];
		if (evt->flag & 0x80000000) break;
		trig++;
	}
	return trig;
}

int PropCycle::Read(FILE *f)
{
	int irc;
	int len;

	irc = fread(&len, sizeof(int), 1, f);
	if (irc != 1) {
		if (irc != 0) printf("Strange read of PROP file %m\n");
		return -1;
	}
	if (len < sizeof(RecordHeaderStruct) + sizeof(EventHeaderStruct)) {
		printf("Strange file record of %d bytes found\n", len);
		return -2;
	}
	if (len > size) Realloc(len);
	memcpy(buf, &len, sizeof(int));
	irc = fread(&buf[sizeof(int)], len - sizeof(int), 1, f);
	if (irc != 1) {
		printf("Unexpected EOF - %m\n");
		return -3;
	}
	rptr = sizeof(RecordHeaderStruct);
	if (head->type == REC_CYCLE) return 1;
	return 0;
}

void PropCycle::Realloc(int newsize)
{
	char *tmp;
	
	tmp = (char *)realloc(buf, newsize);
	if (!tmp) {
		printf("No memory: %m\n");
		throw "No memory";
	}
	buf = tmp;
	head = (RecordHeaderStruct *) tmp;
	size = newsize;
}

/****************************************************************************************************************************************
 *							the main									*
 ****************************************************************************************************************************************/

int main(int argc, char **argv)
{
	FILE *fPRP;
	FILE *fOut;
	PropCycle *prop;
	int iCyclePRP;
	int CycleCnt;
	int fnum;
	char str[1024];
	int irc;
	int Events;

    	if (argc < 2) {
		printf("Merge DANSS and EPECUR data files with the same run number\n");
		printf("Part 1 - make a list of prop cycles\n");
		printf("Usage: %s NNN\n", argv[0]);
		printf("Will use input file: data/prop_NNN.data\n");
		printf("And output file data/prop_list_NNN.data\n");
		return 0;
    	}

	fnum = strtol(argv[1], NULL, 10);

	sprintf(str, "data/prop_%3.3d.data", fnum);
	fPRP = fopen(str, "rb");

	sprintf(str, "data/prop_list_%3.3d.data", fnum);
	fOut = fopen(str, "wt");

	if (!fPRP || !fOut) {
		printf("Can not open all required files\n");
		return 10;
	}

	prop = new PropCycle();

	CycleCnt = 0;
	for(;;) {
//		Read proportional chambers cycle
		irc = prop->Read(fPRP);
		if (irc < 0) {
			goto fin;
		} else if (irc != 1) {
			continue;
		}
		iCyclePRP = prop->GetCycleNumber();
		Events = prop->GetNumberOfTriggers();
		fprintf(fOut, "%8d %8d %8d\n", CycleCnt, iCyclePRP, Events);
		CycleCnt++;
	}
fin:
	delete prop;

	fclose(fPRP);
	fclose(fOut);
	return 0;
}
