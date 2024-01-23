/** @file
  This is a test application that demonstrates how to use the C-style entry point
  for a shell application.

  Copyright (c) 2009 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/FileInfo.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>

#define FRAME_HEIGHT 360
#define FRAME_WIDTH 480
#define PIXELS (FRAME_HEIGHT * FRAME_WIDTH)
#define FRAME_OFFSET_Y ((ScreenHeight - FRAME_HEIGHT) / 2)
#define FRAME_OFFSET_X ((ScreenWidth - FRAME_WIDTH) / 2)
#define FRAME_STALL 26500 // 30fps?

#define BLACK_PIXEL ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL) { .Red = 0, .Blue = 0, .Green = 0 })
#define WHITE_PIXEL ((EFI_GRAPHICS_OUTPUT_BLT_PIXEL) { .Red = 255, .Blue = 255, .Green = 255 })

#define FRAMES_FILE_PATH L"\\BadApple.bin"

STATIC
VOID
ReadNextFrame(
    UINT8 *VideoBuffer,
    UINTN *Index,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Frame
) {
    // Get current point in video buffer.
    UINT8 *Video = VideoBuffer + *Index;

    // Get the number of runs.
    UINTN Runs = (UINTN)(*(UINT16 *)Video);
    Video += sizeof(UINT16);

    UINTN FrameIndex = 0;
    for(UINTN Run = 0; Run < Runs; Run++) {
        // Get current pixel value.
        UINT8 Value = *Video;
        Video += sizeof(Value);

        // Get run length.
        UINT16 ShortRunLength = *(UINT16 *)Video;
        Video += sizeof(ShortRunLength);
        UINTN RunLength = (UINTN)ShortRunLength;

        // Print(L"FrameIndex %u: %u run of length %u\n", FrameIndex, Value, RunLength);

        // Set frame buffer pixels according to run.
        for(UINTN RunIndex = 0; RunIndex < RunLength; RunIndex++) {
            if(Value) {
                Frame[FrameIndex + RunIndex] = WHITE_PIXEL;
            } else {
                Frame[FrameIndex + RunIndex] = BLACK_PIXEL;
            }
        }
        FrameIndex += RunLength;
    }

    // Push forward index.
    *Index = (Video - VideoBuffer);
}

STATIC
EFI_STATUS
GetVideoData(
    EFI_HANDLE FilesystemHandle,
    UINT8 **VideoData,
    UINTN *VideoDataSize
) {
    EFI_STATUS Status = EFI_SUCCESS;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Filesystem = NULL;
    Status = gBS->HandleProtocol(
        FilesystemHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&Filesystem);
    if(EFI_ERROR(Status)) {
        Print(L"Couldn't open filesystem.\n");
        return Status;
    }

    EFI_FILE_PROTOCOL *Root = NULL;
    Status = Filesystem->OpenVolume(Filesystem, &Root);
    if(EFI_ERROR(Status)) {
        Print(L"Couldn't open filesystem.\n");
        return Status;
    }

    EFI_FILE_PROTOCOL *VideoFile = NULL;
    Status = Root->Open(Root, &VideoFile, FRAMES_FILE_PATH, EFI_FILE_MODE_READ, 0);
    if(EFI_ERROR(Status)) {
        Print(L"Couldn't open video file.\n");
        return Status;
    }

    UINT8 FileInfoPtr[sizeof(EFI_FILE_INFO) + 256];
    UINTN FileInfoSize = sizeof(FileInfoPtr);
    Status = VideoFile->GetInfo(VideoFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfoPtr);
    EFI_FILE_INFO *FileInfo = (EFI_FILE_INFO *)FileInfoPtr;
    if(EFI_ERROR(Status)) {
        Print(L"Failed to get file size: %d\n", Status);
        return Status;
    }

    UINTN FileSize = FileInfo->FileSize;
    Print(L"Video file: %u bytes.\n", FileSize);
    if(FileSize == 0) {
        return EFI_NOT_FOUND;
    }

    Status = gBS->AllocatePool(EfiBootServicesData, FileSize, (VOID **)VideoData);
    if(EFI_ERROR(Status)) {
        Print(L"Failed to alloc: %d\n", Status);
        return Status;
    }

    Status = VideoFile->Read(VideoFile, &FileSize, *VideoData);
    if(EFI_ERROR(Status)) {
        Print(L"Failed to read video data.\n");
        gBS->FreePool(*VideoData);
    }

    *VideoDataSize = FileSize;
    return Status;
}

/**
  UEFI application entry point which has an interface similar to a
  standard C main function.

  The ShellCEntryLib library instance wrappers the actual UEFI application
  entry point and calls this ShellAppMain function.

  @param[in] Argc     The number of items in Argv.
  @param[in] Argv     Array of pointers to strings.

  @retval  0               The application exited normally.
  @retval  Other           An error occurred.

**/
EFI_STATUS
EFIAPI
BadAppleMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
    EFI_STATUS Status = EFI_SUCCESS;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *Screen = NULL;
    Status = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID **)&Screen
    );
    if(EFI_ERROR(Status)) {
        return Status;
    }

    UINTN ScreenWidth = Screen->Mode->Info->HorizontalResolution;
    UINTN ScreenHeight = Screen->Mode->Info->VerticalResolution;

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *ScreenBuffer = NULL;
    Status = gBS->AllocatePool(EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * ScreenHeight * ScreenWidth, (VOID **)&ScreenBuffer);
    if(Status) {
        return Status;
    }

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *FrameBuffer = NULL;
    Status = gBS->AllocatePool(EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * FRAME_HEIGHT * FRAME_WIDTH, (VOID **)&FrameBuffer);
    if(Status) {
        return Status;
    }

    ZeroMem(ScreenBuffer, sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * ScreenHeight * ScreenWidth);
    Status = Screen->Blt(Screen, ScreenBuffer, EfiBltVideoToBltBuffer, 0, 0, 0, 0, ScreenWidth, ScreenHeight, 0);

    UINT8 *Video = NULL;
    UINTN VideoSize = 0;

    UINTN Handles = 0;
    EFI_HANDLE *FsHandles = NULL;
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiSimpleFileSystemProtocolGuid,
        NULL,
        &Handles,
        &FsHandles);
    if(EFI_ERROR(Status)) {
        Print(L"Failed to get filesystems.\n");
        goto fail;
    }

    for(UINTN Idx = 0; Idx < Handles; Idx++) {
        Status = GetVideoData(FsHandles[Idx], &Video, &VideoSize);
        if(Status == EFI_SUCCESS) {
            break;
        }
    }
    if(EFI_ERROR(Status)) {
        Print(L"Video retrieve failed.\n");
        goto fail;
    }

    UINTN VideoIndex = 0;
    while(VideoIndex < VideoSize) {
        ReadNextFrame(Video, &VideoIndex, FrameBuffer);

        // Copy pixels line-by-line.
        for(UINTN FY = 0; FY < FRAME_HEIGHT; FY++) {
            CopyMem(
                ScreenBuffer + ((FY + FRAME_OFFSET_Y) * ScreenWidth) + FRAME_OFFSET_X,
                FrameBuffer + (FY * FRAME_WIDTH),
                FRAME_WIDTH * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
        }

        Status = Screen->Blt(Screen, ScreenBuffer, EfiBltBufferToVideo, FRAME_OFFSET_X, FRAME_OFFSET_Y, FRAME_OFFSET_X, FRAME_OFFSET_Y, FRAME_WIDTH, FRAME_HEIGHT, sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * ScreenWidth);
        if(EFI_ERROR(Status)) {
            goto fail;
        }

        gBS->Stall(FRAME_STALL);
    }

fail:

    ScreenBuffer[0] = BLACK_PIXEL;

    if(EFI_ERROR(Status)) {
        ScreenBuffer[0] = WHITE_PIXEL;
        Print(L"Failure: %d\n", Status);
    }

    Status = Screen->Blt(Screen, ScreenBuffer, EfiBltVideoFill, 0, 0, 0, 0, ScreenWidth, ScreenHeight, 0);

    for(;;) {
        gBS->Stall(1000000);
    }
}
