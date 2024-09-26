/***********************************************************************
*                    SEGGER Microcontroller GmbH                       *
*                        The Embedded Experts                          *
************************************************************************
*                                                                      *
*                  (c) SEGGER Microcontroller GmbH                     *
*                        All rights reserved                           *
*                          www.segger.com                              *
*                                                                      *
************************************************************************
*                                                                      *
************************************************************************
*                                                                      *
*                                                                      *
*  Licensing terms                                                     *
*                                                                      *
* Redistribution and use in source and binary forms, with or without   *
* modification, are permitted provided that the following conditions   *
* are met:                                                             *
*                                                                      *
* 1. Redistributions of source code must retain the above copyright    *
* notice, this list of conditions and the following disclaimer.        *
*                                                                      *
* 2. Redistributions in binary form must reproduce the above           *
* copyright notice, this list of conditions and the following          *
* disclaimer in the documentation and/or other materials provided      *
* with the distribution.                                               *
*                                                                      *
*                                                                      *
* THIS SOFTWARE IS PROVIDED BY COPYRIGHT HOLDER "AS IS" AND ANY        *
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR   *
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE        *
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,     *
* OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,             *
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR   *
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY  *
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT         *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE    *
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH.    *
* DAMAGE.                                                              *
*                                                                      *
************************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : FlashPrg.c
Purpose : Implementation of RAMCode template
*/

#include <stddef.h>
#include "FlashOS.h"

#include "SystemConfig.h"

/*********************************************************************
 *
 *       Defines (configurable)
 *
 **********************************************************************
 */

//
// Only compile in functions that make sense to keep RAMCode as small as possible
//
#define SUPPORT_NATIVE_VERIFY (0)        // Non-memory mapped flashes only. Flash cannot be read memory-mapped
#define SUPPORT_NATIVE_READ_FUNCTION (0) // Non-memory mapped flashes only. Flash cannot be read memory-mapped
#define SUPPORT_ERASE_CHIP (1)           // To potentially speed up production programming: Erases whole flash bank / chip with special command
#define SUPPORT_TURBO_MODE (1)           // Currently available for Cortex-M only
#define SUPPORT_SEGGER_OPEN_ERASE (1)    // Flashes with uniform sectors only. Speed up erase because 1 OFL call may erase multiple sectors

/*********************************************************************
 *
 *       Defines (fixed)
 *
 **********************************************************************
 */

#define PAGE_SIZE_SHIFT (11)   // Smallest amount of data that can be programmed. <PageSize> = 2 ^ Shift. Shift = 11 => <PageSize> = 2 ^ 11 = 2048 bytes
#define SECTOR_SIZE_SHIFT (11) // Flashes with uniform sectors only. <SectorSize> = 2 ^ Shift. Shift = 11 => <SectorSize> = 2 ^ 11 = 2048 bytes

//
// Default definitions for optional functions if not compiled in
// Makes Api table code further down less ugly
//
#if (SUPPORT_ERASE_CHIP == 0)
#define EraseChip NULL
#endif
#if (SUPPORT_NATIVE_VERIFY == 0)
#define Verify NULL
#endif
#if (SUPPORT_NATIVE_READ_FUNCTION == 0)
#define SEGGER_OPEN_Read NULL
#endif
#if (SUPPORT_SEGGER_OPEN_ERASE == 0)
#define SEGGER_OPEN_Erase NULL
#endif
#if (SUPPORT_TURBO_MODE == 0)
#define SEGGER_OPEN_Start NULL
#endif

/*********************************************************************
 *
 *       Types
 *
 **********************************************************************
 */

typedef struct
{
  U32 AddVariablesHere;
} RESTORE_INFO;

static void _FeedWatchdog(void);

/*********************************************************************
 *
 *       Static data
 *
 **********************************************************************
 */

static RESTORE_INFO _RestoreInfo;

/*********************************************************************
 *
 *       Public data
 *
 **********************************************************************
 */

volatile int PRGDATA_StartMarker __attribute__((section("PrgData"))); // Mark start of <PrgData> segment. Non-static to make sure linker can keep this symbol. Dummy needed to make sure that <PrgData> section in resulting ELF file is present. Needed by open flash loader logic on PC side

const SEGGER_OFL_API SEGGER_OFL_Api __attribute__((section("PrgCode"))) = { // Mark start of <PrgCode> segment. Non-static to make sure linker can keep this symbol.
    _FeedWatchdog,
    Init,
    UnInit,
    EraseSector,
    ProgramPage,
    BlankCheck,
    EraseChip,
    Verify,
    SEGGER_OPEN_CalcCRC,
    SEGGER_OPEN_Read,
    SEGGER_OPEN_Program,
    SEGGER_OPEN_Erase,
    SEGGER_OPEN_Start};

/*********************************************************************
 *
 *       Static code
 *
 **********************************************************************
 */

/*********************************************************************
 *
 *       _FeedWatchdog
 *
 *  Function description
 *    Feeds the watchdog. Needs to be called during RAMCode execution in case of an watchdog is active.
 *    In case no handling is necessary, it could perform a dummy access, to make sure that this function is linked in
 */
static void _FeedWatchdog(void)
{
  *((volatile int *)&PRGDATA_StartMarker); // Dummy operation
}

/*********************************************************************
 *
 *       Public code
 *
 **********************************************************************
 */

/*********************************************************************
 *
 *       Init
 *
 *  Function description
 *    Handles the initialization of the flash module.
 *    It is called once per flash programming step (Erase, Program, Verify)
 *
 *  Parameters
 *    Addr: Flash base address
 *    Freq: Clock frequency in Hz
 *    Func: Specifies the action followed by Init() (e.g.: 1 - Erase, 2 - Program, 3 - Verify / Read)
 *
 *  Return value
 *    == 0  O.K.
 *    == 1  Error
 *
 *  Notes
 *    (1) This function is mandatory.
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
int Init(U32 Addr, U32 Freq, U32 Func)
{
  /* Func is not 1 or 2 or 3, it does not work. */
  SystemInit();         // 系统初始化
  SystemClock_Config(); // 配置HSI系统时钟，主频170MHz
  HAL_FLASH_Unlock();

  return 0;
}

/*********************************************************************
 *
 *       UnInit
 *
 *  Function description
 *    Handles the de-initialization of the flash module.
 *    It is called once per flash programming step (Erase, Program, Verify)
 *
 *  Parameters
 *    Func  Caller type (e.g.: 1 - Erase, 2 - Program, 3 - Verify)
 *
 *  Return value
 *    == 0  O.K.
 *    == 1  Error
 *
 *  Notes
 *    (1) This function is mandatory.
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
int UnInit(U32 Func)
{
  (void)Func;

  /* Func equals to 0 */

  return 0;
}

/*********************************************************************
 *
 *       EraseSector
 *
 *  Function description
 *    Erases one flash sector.
 *
 *  Parameters
 *    SectorAddr  Absolute address of the sector to be erased
 *
 *  Return value
 *    == 0  O.K.
 *    == 1  Error
 *
 *  Notes
 *    (1) This function is mandatory.
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
int EraseSector(U32 SectorAddr)
{
  //
  // Erase sector code
  //
  _FeedWatchdog();
  uint32_t BankNum, FirstPage = 0, PageError = 0;

  /* Clear OPTVERR bit set on virgin samples */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

  /* Erase the user Flash area
    (area defined by FLASH_USER_START_ADDR and FLASH_USER_END_ADDR) ***********/

  /* Get the 1st page to erase */
  FirstPage = GetPage(SectorAddr);
  BankNum = GetBank(SectorAddr);

  /*Variable used for Erase procedure*/
  FLASH_EraseInitTypeDef EraseInitStruct;

  /* Fill EraseInit structure*/
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  EraseInitStruct.Banks = BankNum;
  EraseInitStruct.Page = FirstPage;
  EraseInitStruct.NbPages = 1;

  /* Note: If an erase operation in Flash memory also concerns data in the data or instruction cache,
     you have to make sure that these data are rewritten before they are accessed during code
     execution. If this cannot be done safely, it is recommended to flush the caches by setting the
     DCRST and ICRST bits in the FLASH_CR register. */
  if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
    return 1;
  return 0;
}

/*********************************************************************
 *
 *       ProgramPage
 *
 *  Function description
 *    Programs one flash page.
 *
 *  Parameters
 *    DestAddr  Address to start programming on
 *    NumBytes  Number of bytes to program. Guaranteed to be == <FlashDevice.PageSize>
 *    pSrcBuff  Pointer to data to be programmed
 *
 *  Return value
 *    == 0  O.K.
 *    == 1  Error
 *
 *  Notes
 *    (1) This function is mandatory.
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
int ProgramPage(U32 DestAddr, U32 NumBytes, U8 *pSrcBuff)
{
  int r = -1;

  HAL_StatusTypeDef errorCode;
  uint32_t endAddress;
  uint64_t *p = (uint64_t *)pSrcBuff;

  endAddress = DestAddr + NumBytes;
  // NumBytes = (NumBytes + 255) & ~255; // Adjust size for 256 bytes, write 256 bytes everytime

  while (DestAddr < endAddress)
  {
    errorCode = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FAST, DestAddr, ((uint32_t)p));
    if (errorCode == HAL_OK)
    {
      DestAddr += (32 * sizeof(uint64_t)); /* increment for the next Flash word*/
      p += 32;                             /* increment for the next Data word*/
    }
    else
    {
      return errorCode;
    }
  }

  return 0;
}

/*********************************************************************
 *
 *       BlankCheck
 *
 *  Function description
 *    Checks if a memory region is blank
 *
 *  Parameters
 *    Addr       Address to start checking
 *    NumBytes   Number of bytes to be checked
 *    BlankData  Blank (erased) value of flash (Most flashes have 0xFF, some have 0x00, some do not have a defined erased value)
 *
 *  Return value
 *    == 0  O.K., blank
 *    == 1  O.K., *not* blank
 *     < 0  Error
 *
 *  Notes
 *    (1) This function is optional. If not present, the J-Link software will assume that erased state of a sector can be determined via normal memory-mapped readback of sector.
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
int BlankCheck(U32 Addr, U32 NumBytes, U8 BlankData)
{
  /* This function will not be called. */
  _FeedWatchdog();
  /* always erase sectors */
  return 1;
}

/*********************************************************************
 *
 *       SEGGER_OPEN_CalcCRC
 *
 *  Function description
 *    Calculates the CRC over a specified number of bytes
 *    Even more optimized version of Verify() as this avoids downloading the compare data into the RAMCode for comparison.
 *    Heavily reduces traffic between J-Link software and target and therefore speeds up verification process significantly.
 *
 *  Parameters
 *    CRC       CRC start value
 *    Addr      Address where to start calculating CRC from
 *    NumBytes  Number of bytes to calculate CRC on
 *    Polynom   Polynom to be used for CRC calculation
 *
 *  Return value
 *    CRC
 *
 *  Notes
 *    (1) This function is optional
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
U32 SEGGER_OPEN_CalcCRC(U32 crc, U32 Addr, U32 NumBytes, U32 Polynom)
{
  crc = SEGGER_OFL_Lib_CalcCRC(&SEGGER_OFL_Api, crc, Addr, NumBytes, Polynom); // Use lib function from SEGGER by default. Pass API pointer to it because it may need to call the read function (non-memory mapped flashes)
  return crc;
}

/*********************************************************************
 *
 *       SEGGER_OPEN_Program
 *
 *  Function description
 *    Optimized variant of ProgramPage() which allows multiple pages to be programmed in 1 RAMCode call.
 *
 *  Parameters
 *    DestAddr  Address to start flash programming at.
 *    NumBytes  Number of bytes to be program. Guaranteed to be multiple of <FlashDevice.PageSize>
 *    pSrcBuff  Pointer to data to be programmed
 *
 *  Return value
 *    == 0  O.K.
 *    == 1  Error
 *
 *  Notes
 *    (1) This function is optional. If not present, the J-Link software will use ProgramPage()
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
int SEGGER_OPEN_Program(U32 DestAddr, U32 NumBytes, U8 *pSrcBuff)
{
  U32 NumPages;
  int r;

  NumPages = (NumBytes >> PAGE_SIZE_SHIFT);
  r = 0;
  do
  {
    r = ProgramPage(DestAddr, (1uL << PAGE_SIZE_SHIFT), pSrcBuff);
    if (r < 0)
    {
      return r;
    }
    DestAddr += (1uL << PAGE_SIZE_SHIFT);
    pSrcBuff += (1uL << PAGE_SIZE_SHIFT);
  } while (--NumPages);
  return r;
}

/*********************************************************************
 *
 *       Verify
 *
 *  Function description
 *    Verifies flash contents.
 *    Usually not compiled in. Only needed for non-memory mapped flashes.
 *
 *  Parameters
 *    Addr      Address to start verify on
 *    NumBytes  Number of bytes to verify
 *    pBuff     Pointer data to compare flash contents to
 *
 *  Return value
 *    == (Addr + NumBytes): O.K.
 *    != (Addr + NumBytes): *not* O.K. (ideally the fail address is returned)
 *
 *  Notes
 *    (1) This function is optional. If not present, the J-Link software will assume that flash memory can be verified via memory-mapped readback of flash contents.
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
#if SUPPORT_NATIVE_VERIFY
U32 Verify(U32 Addr, U32 NumBytes, U8 *pBuff)
{
  unsigned char *pFlash;
  unsigned long r;

  pFlash = (unsigned char *)Addr;
  r = Addr + NumBytes;
  do
  {
    if (*pFlash != *pBuff)
    {
      r = (unsigned long)pFlash;
      break;
    }
    pFlash++;
    pBuff++;
  } while (--NumBytes);
  return r;
}
#endif

/*********************************************************************
 *
 *       EraseChip
 *
 *  Function description
 *    Erases the entire flash.
 *
 *  Return value
 *    == 0  O.K.
 *    == 1  Error
 *
 *  Notes
 *    (1) This function is optional. If not present, J-Link will always use EraseSector() for erasing.
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
#if SUPPORT_ERASE_CHIP
int EraseChip(void)
{
  _FeedWatchdog();

  uint32_t SECTORError = 0, PAGEError = 0;

  /* Variable used for Erase procedure */
  FLASH_EraseInitTypeDef EraseInitStruct;

  /* Clear OPTVERR bit set on virgin samples */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

  /* Fill EraseInit structure*/
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_MASSERASE;
  EraseInitStruct.Banks = FLASH_BANK_BOTH;

  if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) != HAL_OK)
    return 1;

  return 0;
}
#endif

/*********************************************************************
 *
 *       SEGGER_OPEN_Read
 *
 *  Function description
 *    Reads a specified number of bytes from flash into the provided buffer.
 *    Usually not compiled in. Only needed for non-memory mapped flashes.
 *
 *  Parameters
 *    Addr      Address to start reading from
 *    NumBytes  Number of bytes to read
 *    pDestBuff Pointer to buffer to store read data
 *
 *  Return value
 *    >= 0: O.K., NumBytes read
 *    <  0: Error
 *
 *  Notes
 *    (1) This function is optional. If not present, the J-Link software will assume that a normal memory-mapped read can be performed to read from flash.
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
#if SUPPORT_NATIVE_READ_FUNCTION
int SEGGER_OPEN_Read(U32 Addr, U32 NumBytes, U8 *pDestBuff)
{
  //
  // Read function
  // Add your code here...
  //
  //_FeedWatchdog();
  return NumBytes;
}
#endif

/*********************************************************************
 *
 *       SEGGER_OPEN_Erase
 *
 *  Function description
 *    Erases one or more flash sectors.
 *    The implementation from this template only works on flashes that have uniform sectors.
 *
 *  Notes
 *    (1) This function can rely on that at least one sector will be passed
 *    (2) This function must be able to handle multiple sectors at once
 *    (3) This function can rely on that only multiple sectors of the same sector
 *        size will be passed. (e.g. if the device has two sectors with different
 *        sizes, the DLL will call this function two times with NumSectors = 1)
 *
 *  Parameters
 *    SectorAddr:  Address of the start sector to be erased
 *    SectorIndex: Index of the start sector to be erased (1st sector handled by this flash bank: SectorIndex == 0)
 *    NumSectors:  Number of sectors to be erased. Min. 1
 *
 *  Return value
 *    == 0  O.K.
 *    == 1  Error
 *
 *  Notes
 *    (1) This function is optional. If not present, the J-Link software will use EraseSector()
 *    (2) Use "noinline" attribute to make sure that function is never inlined and label not accidentally removed by linker from ELF file.
 */
#if SUPPORT_SEGGER_OPEN_ERASE
int SEGGER_OPEN_Erase(U32 SectorAddr, U32 SectorIndex, U32 NumSectors)
{
  /* This function will still be called every erasing, can't accelerate. */
  int r;
  uint32_t PageError = 0;

  /*Variable used for Erase procedure*/
  FLASH_EraseInitTypeDef EraseInitStruct;

  /* Fill EraseInit structure*/
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_MASSERASE;
  EraseInitStruct.Banks = FLASH_BANK_BOTH;

  if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
  {
    return 1;
  }
  return 0;
}
#endif

/*********************************************************************
 *
 *       SEGGER_OPEN_Start
 *
 *  Function description
 *    Starts the turbo mode of flash algo.
 *    Currently only available for Cortex-M based targets.
 */
#if SUPPORT_TURBO_MODE
void SEGGER_OPEN_Start(volatile struct SEGGER_OPEN_CMD_INFO *pInfo)
{
  SEGGER_OFL_Lib_StartTurbo(&SEGGER_OFL_Api, pInfo);
}
#endif

/**************************** End of file ***************************/
