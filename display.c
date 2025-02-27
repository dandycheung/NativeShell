/**
 * PROJECT:         Native Shell
 * COPYRIGHT:       LGPL; See LICENSE in the top level directory
 * FILE:            display.c
 * DESCRIPTION:     This module handles displaying output to screen.
 * DEVELOPERS:      See CONTRIBUTORS.md in the top level directory
 */

#include "precomp.h"

WCHAR DisplayBuffer[1024];
USHORT LinePos = 0;
WCHAR PutChar[2] = L" ";
UNICODE_STRING CharString = {2, 2, PutChar};

/*++
 * @name RtlCliPrintString
 *
 * The RtlCliPrintString routine display a unicode string on the display device
 *
 * @param Message
 *        Pointer to a unicode string containing the message to print.
 *
 * @return STATUS_SUCCESS or failure code.
 *
 * @remarks None.
 *
 *--*/
NTSTATUS
RtlCliPrintString(IN PUNICODE_STRING Message)
{
    ULONG i;
    NTSTATUS Status;

    for (i = 0; i < (Message->Length / sizeof(WCHAR)); i++)
    {
        Status = RtlCliPutChar(Message->Buffer[i]);
    }

    return Status;
}

/*++
 * @name RtlCliPutChar
 *
 * The RtlCliPutChar routine displays a character.
 *
 * @param Char
 *        Character to print out.
 *
 * @return STATUS_SUCCESS or failure code.
 *
 * @remarks None.
 *
 *--*/
NTSTATUS
RtlCliPutChar(IN WCHAR Char)
{
    // Initialize the string
    CharString.Buffer[0] = Char;

    // Make sure that this isn't backspace
    if (Char != '\r')
    {
        // Check if it's a new line
        if (Char == '\n')
        {
            // Reset the display buffer
            LinePos = 0;
            DisplayBuffer[LinePos] = UNICODE_NULL;
        }
        else
        {
            // Add the character in our buffer
            DisplayBuffer[LinePos] = Char;
            LinePos++;
        }
    }

    // Print the character
    return NtDisplayString(&CharString);
}

/*++
 * @name RtlClipBackspace
 *
 * The RtlClipBackspace routine handles a backspace command.
 *
 * @param None.
 *
 * @return STATUS_SUCCESS or failure code if printing failed.
 *
 * @remarks Backspace is handled by printing the previous string minus the last
 *          two characters.
 *
 *--*/
NTSTATUS
RtlClipBackspace(VOID)
{
    UNICODE_STRING BackString;

    // Update the line position
    LinePos--;

    // Finalize this buffer and make it unicode
    DisplayBuffer[LinePos] = ANSI_NULL;
    RtlInitUnicodeString(&BackString, DisplayBuffer);

    // Display the buffer
    return NtDisplayString(&BackString);
}

NTSTATUS
__cdecl RtlCliDisplayString(IN PCH Message, ...)
{
    va_list MessageList;
    PCHAR MessageBuffer;
    UNICODE_STRING MessageString;
    NTSTATUS Status;

    MessageBuffer = RtlAllocateHeap(RtlGetProcessHeap(), 0, 512);

    // First, combine the message
    va_start(MessageList, Message);
    _vsnprintf(MessageBuffer, 512, Message, MessageList);
    va_end(MessageList);

    // Now make it a unicode string
    RtlCreateUnicodeStringFromAsciiz(&MessageString, MessageBuffer);

    // Display it on screen
    Status = RtlCliPrintString(&MessageString);

    // Free Memory
    RtlFreeHeap(RtlGetProcessHeap(), 0, MessageBuffer);
    RtlFreeUnicodeString(&MessageString);

    return Status;
}
