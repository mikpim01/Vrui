/***********************************************************************
LargeFile - Wrapper class for the stdio FILE interface for files larger
than 2GB with exception safety, typed data I/O, and automatic
endianness conversion.
Copyright (c) 2005 Oliver Kreylos

This file is part of the Miscellaneous Support Library (Misc).

The Miscellaneous Support Library is free software; you can
redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

The Miscellaneous Support Library is distributed in the hope that it
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Miscellaneous Support Library; if not, write to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA
***********************************************************************/

#ifndef MISC_LARGEFILE_INCLUDED
#define MISC_LARGEFILE_INCLUDED

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdexcept>
#include <Misc/ThrowStdErr.h>
#include <Misc/Endianness.h>

namespace Misc {

class LargeFile
	{
	/* Embedded classes: */
	public:
	#ifdef __DARWIN__
	typedef fpos_t Offset; // Type for file offsets
	#else
	typedef off64_t Offset; // Type for file offsets
	#endif
	
	enum Endianness // Enumerated type to enforce file endianness
		{
		DontCare,LittleEndian,BigEndian
		};
	
	class OpenError:public std::runtime_error // Exception class to report file opening errors
		{
		/* Constructors and destructors: */
		public:
		OpenError(const char* fileName,const char* openMode)
			:std::runtime_error(std::string("Error opening file ")+std::string(fileName)+std::string(" in mode ")+std::string(openMode))
			{
			}
		};
	
	class ReadError:public std::runtime_error // Exception class to report file reading errors
		{
		/* Constructors and destructors: */
		public:
		ReadError(size_t numBytes,size_t numBytesRead)
			:std::runtime_error(printStdErrMsg("Misc::LargeFile: Error reading %u bytes from file, read %u bytes instead",numBytes,numBytesRead))
			{
			}
		};
	
	class WriteError:public std::runtime_error // Exception class to report file writing errors
		{
		/* Constructors and destructors: */
		public:
		WriteError(size_t numBytes,size_t numBytesWritten)
			:std::runtime_error(printStdErrMsg("Misc::LargeFile: Error writing %u bytes from file, wrote %u bytes instead",numBytes,numBytesWritten))
			{
			}
		};
	
	/* Elements: */
	private:
	char* openMode; // Mode the file was opened with
	FILE* filePtr; // Pointer to the stdio file structure
	Endianness endianness; // Endianness of the represented file
	bool mustSwapEndianness; // Flag if current file endianness is different from machine endianness
	
	/* Constructors and destructors: */
	public:
	LargeFile(const char* fileName,const char* sOpenMode,Endianness sEndianness =DontCare) // Opens file by name
		:openMode(new char[strlen(sOpenMode)+1]),
		 #ifdef __DARWIN__
		 filePtr(fopen(fileName,sOpenMode))
		 #else
		 filePtr(fopen64(fileName,sOpenMode))
		 #endif
		{
		strcpy(openMode,sOpenMode);
		if(filePtr==0)
			{
			delete[] openMode;
			throw OpenError(fileName,sOpenMode);
			}
		setEndianness(sEndianness);
		}
	LargeFile(int fileNumber,const char* sOpenMode,Endianness sEndianness =DontCare) // Opens file from already open low-level file number; inherits file number
		:openMode(new char[strlen(sOpenMode)+1]),
		 filePtr(fdopen(fileNumber,sOpenMode))
		{
		strcpy(openMode,sOpenMode);
		setEndianness(sEndianness);
		}
	LargeFile(const LargeFile& source) // Copy constructor
		:openMode(new char[strlen(source.openMode)+1]),
		 filePtr(fdopen(dup(fileno(source.filePtr)),source.openMode)),
		 endianness(source.endianness),mustSwapEndianness(source.mustSwapEndianness)
		{
		strcpy(openMode,source.openMode);
		}
	~LargeFile(void)
		{
		delete[] openMode;
		if(filePtr!=0)
			fclose(filePtr);
		}
	
	/* Methods: */
	FILE* getFilePtr(void) // Converts File object to C stdio FILE pointer
		{
		return filePtr;
		}
	Endianness getEndianness(void) // Returns current endianness setting of file
		{
		return endianness;
		}
	void setEndianness(Endianness newEndianness) // Sets current endianness setting of file
		{
		endianness=newEndianness;
		#if __BYTE_ORDER==__LITTLE_ENDIAN
		mustSwapEndianness=endianness==BigEndian;
		#endif
		#if __BYTE_ORDER==__BIG_ENDIAN
		mustSwapEndianness=endianness==LittleEndian;
		#endif
		}
	void rewind(void)
		{
		::rewind(filePtr);
		}
	void seekSet(Offset offset)
		{
		#ifdef __DARWIN__
		fseeko(filePtr,offset,SEEK_SET);
		#else
		fseeko64(filePtr,offset,SEEK_SET);
		#endif
		}
	void seekCurrent(Offset offset)
		{
		#ifdef __DARWIN__
		fseeko(filePtr,offset,SEEK_CUR);
		#else
		fseeko64(filePtr,offset,SEEK_CUR);
		#endif
		}
	void seekEnd(Offset offset)
		{
		#ifdef __DARWIN__
		fseeko(filePtr,offset,SEEK_END);
		#else
		fseeko64(filePtr,offset,SEEK_END);
		#endif
		}
	Offset tell(void)
		{
		#ifdef __DARWIN__
		return ftello(filePtr);
		#else
		return ftello64(filePtr);
		#endif
		}
	bool eof(void)
		{
		return feof(filePtr);
		}
	
	/* Methods for text file I/O: */
	int getc(void)
		{
		return fgetc(filePtr);
		}
	int ungetc(int c)
		{
		return ::ungetc(c,filePtr);
		}
	#if 0
	void putc(int c)
		{
		fputc(c,filePtr);
		}
	#endif
	char* gets(char* stringBuffer,int stringBufferSize)
		{
		return fgets(stringBuffer,stringBufferSize,filePtr);
		}
	int puts(const char* string)
		{
		return fputs(string,filePtr);
		}
	
	/* Methods for binary file I/O with endianness conversion: */
	template <class DataParam>
	DataParam read(void) // Reads single value
		{
		DataParam result;
		size_t numBytesRead=fread(&result,1,sizeof(DataParam),filePtr);
		if(numBytesRead!=sizeof(DataParam))
			throw ReadError(sizeof(DataParam),numBytesRead);
		if(mustSwapEndianness)
			swapEndianness(result);
		return result;
		}
	template <class DataParam>
	DataParam& read(DataParam& data) // Reads single value through reference
		{
		size_t numBytesRead=fread(&data,1,sizeof(DataParam),filePtr);
		if(numBytesRead!=sizeof(DataParam))
			throw ReadError(sizeof(DataParam),numBytesRead);
		if(mustSwapEndianness)
			swapEndianness(data);
		return data;
		}
	template <class DataParam>
	size_t read(DataParam* data,size_t numItems) // Reads array of values
		{
		size_t numItemsRead=fread(data,sizeof(DataParam),numItems,filePtr);
		if(mustSwapEndianness)
			swapEndianness(data,numItemsRead);
		return numItemsRead;
		}
	template <class DataParam>
	void write(const DataParam& data) // Writes single value
		{
		size_t numBytesWritten;
		if(mustSwapEndianness)
			{
			DataParam temp=data;
			swapEndianness(temp);
			numBytesWritten=fwrite(&temp,1,sizeof(DataParam),filePtr);
			}
		else
			numBytesWritten=fwrite(&data,1,sizeof(DataParam),filePtr);
		if(numBytesWritten!=sizeof(DataParam))
			throw WriteError(sizeof(DataParam),numBytesWritten);
		}
	template <class DataParam>
	void write(const DataParam* data,size_t numItems) // Writes array of values
		{
		size_t numBytesWritten;
		if(mustSwapEndianness)
			{
			numBytesWritten=0;
			for(size_t i=0;i<numItems;++i)
				{
				DataParam temp=data[i];
				swapEndianness(temp);
				numBytesWritten+=fwrite(&temp,1,sizeof(DataParam),filePtr);
				}
			}
		else
			numBytesWritten=fwrite(data,sizeof(DataParam),numItems,filePtr)*sizeof(DataParam);
		if(numBytesWritten!=sizeof(DataParam)*numItems)
			throw WriteError(sizeof(DataParam)*numItems,numBytesWritten);
		}
	};

}

#endif
