/**
 * PROJECT:         Native Shell
 * COPYRIGHT:       LGPL; See LICENSE in the top level directory
 * FILE:            input.c
 * DESCRIPTION:     This module deals with device input (such as mouse or keyboard).
 * DEVELOPERS:      See CONTRIBUTORS.md in the top level directory
 */

#include "precomp.h"

// FIXME: Temporary here

NTSTATUS
RtlClipBackspace(
    VOID);

// Event to wait on for keyboard input
HANDLE hEvent;

// Raw keyboard character buffer

ULONG CurrentChar = 0;

// Input buffer
CHAR Line[1024];
CHAR CurrentPosition = 0;

/*++
 * @name RtlCliOpenInputDevice
 *
 * The RtlCliOpenInputDevice routine opens an input device.
 *
 * @param Handle
 *        Pointer where the handle for the input device will be returned.
 *
 * @param Type
 *        Type of the input device to use.
 *
 * @return STATUS_SUCCESS or error code when attemping to open the device.
 *
 * @remarks This routine supports both mouse and keyboard input devices.
 *
 *--*/
NTSTATUS
RtlCliOpenInputDevice(OUT PHANDLE Handle,
                      IN CON_DEVICE_TYPE Type)
{
    UNICODE_STRING Driver;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK Iosb;
    HANDLE hDriver;
    NTSTATUS Status;

    // Chose the driver to use
    // FIXME: Support MouseType later
    // FIXME: Don't hardcode keyboard path

    if (Type == KeyboardType)
    {
        RtlInitUnicodeString(&Driver, L"\\Device\\KeyboardClass0");
    }

    // Initialize the object attributes
    InitializeObjectAttributes(&ObjectAttributes,
                               &Driver,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    // Open a handle to it
    Status = NtCreateFile(&hDriver,
                          SYNCHRONIZE | GENERIC_READ | FILE_READ_ATTRIBUTES,
                          &ObjectAttributes,
                          &Iosb,
                          NULL,
                          FILE_ATTRIBUTE_NORMAL,
                          0,
                          FILE_OPEN,
                          FILE_DIRECTORY_FILE,
                          NULL,
                          0);

    // Now create an event that will be used to wait on the device
    InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);
    Status = NtCreateEvent(&hEvent, EVENT_ALL_ACCESS, &ObjectAttributes, 1, 0);

    // Return the handle
    *Handle = hDriver;
    return Status;
}

/*++
 * @name RtlClipWaitForInput
 *
 * The RtlClipWaitForInput routine waits for input from an input device.
 *
 * @param hDriver
 *        Handle of the driver/device to get input from.
 *
 * @param Buffer
 *        Input buffer.
 *
 * @param BufferSize
 *        Size of the input buffer.
 *
 * @return STATUS_SUCCESS or error code from the read operation.
 *
 * @remarks This routine waits for input to be available.
 *
 *--*/
NTSTATUS
RtlClipWaitForInput(IN HANDLE hDriver,
                    IN PVOID Buffer,
                    IN OUT PULONG BufferSize)
{
    IO_STATUS_BLOCK Iosb;
    LARGE_INTEGER ByteOffset;
    NTSTATUS Status;

    // Clean up the I/O Status block and read from byte 0

    RtlZeroMemory(&Iosb, sizeof(Iosb));
    RtlZeroMemory(&ByteOffset, sizeof(ByteOffset));

    // Try to read the data

    Status = NtReadFile(hDriver,
                        hEvent,
                        NULL,
                        NULL,
                        &Iosb,
                        Buffer,
                        *BufferSize,
                        &ByteOffset,
                        NULL);

    // Check if data is pending

    if (Status == STATUS_PENDING)
    {

        // Wait on the data to be read

        Status = NtWaitForSingleObject(hEvent, TRUE, NULL);
    }

    // Return status and how much data was read

    *BufferSize = (ULONG)Iosb.Information;
    return Status;
}

CHAR RtlCliGetChar(IN HANDLE hDriver)
{
    KEYBOARD_INPUT_DATA KeyboardData;
    KBD_RECORD kbd_rec;
    ULONG BufferLength = sizeof(KEYBOARD_INPUT_DATA);

    RtlClipWaitForInput(hDriver, &KeyboardData, &BufferLength);

    IntTranslateKey(&KeyboardData, &kbd_rec);

    if (!kbd_rec.bKeyDown)
    {
        return (-1);
    }
    return kbd_rec.AsciiChar;
}

/*++
 * @name RtlCliGetLine
 *
 * The RtlCliGetLine routine gets line from keyboard.
 *
 * @param hDriver
 *        Keyboard handle.
 *
 * @return PCHAR
 *
 * @remarks Because we don't currently have a thread to display on screen
 *          whatever is typed, we handle this in the same thread and display
 *          a character only if someone is actually waiting for it. This
 *          will be changed later.
 */
PCHAR
RtlCliGetLine(IN HANDLE hDriver)
{
    CHAR Char;
    BOOLEAN First = FALSE;

    // Wait for a new character

    while (TRUE)
    {
        // Get the character that was pressed
        Char = RtlCliGetChar(hDriver);

        // Check if this was ENTER
        if (Char == '\r')
        {
            // First, null-terminate the line buffer
            Line[CurrentPosition] = ANSI_NULL;
            CurrentPosition = 0;

            // Return it
            return Line;
        }
        else if (Char == '\b')
        {
            // Make sure we don't back-space beyond the limit
            if (CurrentPosition)
            {
                // NtDisplayString does not properly handle backspace, so
                // we unfortunately have to rely on a hack.

                // We have to call in the display subsystem to redisplay the
                // current text buffer and replace last character with space.

                RtlCliPutChar('\r');
                RtlClipBackspace();

                RtlCliPutChar(' ');

                RtlCliPutChar('\r');
                RtlClipBackspace();

                // Now we do the only thing we're supposed to do, which is to
                // remove a character in the command buffer as well.
                CurrentPosition--;
            }

            // Continue listening for chars.
            continue;
        }

        // We got another character. Make sure it's not NULL.
        if (!Char || Char == -1)
            continue;

        // Add it to our line buffer
        Line[CurrentPosition] = Char;
        CurrentPosition++;

        // Again, as noted earlier, we combine input with display in a very
        // unholy way, so we also have to display it on screen.
        RtlCliPutChar(Char);
    }
}
