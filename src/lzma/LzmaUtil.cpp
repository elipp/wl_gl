/* LzmaUtil.c -- Test application for LZMA compression
2010-09-20 : Igor Pavlov : Public domain */

#define _CRT_SECURE_NO_WARNINGS

#include "lzma/mylzma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzma/Alloc.h"
#include "lzma/LzmaDec.h"
#include "lzma/7zFile.h"

const char *kCantReadMessage = "Can not read input file";
const char *kCantWriteMessage = "Can not write output file";
const char *kCantAllocateMessage = "Can not allocate memory";
const char *kDataErrorMessage = "Data error";

static void *SzAlloc(void *p, size_t size) { p = p; return MyAlloc(size); }
static void SzFree(void *p, void *address) { p = p; MyFree(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };


int PrintError(char *buffer, const char *message)
{
  strcat(buffer, "\nError: ");
  strcat(buffer, message);
  strcat(buffer, "\n");
  fprintf(stderr, "error: %s\n", message);
  return 1;
}

int PrintErrorNumber(char *buffer, SRes val)
{
  sprintf(buffer + strlen(buffer), "\nError code: %x\n", (unsigned)val);
  return 1;
}


#define IN_BUF_SIZE (1 << 16)
#define OUT_BUF_SIZE (1 << 16)

static SRes Decode2(CLzmaDec *state, ISeqOutStream *outStream, ISeqInStream *inStream,
    UInt64 unpackSize, char *out_buffer)
{
  int thereIsSize = (unpackSize != (UInt64)(Int64)-1);
  Byte inBuf[IN_BUF_SIZE];
  Byte outBuf[OUT_BUF_SIZE];
  size_t inPos = 0, inSize = 0, outPos = 0;
  LzmaDec_Init(state);

  SizeT total_offset = 0;

  for (;;)
  {
	  // reads in chunks of 65k, apparently
    if (inPos == inSize)
    {
      inSize = IN_BUF_SIZE;
      RINOK(inStream->Read(inStream, inBuf, &inSize));
      inPos = 0;
    }
    {
      SRes res;
      SizeT inProcessed = inSize - inPos;
      SizeT outProcessed = OUT_BUF_SIZE - outPos;
      ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
      ELzmaStatus status;
      if (thereIsSize && outProcessed > unpackSize)
      {
        outProcessed = (SizeT)unpackSize;
        finishMode = LZMA_FINISH_END;
      }
      
      res = LzmaDec_DecodeToBuf(state, outBuf + outPos, &outProcessed,
        inBuf + inPos, &inProcessed, finishMode, &status);
      inPos += inProcessed;
      outPos += outProcessed;
      unpackSize -= outProcessed;
      
	  memcpy(out_buffer + total_offset, outBuf, outPos);
	  total_offset += outPos;
		
      outPos = 0;
      
      if (res != SZ_OK || thereIsSize && unpackSize == 0)
        return res;
      
      if (inProcessed == 0 && outProcessed == 0)
      {
        if (thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
          return SZ_ERROR_DATA;
        return res;
      }
    }
  }
}

static SRes Decode(ISeqOutStream *outStream, ISeqInStream *inStream, char **out_buffer, size_t *out_size)
{
  UInt64 unpackSize;
  int i;
  SRes res = 0;

  CLzmaDec state;

  /* header: 5 bytes of LZMA properties and 8 bytes of uncompressed size */
  unsigned char header[LZMA_PROPS_SIZE + 8];

  /* Read and parse header */

  RINOK(SeqInStream_Read(inStream, header, sizeof(header)));

  unpackSize = 0;
  for (i = 0; i < 8; i++)
    unpackSize += (UInt64)header[LZMA_PROPS_SIZE + i] << (i * 8);

  *out_buffer = new char[unpackSize];

  *out_size = unpackSize;

  LzmaDec_Construct(&state);
  RINOK(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc));
  res = Decode2(&state, outStream, inStream, unpackSize, *out_buffer);
  LzmaDec_Free(&state, &g_Alloc);
  return res;
}

int LZMA_decode(const std::string &filename, char **out_buffer, size_t *out_size) {
	char errbuf[800] = {0};
  CFileSeqInStream inStream;
  CFileOutStream outStream;
  int res;

  FileSeqInStream_CreateVTable(&inStream);
  File_Construct(&inStream.file);

  if (InFile_Open(&inStream.file, filename.c_str()) != 0) {
    fprintf(stderr, "Can not open input file %s\n", filename.c_str());
	return 0;
  }
  
  // even though we're not actually using the outStream, it can safely be passed to the decode chain (we're not using it in any way)

  res = Decode(&outStream.s, &inStream.s, out_buffer, out_size);
  
  File_Close(&inStream.file);

  if (res != SZ_OK)
  {
    if (res == SZ_ERROR_MEM)
      return PrintError(errbuf, kCantAllocateMessage);
    else if (res == SZ_ERROR_DATA)
      return PrintError(errbuf, kDataErrorMessage);
    else if (res == SZ_ERROR_WRITE)
      return PrintError(errbuf, kCantWriteMessage);
    else if (res == SZ_ERROR_READ)
      return PrintError(errbuf, kCantReadMessage);
    return PrintErrorNumber(errbuf, res);
  }
  return 0;
}
