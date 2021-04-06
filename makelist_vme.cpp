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
#include "recformat-2.h"

/****************************************************************************************************************************************
 *							Class OutEvent									*
 ****************************************************************************************************************************************/

class OutEvent {
private:
	char *buf;
	int size;
	struct rec_header_struct *head;
	void Realloc(int newsize);
public:
	OutEvent(void);
	~OutEvent(void);
	int GetCycleNumber(void);
	int Read(FILE *f);
};

OutEvent::OutEvent(void)
{
	buf = NULL;
	size = 0;
	head = NULL;
}

OutEvent::~OutEvent(void)
{
	if (buf) free(buf);
}

int OutEvent::GetCycleNumber(void)
{
	union hw_rec_union *rec;
	int ptr;
	int cn;

	if (!head) {
		printf("Internal error - GetUserWord before Set\n");
		throw "Internal error - GetUserWord before Set";
	}
	for (ptr = sizeof(struct rec_header_struct); ptr < head->len; ptr += (rec->rec.len + 1) * sizeof(short)) {
		rec = (union hw_rec_union *) &buf[ptr];
		if (rec->rec.type == TYPE_CYCLE) break;
	}
	if (ptr >= head->len) return -1;
	cn = (rec->cycle.ccnt[1] << 15) + rec->cycle.ccnt[0];
	return cn;
}

int OutEvent::Read(FILE *f)
{
	int irc;
	int len;

	irc = fread(&len, sizeof(int), 1, f);
	if (irc != 1) {
		if (irc != 0) printf("Strange read of VME file %m\n");
		return -1;
	}
	if (len < sizeof(struct rec_header_struct)) {
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
	if (head->type & REC_EVENT) return 1;
	return 0;
}

void OutEvent::Realloc(int newsize)
{
	char *tmp;
	
	tmp = (char *)realloc(buf, newsize);
	if (!tmp) {
		printf("No memory: %m\n");
		throw "No memory";
	}
	buf = tmp;
	head = (struct rec_header_struct *) tmp;
	size = newsize;
}

/****************************************************************************************************************************************
 *							the main									*
 ****************************************************************************************************************************************/

int main(int argc, char **argv)
{
	FILE *fVME;
	FILE *fOut;
	OutEvent *evt;
	int iCycleVME;
	int EventCnt;
	int CycleCnt;
	int Events;
	int iStopCnt;
	char str[1024];
	int irc;
	int fnum;

    	if (argc < 2) {
		printf("Merge DANSS and EPECUR data files with the same run number\n");
		printf("Part II - make vme cycle list\n");
		printf("Usage: %s NNN\n", argv[0]);
		printf("Will use input files: data/vme_NNN.data\n");
		printf("And output file data/vme_list_NNN.data\n");
		return 0;
    	}

	fnum = strtol(argv[1], NULL, 10);

	sprintf(str, "data/vme_%3.3d.data", fnum);
	fVME = fopen(str, "rb");

	sprintf(str, "data/vme_list_%3.3d.data", fnum);
	fOut = fopen(str, "wt");

	if (!fVME || !fOut) {
		printf("Can not open all required files\n");
		return 10;
	}

	evt = new OutEvent();

	EventCnt = CycleCnt = Events = 0;
	for(;;) {
//		Read VME cycle
		irc = evt->Read(fVME);
		if (irc < 0) {
			break;
		} else if (irc == 0) {
			continue;
		}
		iCycleVME = evt->GetCycleNumber();
		if (iCycleVME < 0) {
			Events++;
		} else {
			fprintf(fOut, "%8d %8d %8d %6d\n", EventCnt, CycleCnt, iCycleVME, Events);
			CycleCnt++;
			Events = 0;
		}
		EventCnt++;
	}
fin:
	delete evt;

	fclose(fVME);
	fclose(fOut);
	return 0;
}
