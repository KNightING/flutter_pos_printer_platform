#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>
#include <fstream>

#include "printer.h"
#include "utils.hpp"

HANDLE PrintManager::_hPrinter;

std::vector<Printer> PrintManager::listPrinters()
{
    LPTSTR defaultPrinter;
    DWORD size = 0;
    GetDefaultPrinter(nullptr, &size);

    defaultPrinter = static_cast<LPTSTR>(malloc(size * sizeof(TCHAR)));
    if (!GetDefaultPrinter(defaultPrinter, &size))
    {
        size = 0;
    }

    auto printers = std::vector<Printer>{};

    DWORD needed = 0,
          returned = 0,
          flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;

    EnumPrintersW(flags, nullptr, 2, nullptr, 0, &needed, &returned);

    auto buffer = (PRINTER_INFO_2 *)malloc(needed);
    if (!buffer)
    {
        return printers;
    }

    auto result = EnumPrintersW(flags, nullptr, 2, (LPBYTE)buffer, needed, &needed, &returned);
    if (!result)
    {
        free(buffer);
        return printers;
    }

    for (DWORD i = 0; i < returned; i++)
    {
         // Get the printer status
                            HANDLE hPrinter = NULL;
                            bool work_attributes = false;
                            if (OpenPrinterW(buffer[i].pPrinterName, &hPrinter, NULL))
                            {
                                DWORD dwBytesNeeded = 0;
                                GetPrinterW(hPrinter, 2, NULL, 0, &dwBytesNeeded);
                                PRINTER_INFO_2* pPrinterInfo2 = (PRINTER_INFO_2*)malloc(dwBytesNeeded);
                                if (pPrinterInfo2 != NULL)
                                {
                                    if (GetPrinterW(hPrinter, 2, (LPBYTE)pPrinterInfo2, dwBytesNeeded, &dwBytesNeeded))
                                    {
                                       work_attributes = (pPrinterInfo2->Attributes & PRINTER_ATTRIBUTE_WORK_OFFLINE) == 0;
                                        // Check if the printer is offline
                                        //if (pPrinterInfo2->Attributes & PRINTER_ATTRIBUTE_WORK_OFFLINE)
                                        //{
                                        //    printf("Printer %ls is offline.\n", buffer[i].pPrinterName);
                                        //}
                                        //else
                                        //{
                                        //    printf("Printer %ls is online.\n", buffer[i].pPrinterName);
                                        //}
                                    }
                                    free(pPrinterInfo2);
                                }
                                ClosePrinter(hPrinter);
                            }


        printers.push_back(Printer{
            toUtf8(buffer[i].pPrinterName),
            toUtf8(buffer[i].pDriverName),
            size > 0 && _tcsncmp(buffer[i].pPrinterName, defaultPrinter, size) == 0, // if this is the defaultprinter
            work_attributes && (buffer[i].Status &
             (PRINTER_STATUS_NOT_AVAILABLE | PRINTER_STATUS_ERROR |
              PRINTER_STATUS_OFFLINE | PRINTER_STATUS_PAUSED)) == 0});
    }

    free(buffer);
    free(defaultPrinter);
    return printers;
}

void CheckPrinterStatus()
{
    DWORD dwNeeded, dwReturned;
    PRINTER_INFO_2* pPrinterInfo = NULL;

    // Call EnumPrinters to get the list of installed printers
    if (EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &dwNeeded, &dwReturned))
    {
        // Allocate memory for the printer info
        pPrinterInfo = (PRINTER_INFO_2*)malloc(dwNeeded);
        if (pPrinterInfo != NULL)
        {
            // Call EnumPrinters again to get the printer info
            if (EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, (LPBYTE)pPrinterInfo, dwNeeded, &dwNeeded, &dwReturned))
            {
                // Loop through the list of printers and check their status
                for (DWORD i = 0; i < dwReturned; i++)
                {
                    // Get the printer status
                    HANDLE hPrinter = NULL;
                    if (OpenPrinterW(pPrinterInfo[i].pPrinterName, &hPrinter, NULL))
                    {
                        DWORD dwBytesNeeded = 0;
                        GetPrinterW(hPrinter, 2, NULL, 0, &dwBytesNeeded);
                        PRINTER_INFO_2* pPrinterInfo2 = (PRINTER_INFO_2*)malloc(dwBytesNeeded);
                        if (pPrinterInfo2 != NULL)
                        {
                            if (GetPrinterW(hPrinter, 2, (LPBYTE)pPrinterInfo2, dwBytesNeeded, &dwBytesNeeded))
                            {
                                // Check if the printer is offline
                                if (pPrinterInfo2->Attributes & PRINTER_ATTRIBUTE_WORK_OFFLINE)
                                {
                                    printf("Printer %ls is offline.\n", pPrinterInfo[i].pPrinterName);
                                }
                                else
                                {
                                    printf("Printer %ls is online.\n", pPrinterInfo[i].pPrinterName);
                                }
                            }
                            free(pPrinterInfo2);
                        }
                        ClosePrinter(hPrinter);
                    }
                }
            }
            free(pPrinterInfo);
        }
    }
}

BOOL PrintManager::pickPrinter(std::string printerName)
{
    return OpenPrinterW((LPWSTR)fromUtf8(printerName).c_str(), &_hPrinter, NULL);
}

BOOL PrintManager::printBytes(std::vector<uint8_t> data)
{
    BOOL status = false;
    BOOL success = true;
    DOC_INFO_1W docInfo;
    DWORD dwJob = 0;
    DWORD written = 0;

    if (_hPrinter == INVALID_HANDLE_VALUE)
    {
        //  throw std::exception("Printer handle is invalid.");
        success =  false;
    }

    // Fill in default value of the print document
    docInfo.pDocName = L"FeedMe POS Print Job";
    docInfo.pOutputFile = NULL;
    docInfo.pDatatype = L"RAW";

    // Inform the spooler there is a new document
    dwJob = StartDocPrinterW(_hPrinter, 1, (LPBYTE)&docInfo);
    if (dwJob > 0)
    {
        // Start page
        status = StartPagePrinter(_hPrinter);
        if (status)
        {
            // Send data to the printer
            status = WritePrinter(_hPrinter, (LPVOID)std::data(data), (DWORD)data.size(), &written);
            EndPagePrinter(_hPrinter);
        }
        else
        {
            // throw std::exception("StartPagePrinter error.");
            success =  false;
        }
        // Inform the spooler that the document hsa ended
        EndDocPrinter(_hPrinter);
    }
    else
    {
        // throw std::exception("StartDocPrinterW error.");
        success = false;
    }

    // Check if all data are flushed
    if (written != data.size())
    {
        // throw std::exception("Fail to send all bytes");
        success =  false;
    }

    return success;
}

BOOL PrintManager::close()
{
    if (_hPrinter != INVALID_HANDLE_VALUE)
        return ClosePrinter(_hPrinter);

    return false;
}
