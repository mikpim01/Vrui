/***********************************************************************
PrintfTemplateTests - Helper functions to test the well-formedness of
strings to be used as templates for printf functions.
Copyright (c) 2018 Oliver Kreylos

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

#include <Misc/PrintfTemplateTests.h>

namespace Misc {

namespace {

/**************
Helper classes:
**************/

class TemplateChecker // Helper class to run a template checking state machine
	{
	/* Embedded classes: */
	private:
	enum State // Enumerated type for template checking machine states
		{
		Start,Percent,Width,Precision,Error
		};
	
	/* Elements: */
	char conversionType; // Type of conversion to look for
	unsigned int maxLength; // Maximum allowed length for strings generated by the template, including the terminating NUL
	bool haveConversion; // Flag if a valid conversion of the given type was found
	unsigned int conversionWidth; // Width for the found conversion
	unsigned int conversionPrecision; // Precision for the found conversion
	unsigned int length; // Number of non-conversion characters generated by the template
	State state; // Machine's current state
	
	/* Constructors and destructors: */
	public:
	TemplateChecker(char sConversionType,unsigned int sMaxLength) // Creates a template checker for the given conversion type
		:conversionType(sConversionType),maxLength(sMaxLength),
		 haveConversion(false),conversionWidth(0),conversionPrecision(0),length(0),
		 state(Start)
		{
		}
	
	/* Methods: */
	void operator()(char c) // Advances the state machine by processing the given character
		{
		switch(state)
			{
			case Start:
				if(c=='%')
					state=Percent;
				else
					++length;
				break;
			
			case Percent:
				if(c=='%')
					{
					++length;
					state=Start;
					}
				else if(c>='0'&&c<='9')
					{
					conversionWidth=(unsigned int)(c-'0');
					state=Width;
					}
				else if(c=='.')
					state=Precision;
				else if(c==conversionType)
					{
					if(haveConversion)
						state=Error;
					else
						{
						haveConversion=true;
						state=Start;
						}
					}
				else
					state=Error;
				break;
			
			case Width:
				if(c>='0'&&c<='9')
					{
					conversionWidth=conversionWidth*10+(unsigned int)(c-'0');
					if(conversionWidth>=maxLength)
						state=Error;
					}
				else if(c=='.')
					state=Precision;
				else if(c==conversionType)
					{
					if(haveConversion)
						state=Error;
					else
						{
						haveConversion=true;
						state=Start;
						}
					}
				else
					state=Error;
				break;
			
			case Precision:
				if(c>='0'&&c<='9')
					{
					conversionPrecision=conversionPrecision*10+(unsigned int)(c-'0');
					if(conversionPrecision>=maxLength)
						state=Error;
					}
				else if(c==conversionType)
					{
					if(haveConversion)
						state=Error;
					else
						{
						haveConversion=true;
						state=Start;
						}
					}
				else
					state=Error;
				break;
			
			case Error:
				break;
			}
		}
	bool isValid(void) const // Returns true if the parsed template was valid
		{
		/* Template was invalid if it has an unfinished conversion, and error, or not exactly one conversion of the requested type: */
		if(state!=Start||!haveConversion)
			return false;
		
		/* Template is valid if the maximum possible string length is within bounds: */
		unsigned int stringLength;
		switch(conversionType)
			{
			case 'u':
				stringLength=10;
				break;
			
			case 'd':
				stringLength=11;
				break;
			
			default:
				stringLength=0;
			}
		
		/* Adjust for conversion width and precision: */
		if(stringLength<conversionWidth)
			stringLength=conversionWidth;
		if(stringLength<conversionPrecision)
			stringLength=conversionPrecision;
		
		/* Add the verbatim character length of the template: */
		stringLength+=length;
		
		return stringLength<maxLength;
		}
	};

}

bool isValidUintTemplate(const char* templateString,unsigned int maxLength)
	{
	/* Create a template checker: */
	TemplateChecker tc('u',maxLength);
	
	/* Process the template string: */
	for(const char* tPtr=templateString;*tPtr!='\0';++tPtr)
		tc(*tPtr);
	
	/* Return the parsing result: */
	return tc.isValid();
	}

bool isValidUintTemplate(const std::string& templateString,unsigned int maxLength)
	{
	/* Create a template checker: */
	TemplateChecker tc('u',maxLength);
	
	/* Process the template string: */
	for(std::string::const_iterator tIt=templateString.begin();tIt!=templateString.end();++tIt)
		tc(*tIt);
	
	/* Return the parsing result: */
	return tc.isValid();
	}

bool isValidIntTemplate(const char* templateString,unsigned int maxLength)
	{
	/* Create a template checker: */
	TemplateChecker tc('d',maxLength);
	
	/* Process the template string: */
	for(const char* tPtr=templateString;*tPtr!='\0';++tPtr)
		tc(*tPtr);
	
	/* Return the parsing result: */
	return tc.isValid();
	}

bool isValidIntTemplate(const std::string& templateString,unsigned int maxLength)
	{
	/* Create a template checker: */
	TemplateChecker tc('d',maxLength);
	
	/* Process the template string: */
	for(std::string::const_iterator tIt=templateString.begin();tIt!=templateString.end();++tIt)
		tc(*tIt);
	
	/* Return the parsing result: */
	return tc.isValid();
	}

}
