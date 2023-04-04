//--------------------------------------------------------------------
// TUCN, Computer Science Department
// Input/Output Systems and Peripheral Devices
//--------------------------------------------------------------------
// http://users.utcluj.ro/~baruch/en/pages/teaching/inputoutput-systems/laboratory.php
//--------------------------------------------------------------------
// File:		AppScroll-e.cpp
// Changed:		21.02.2021
//--------------------------------------------------------------------
// IOS application example with vertical scroll bar
//--------------------------------------------------------------------
#include <stdio.h>
#include <windows.h>
#include "Hw.h"
#include "ComDef-e.h"
#include "PCI-e.h"
#include "PCI-vendor-dev.h"
#include "SMBus-e.h"
//#include "SMBus.h"

#define NLIN 500								// number of lines in the display window
#define NCOL 240								// number of columns in the display window
#define SMB_HST_STS 0x00
#define HST_STS 0x00
#define HST_STS_INTR 0x01000000
#define HST_CNT 0x02
#define HST_CMD 0x03
#define XMIT_SLVA 0x04
#define HST_D0 0x05
#define HST_D1 0x06
#define Host_BLOCK_dB 0x07
#define PEC 0x08
#define AUX_STS 0x0c
#define Aux_CTL 0x0d
#define HST_BUSY 0x01
#define SMB_CMD 0x00000100
#define START 0x01000000
#define SEND_REC_BYTE 0x00000100
#define HST_CNT_START 0x01000000
#define HSTS_FAILED 0x00010000
//#define SEND_REC_BYTE 0x00
#define HST_STS_DEV_ERR 0x00000100
#define HST_STS_BUS_ERR 0x00001000
#define HST_STS_HOST_BUSY 0x00000001
#define SMB_WR_RD_BYTE 0x00001000


// Global variables
char szBuffer[NLIN][NCOL];						// buffer for the window contents
int  cLine;										// number of current line in the display buffer

DWORD64 BaseAddress;

// Declarations of external functions
DWORD64 PciBaseAddress();

void DisplWindow(HWND hWnd);

//--------------------------------------------------------------------
// Function AppScroll
//--------------------------------------------------------------------
//
// Function:	IOS application example with vertical scroll bar
//
// Parameters:	hWnd - Handle to the application window
//
// Returns:		0 - Operation completed successfully
//				1 - Error initializing the Hw driver
//
//--------------------------------------------------------------------

PPCI_CONFIG0 getConfig(BYTE busNumber, BYTE deviceNumber, BYTE functionNumber)
{
	DWORD64 addr = 0;
	addr |= BaseAddress;
	//addr <<= 20;
	//busNumber <<= 20;
	//deviceNumber <<= 15;
	//functionNumber <<= 12;
	//addr |= busNumber;
	//addr <<= 15;
	//addr |= deviceNumber;
	//addr <<= 12;
	//addr |= functionNumber;
	addr |= busNumber << 20;
	addr |= deviceNumber << 20;
	addr |= functionNumber << 20;

	return (PPCI_CONFIG0)addr;
}
PCI_CONFIG0* GetConfigurationHeader(BYTE busNumber, BYTE functionNumber, BYTE deviceNumber) {
	DWORD64 address = BaseAddress;
	address = address | busNumber << 20 | deviceNumber << 15 | functionNumber << 12;
	return (PPCI_CONFIG0)address;
}

DWORD GetPCIDevice(BYTE classCode, BYTE subclassCode) {

	for (int bus = 0; bus < 64; bus++) {
		for (int device = 0; device < 32; device++) {
			for (int function = 0; function < 8; function++) {
				PPCI_CONFIG0 config = GetConfigurationHeader(bus, function, device);
				WORD wVendorID = _inmw((DWORD_PTR)&config->VendorID);
				if (wVendorID != 0xFFFF) {
					BYTE classC = _inm((DWORD_PTR)&config->BaseClass);
					BYTE subClass = _inm((DWORD_PTR)&config->SubClass);
					if ((classC == classCode) && (subClass == subclassCode)) {


						DWORD result = 0;
						result |= bus;
						result |= (device << 8);
						result |= (function << 16);
						return result;
					}
				}
			}
		}
	}
	wsprintf(szBuffer[cLine++], "Device not found!");
	return 0;
}

DWORD smbus_base_address;

WORD GetSMBusBaseAddress(PPCI_CONFIG0 config)  //problem 3.8.2
{
	PSMBUS_CFG smbus_config = (PSMBUS_CFG)config; //we get the smbus address
	DWORD base_address = _inmdw((DWORD_PTR)&smbus_config->SMB_BASE) & 0xfffe; //? 

	return LOWORD(base_address); //need loword because we are working with WORD instead of dword
}

BYTE SendReceiveByte(HWND hWnd, BYTE deviceAddress) //3.8.4 - implement receive byte command
{
	BYTE status = 0x0; //for now initialize it 

	while ((__inp(BaseAddress + HST_STS_INTR) & HST_BUSY) != 0);

	BYTE data = (deviceAddress << 1) | 1;

	__outp(BaseAddress + XMIT_SLVA, data);// transmit slave address register is written with slave device address and direction of transfer register :D
	//write command code and the start
	__outp(BaseAddress + HST_CNT, SMB_CMD | START); //i think?

	return status;
}

int receiveByte(WORD baseAddress, BYTE deviceAddress)
{
	while ((__inp(baseAddress + SMB_HST_STS) & HST_STS_HOST_BUSY) != 0);

	__outp(baseAddress + XMIT_SLVA, ((deviceAddress << 1) | 0x01));

	__outp(baseAddress + HST_CNT, SEND_REC_BYTE | HST_CNT_START);

	int flag = 1;
	int count = 10000;
	while ((count--) > 0)
	{
		BYTE hsr = __inp(baseAddress + SMB_HST_STS);
		if ((hsr & (HST_STS_INTR)) != 0)
		{
			flag = 0;
			__outp((baseAddress + SMB_HST_STS), __inp(baseAddress + SMB_HST_STS));
			break;
		}

		if ((hsr & (HSTS_FAILED | HST_STS_BUS_ERR | HST_STS_DEV_ERR)) != 0)
		{
			flag = 2;
			__outp((baseAddress + SMB_HST_STS), __inp(baseAddress + SMB_HST_STS));
			break;
		}
	}

	return flag;
}

int readByte(WORD baseAddress, BYTE deviceAddress, BYTE command)
{
	while ((__inp(baseAddress + SMB_HST_STS) & HST_STS_HOST_BUSY) != 0);

	__outp(baseAddress + XMIT_SLVA, ((deviceAddress << 1) | 0x01));

	__outp(baseAddress + HST_CNT, SMB_WR_RD_BYTE | HST_CNT_START);

	__outp(baseAddress + HST_CMD, command);

	int flag = 1;

	while (1)
	{
		BYTE hsr = __inp(baseAddress + SMB_HST_STS);
		if ((hsr & (HST_STS_INTR)) != 0)
		{
			flag = 0;
			__outp((baseAddress + SMB_HST_STS), __inp(baseAddress + SMB_HST_STS));
			break;
		}

		if ((hsr & (HSTS_FAILED | HST_STS_BUS_ERR | HST_STS_DEV_ERR)) != 0)
		{
			flag = 2;
			__outp((baseAddress + SMB_HST_STS), __inp(baseAddress + SMB_HST_STS));
			break;
		}
	}

	return flag;
}


BYTE sendReceiveByte(WORD baseAddress, BYTE deviceAddress)
{
	BYTE status;
	while ((__inp(baseAddress + SMB_HST_STS) & HST_BUSY) != 0);

	BYTE data = (deviceAddress << 1) | 1;
	__outp(baseAddress + XMIT_SLVA, data);
	__outp(baseAddress + HST_CNT, SEND_REC_BYTE | HST_CNT_START);

	while (1)
	{
		status = __inp(baseAddress + SMB_HST_STS);
		if ((status & HST_STS_INTR) != 0)
		{
			status = __inp(baseAddress + SMB_HST_STS);
			__outp(baseAddress + SMB_HST_STS, status);
			return 0;
			break;
		}

		if (((status & HSTS_FAILED) != 0) || ((status & HST_STS_DEV_ERR) != 0) || ((status & HST_STS_BUS_ERR) != 0))
		{
			status = __inp(baseAddress);
		}
		else
		{
			return 1;
		}
	}
}

int AppScroll(HWND hWnd)
{
	DWORD dwLastError;							// code of last error
	int   i;
	//HANDLE hFile;

	char szMes0[] = "Error initializing the Hw driver";
	char szMes1[] = "IOS Application";
	//char szMes2[] = "Error opening the COM1 port\nCode: 0x%08X";

	// Initialize the Hw library
	if (!HwOpen()) {
		wsprintf(szBuffer[0], szMes0);
		MessageBox(NULL, szBuffer[0], "HwOpen", MB_ICONSTOP);
		return 1;
	}

	// Clear the display buffer and the window contents
	for (i = 0; i < NLIN; i++) {
		memset(szBuffer[i], ' ', NCOL);
	}
	cLine = 1;

	// Copy the start message into the display buffer and display the message
	wsprintf(szBuffer[cLine], szMes1);
	cLine += 2;
	DisplWindow(hWnd);

	//--------------------------------------------------------------------
	// To be completed with the application code
	//--------------------------------------------------------------------

	BaseAddress = PciBaseAddress();

	if (BaseAddress == 0 || BaseAddress == 1) {
		char errorMessage[] = "The base address could not be found";
		wsprintf(szBuffer[cLine++], errorMessage);
	}
	else {
		DWORD baseAddress1 = (DWORD)BaseAddress;
		DWORD baseAddress2 = BaseAddress >> 32;
		wsprintf(szBuffer[cLine++], "%X", baseAddress1);
		wsprintf(szBuffer[cLine++], "%X", baseAddress2);
	}

	DWORD pciDevice = GetPCIDevice(0x0C, 0x05);
	BYTE bus = pciDevice & 0xFF;
	BYTE device = ((pciDevice >> 8)) & 0xFF;
	BYTE function = ((pciDevice >> 16)) & 0xFF;
	wsprintf(szBuffer[cLine++], "Device found (bus,device, function) : (%d , %d, %d)", bus, device, function);
	PPCI_CONFIG0 config = GetConfigurationHeader(bus, function, device);

	smbus_base_address = GetSMBusBaseAddress(config);

	wsprintf(szBuffer[cLine++], "SMBus base address is %X", smbus_base_address);
	// Display the messages
	cLine++;
	cLine++;

	for (BYTE x = 0x10; x < 0x7F; x++) {
		int flag = receiveByte(smbus_base_address, x);
		char devType[1024];
		strcpy_s(devType, "N/A");

		//wsprintf(szBuffer[cLine++], "[3.8.4] Transfer initiated ... ");
		if (flag == 0) {

			//wsprintf(szBuffer[cLine++], "[3.8.4] Transfer initiated ... ");
			if (x > 0x18 && x < 0x1F) {
				strcpy_s(devType, "Thermal sensors of an SPD memory");
			}
			if (x > 0x30 && x < 0x37) {
				strcpy_s(devType, "Write protection of an SPD memory");
			}
			if (x > 0x40 && x < 0x47) {
				strcpy_s(devType, "Real-time clock");
			}
			if (x > 0x50 && x < 0x57) {
				strcpy_s(devType, "SPD Memory");
			}
			wsprintf(szBuffer[cLine++], "The transfer completed successfully on: %x, %s", x, devType);
		}
		else if (flag == 1) {
			wsprintf(szBuffer[cLine++], "The transfer timed out!");
		}
		else {
			//wsprintf(szBuffer[cLine++], "The transfer encountered an error!");
		}
	}

	// Display the messages
	DisplWindow(hWnd);

	HwClose();
	return 0;
}