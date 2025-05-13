//
// usbcdgadget.cpp
// 
// CDROM Gadget by Ian Cass, heavily based on
// USB Mass Storage Gadget by Mike Messinides
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2023-2024  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <circle/usb/gadget/usbcdgadget.h>
#include <circle/usb/gadget/usbcdgadgetendpoint.h>
#include <circle/logger.h>
#include <circle/sysconfig.h>
#include <circle/util.h>
#include <assert.h>
#include <stddef.h>
#include <math.h>

#define MLOGNOTE(From,...)		CLogger::Get ()->Write (From, LogNotice, __VA_ARGS__)
#define MLOGDEBUG(From,...)		//CLogger::Get ()->Write (From, LogDebug, __VA_ARGS__)
#define MLOGERR(From,...)		CLogger::Get ()->Write (From, LogError,__VA_ARGS__)
#define DEFAULT_BLOCKS 16000

const TUSBDeviceDescriptor CUSBCDGadget::s_DeviceDescriptor =
{
	sizeof (TUSBDeviceDescriptor),
	DESCRIPTOR_DEVICE,
	0x200,				// bcdUSB
	0,               //bDeviceClass
	0,              //bDeviceSubClass
	0,              //bDeviceProtocol
	64,				// wMaxPacketSize0
	0x04da, // Panasonic
	0x0d01,	// CDROM
	//USB_GADGET_VENDOR_ID,
	//USB_GADGET_DEVICE_ID_CD,
	0x100,				// bcdDevice
	1, 2, 0,			// strings
	1                   //num configurations
};

const CUSBCDGadget::TUSBMSTGadgetConfigurationDescriptor CUSBCDGadget::s_ConfigurationDescriptor =
{
	{
		sizeof (TUSBConfigurationDescriptor),
		DESCRIPTOR_CONFIGURATION,
		sizeof s_ConfigurationDescriptor,
		1,			// bNumInterfaces
		1,
		0,
		0x80,			// bmAttributes (bus-powered)
		500 / 2			// bMaxPower (500mA)
	},
	{
		sizeof (TUSBInterfaceDescriptor),
		DESCRIPTOR_INTERFACE,
		0,			// bInterfaceNumber
		0,			// bAlternateSetting
		2,			// bNumEndpoints
		0x08, 0x05, 0x50,	// bInterfaceClass, SubClass, Protocol
		0			// iInterface
	},
	{
		sizeof (TUSBEndpointDescriptor),
		DESCRIPTOR_ENDPOINT,
		0x81, 			//IN number 1
		2,			// bmAttributes (Bulk)
		512,			// wMaxPacketSize
		0			// bInterval
	},
	{
		sizeof (TUSBEndpointDescriptor),
		DESCRIPTOR_ENDPOINT,
		0x02, 			//OUT number 2
		2,			// bmAttributes (Bulk)
		512,			// wMaxPacketSize
		0   			// bInterval
	}
};

const char *const CUSBCDGadget::s_StringDescriptor[] =
{
	"\x04\x03\x09\x04",		// Language ID
	"Circle",
	"CDROM Gadget"
};

CUSBCDGadget::CUSBCDGadget (CInterruptSystem *pInterruptSystem, CDevice *pDevice)
:	CDWUSBGadget (pInterruptSystem, HighSpeed),
	m_pDevice (pDevice),
	m_pEP {nullptr, nullptr, nullptr}
{
	MLOGNOTE ("CUSBCDGadget::CUSBCDGadget", "entered");
	if(pDevice)
		SetDevice(pDevice);
}

CUSBCDGadget::~CUSBCDGadget (void)
{
	assert (0);
}

const void *CUSBCDGadget::GetDescriptor (u16 wValue, u16 wIndex, size_t *pLength)
{
	MLOGNOTE ("CUSBCDGadget::GetDescriptor", "entered");
	assert (pLength);

	u8 uchDescIndex = wValue & 0xFF;

	switch (wValue >> 8)
	{
	case DESCRIPTOR_DEVICE:
	        MLOGNOTE ("CUSBCDGadget::GetDescriptor", "DESCRIPTOR_DEVICE %02x", uchDescIndex);
		if (!uchDescIndex)
		{
			*pLength = sizeof s_DeviceDescriptor;
			return &s_DeviceDescriptor;
		}
		break;

	case DESCRIPTOR_CONFIGURATION:
	        MLOGNOTE ("CUSBCDGadget::GetDescriptor", "DESCRIPTOR_CONFIGURATION %02x", uchDescIndex);
		if (!uchDescIndex)
		{
			*pLength = sizeof s_ConfigurationDescriptor;
			return &s_ConfigurationDescriptor;
		}
		break;

	case DESCRIPTOR_STRING:
	        MLOGNOTE ("CUSBCDGadget::GetDescriptor", "DESCRIPTOR_STRING %02x", uchDescIndex);
		if (!uchDescIndex)
		{
			*pLength = (u8) s_StringDescriptor[0][0];
			return s_StringDescriptor[0];
		}
		else if (uchDescIndex < sizeof s_StringDescriptor / sizeof s_StringDescriptor[0])
		{
			return ToStringDescriptor (s_StringDescriptor[uchDescIndex], pLength);
		}
		break;

	default:
		break;
	}

	return nullptr;
}

void CUSBCDGadget::AddEndpoints (void)
{

	MLOGNOTE ("CUSBCDGadget::AddEndpoints", "entered");
	assert (!m_pEP[EPOut]);
	m_pEP[EPOut] = new CUSBCDGadgetEndpoint (
				reinterpret_cast<const TUSBEndpointDescriptor *> (
					&s_ConfigurationDescriptor.EndpointOut), this);
	assert (m_pEP[EPOut]);

	assert (!m_pEP[EPIn]);
	m_pEP[EPIn] = new CUSBCDGadgetEndpoint (
				reinterpret_cast<const TUSBEndpointDescriptor *> (
					&s_ConfigurationDescriptor.EndpointIn), this);
	assert (m_pEP[EPIn]);

	m_nState=TCDState::Init;
}

//must set device before usb activation
void CUSBCDGadget::SetDevice (CDevice* dev)
{
	MLOGNOTE ("CUSBCDGadget::SetDevice", "entered");
	m_pDevice=dev;
	u64 devSize=dev->GetSize();
	if(devSize==(u64)-1)MLOGERR("SetDevice","Device size not reported");
	u64 blocks = devSize/BLOCK_SIZE;
	MLOGNOTE("CUSBCDGadget::SetDevice","Size is %d, Blocks are %d", devSize, blocks);
	InitDeviceSize(blocks);
}

void CUSBCDGadget::InitDeviceSize(u64 blocks)
{
	MLOGNOTE ("CUSBCDGadget::InitDevice", "entered");
	u32 lastBlock=blocks-1;//address of last block
	m_nDeviceBlocks=blocks;
	m_ReadCapReply.nLastBlockAddr = htonl(lastBlock);
	m_FormatCapReply.numBlocks = htonl(blocks);
        m_DiscInfoReply.last_possible_lead_out = htonl(blocks);
	m_CDReady = true;
}


u64 CUSBCDGadget::GetBlocks (void) const
{
	MLOGNOTE ("CUSBCDGadget::GetBlocks", "entered");
	return m_nDeviceBlocks;
}

//use when device does not report size
void CUSBCDGadget::SetDeviceBlocks(u64 numBlocks)
{
	MLOGNOTE ("CUSBCDGadget::SetDeviceBlocks", "entered");
	InitDeviceSize(numBlocks);
}

void CUSBCDGadget::CreateDevice (void)
{
	MLOGNOTE ("CUSBCDGadget::GetDescriptor", "entered");
	assert(m_pDevice);
}

void CUSBCDGadget::OnSuspend (void)
{

	MLOGNOTE ("CUSBCDGadget::OnSuspend", "entered");
	delete m_pEP[EPOut];
	m_pEP[EPOut] = nullptr;

	delete m_pEP[EPIn];
	m_pEP[EPIn] = nullptr;

	m_nState=TCDState::Init;
}

const void *CUSBCDGadget::ToStringDescriptor (const char *pString, size_t *pLength)
{
	MLOGNOTE ("CUSBCDGadget::ToStringDescriptor", "entered");
	assert (pString);

	size_t nLength = 2;
	for (u8 *p = m_StringDescriptorBuffer+2; *pString; pString++)
	{
		assert (nLength < sizeof m_StringDescriptorBuffer-1);

		*p++ = (u8) *pString;		// convert to UTF-16
		*p++ = '\0';

		nLength += 2;
	}

	m_StringDescriptorBuffer[0] = (u8) nLength;
	m_StringDescriptorBuffer[1] = DESCRIPTOR_STRING;

	assert (pLength);
	*pLength = nLength;

	return m_StringDescriptorBuffer;
}

int CUSBCDGadget::OnClassOrVendorRequest (const TSetupData *pSetupData, u8 *pData)
{
	MLOGNOTE ("CUSBCDGadget::OnClassOrVendorRequest", "entered");
	if(pSetupData->bmRequestType==0xA1 && pSetupData->bRequest==0xfe) //get max LUN
	{
		MLOGDEBUG("OnClassOrVendorRequest", "state = %i",m_nState);
		pData[0]=0;
		return 1;
	}
	return -1;
}

void CUSBCDGadget::OnTransferComplete (boolean bIn, size_t nLength)
{
	//MLOGNOTE("OnXferComplete", "state = %i, dir = %s, len=%i ",m_nState,bIn?"IN":"OUT",nLength);
	assert(m_nState != TCDState::Init);
	if(bIn) //packet to host has been transferred
	{
		switch(m_nState)
		{
		case TCDState::SentCSW:
			{
				m_nState=TCDState::ReceiveCBW;
				m_pEP[EPOut]->BeginTransfer(CUSBCDGadgetEndpoint::TransferCBWOut,
				                            m_OutBuffer,SIZE_CBW);
				break;
			}
		case TCDState::DataIn:
			{
				if(m_nnumber_blocks>0)
				{
					if(m_CDReady)
					{
						m_nState=TCDState::DataInRead; //see Update function
					}
					else
					{
						MLOGERR("onXferCmplt DataIn","failed, %s",
						        m_CDReady?"ready":"not ready");
						m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
						m_ReqSenseReply.bSenseKey = 2;
						m_ReqSenseReply.bAddlSenseCode = 1;
						SendCSW();
					}
				}
				else     //done sending data to host
				{
					SendCSW();
				}
				break;
			}
		case TCDState::SendReqSenseReply:
			{
				SendCSW();
				break;
			}
		default:
			{
				MLOGERR("onXferCmplt","dir=in, unhandled state = %i", m_nState);
				assert(0);
				break;
			}
		}
	}
	else    //packet from host is available in m_OutBuffer
	{
		switch(m_nState)
		{
		case TCDState::ReceiveCBW:
			{
				if(nLength != SIZE_CBW)
				{
					MLOGERR("ReceiveCBW","Invalid CBW len = %i",nLength);
					m_pEP[EPIn]->StallRequest(true);
					break;
				}
				memcpy(&m_CBW,m_OutBuffer,SIZE_CBW);
				if(m_CBW.dCBWSignature != VALID_CBW_SIG)
				{
					MLOGERR("ReceiveCBW","Invalid CBW sig = 0x%x",
						m_CBW.dCBWSignature);
					m_pEP[EPIn]->StallRequest(true);
					break;
				}
				m_CSW.dCSWTag=m_CBW.dCBWTag;
				if(m_CBW.bCBWCBLength<=16 && m_CBW.bCBWLUN==0) //meaningful CBW
				{
					HandleSCSICommand(); //will update m_nstate
					break;
				} // TODO: response for not meaningful CBW
				break;
			}
		case TCDState::DataOut:
			{
				//process block from host
				assert(m_nnumber_blocks>0);
				if(m_CDReady)
				{
					m_nState=TCDState::DataOutWrite; //see Update function
				}
				else
				{
					MLOGERR("onXferCmplt DataOut","failed, %s",
					        m_CDReady?"ready":"not ready");
					m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
					m_ReqSenseReply.bSenseKey = 2;
					m_ReqSenseReply.bAddlSenseCode = 1;
					SendCSW();
				}
				break;
			}

		default:
			{
				MLOGERR("onXferCmplt","dir=out, unhandled state = %i", m_nState);
				assert(0);
				break;
			}
		}
	}
}

// will be called before vendor request 0xfe
void CUSBCDGadget::OnActivate()
{
	MLOGNOTE("CD OnActivate", "state = %i",m_nState);
	m_CDReady=true;
	m_nState=TCDState::ReceiveCBW;
	m_pEP[EPOut]->BeginTransfer(CUSBCDGadgetEndpoint::TransferCBWOut,m_OutBuffer,SIZE_CBW);
}

void CUSBCDGadget::SendCSW()
{
	//MLOGNOTE ("CUSBCDGadget::SendCSW", "entered");
	memcpy(&m_InBuffer,&m_CSW,SIZE_CSW);
	m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferCSWIn,m_InBuffer,SIZE_CSW);
	m_nState=TCDState::SentCSW;
}

u32 lba_to_msf(u32 lba) {
    lba = lba + 150; // MSF values are offset by 2mins. Weird
    u32 minutes = floor(lba / (75.0 * 60.0));
    u32 seconds = floor((lba - (minutes * 75.0 * 60.0)) / 75.0);
    u32 frames = lba - (minutes * 75 * 60) - (seconds * 75);

    // Clamp values to their valid ranges (though they should be within if LBA is valid)
    minutes = (minutes > 99) ? 99 : minutes;
    seconds = (seconds > 59) ? 59 : seconds;
    frames = (frames > 74) ? 74 : frames;

    // Encode as BCD (Binary Coded Decimal)
    u8 bcd_minutes = ((minutes / 10) << 4) | (minutes % 10);
    u8 bcd_seconds = ((seconds / 10) << 4) | (seconds % 10);
    u8 bcd_frames = ((frames / 10) << 4) | (frames % 10);
    u8 reserved = 0;

    // Combine into a 32-bit integer (MSFR format)
    u32 msfr = (bcd_minutes << 24) | (bcd_seconds << 16) | (bcd_frames << 8) | reserved;

    // Convert to network byte order (Big Endian)
    return msfr;
}

u32 getAddress(u32 lba, int msf) {
	u32 address = lba;
	if (msf)
		address = lba_to_msf(lba);
	return address;
}

void CUSBCDGadget::HandleSCSICommand()
{

	MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "SCSI Command is 0x%02x", m_CBW.CBWCB[0]);
	switch(m_CBW.CBWCB[0])
	{
	case 0x0: // Test unit ready
		{
			m_CSW.bmCSWStatus=m_CDReady?CD_CSW_STATUS_OK:CD_CSW_STATUS_FAIL;
			if(!m_CDReady)
			{
	                        MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Test Unit Ready (returning CD_CSW_STATUS_FAIL)");
				m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
				m_ReqSenseReply.bSenseKey = 2;
				m_ReqSenseReply.bAddlSenseCode = 1;
			}
			else
			{
	                        //MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Test Unit Ready (returning CD_CSW_STATUS_OK)");
				m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
				m_ReqSenseReply.bSenseKey = 0;
				m_ReqSenseReply.bAddlSenseCode = 0;
			}
			SendCSW();
			break;
		}
	case 0x3: // Request sense CMD
		{
	                MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Request Sense CMD");
			memcpy(&m_InBuffer,&m_ReqSenseReply,SIZE_RSR);
                        m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn,
                                                   m_InBuffer,SIZE_RSR);
                        m_nState=TCDState::SendReqSenseReply;
                        m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
                        m_ReqSenseReply.bSenseKey = 0;
                        m_ReqSenseReply.bAddlSenseCode = 0;
			break;
		}
	case 0x12: // Inquiry
		{
	                MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Inquiry %0x", m_CBW.CBWCB[1]);

			if ((m_CBW.CBWCB[1] & 0x01) == 0) { // EVPD bit is 0: Standard Inquiry
	                    MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Inquiry (Standard Enquiry)");
                            memcpy(&m_InBuffer,&m_InqReply,SIZE_INQR);
                            //m_nnumber_blocks=0; //nothing more after this send
                            m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn, m_InBuffer,SIZE_INQR);
                            m_nState=TCDState::DataIn;
                            m_nnumber_blocks=0; //nothing more after this send
                            m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
                            m_ReqSenseReply.bSenseKey = 0;
                            m_ReqSenseReply.bAddlSenseCode = 0;
                        } else { // EVPD bit is 1: VPD Inquiry
                            MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Inquiry (VPD Inquiry)");
                            u8 vpdPageCode = m_CBW.CBWCB[2];
                            switch (vpdPageCode) {
			        case 0x00: // Supported VPD Pages
				    MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Inquiry (Supported VPD Pages)");
				    memcpy(&m_InBuffer, &m_InqVPDReply, SIZE_VPDPAGE);
                                    m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn,
                                                    m_InBuffer, SIZE_VPDPAGE);
                                    m_nState=TCDState::DataIn;
                                    m_nnumber_blocks=0; //nothing more after this send
                                    m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
            	    	            m_ReqSenseReply.bSenseKey = 0;
                                    m_ReqSenseReply.bAddlSenseCode = 0;
				    break;
                                case 0x80: // Unit Serial Number Page
				    MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Inquiry (Unit Serial number Page)");
				    memcpy(&m_InBuffer, &m_InqSerialReply, SIZE_INQSN);
                                    m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn,
                                                    m_InBuffer, SIZE_INQSN);
                                    m_nState=TCDState::DataIn;
                                    m_nnumber_blocks=0; //nothing more after this send
                                    m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
            	    	            m_ReqSenseReply.bSenseKey = 0;
                                    m_ReqSenseReply.bAddlSenseCode = 0;
				    break;
				default: // Unsupported VPD Page
				    MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Inquiry (Unsupported Page)");
                                    m_nState=TCDState::DataIn;
                                    m_nnumber_blocks=0; //nothing more after this send
                                    m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
                                    m_ReqSenseReply.bSenseKey = 0x5; // Illegal/not supported
                                    m_ReqSenseReply.bAddlSenseCode = 0x24;
                                    m_CSW.bmCSWStatus = CD_CSW_STATUS_FAIL;
                                    SendCSW();
				    break;
			    }
			}
			break;
		}
	case 0x1A: // Mode sense (6)
		{
			memcpy(&m_InBuffer,&m_ModeSenseReply,SIZE_MODEREP);
			m_nnumber_blocks=0; //nothing more after this send
			m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn,
			                           m_InBuffer,SIZE_MODEREP);
			m_nState=TCDState::DataIn;
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			break;
		}
	case 0x1B: // Start/stop unit
		{
			m_CDReady = (m_CBW.CBWCB[4] >> 1) == 0;
			MLOGNOTE("HandleSCSI","start/stop, %s",m_CDReady?"ready":"not ready");
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			SendCSW();
			break;
		}
	case 0x1E: // allow removal
		{
			m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
			m_ReqSenseReply.bSenseKey = 0x5; // Illegal/not supported
			m_ReqSenseReply.bAddlSenseCode = 0x20;
			SendCSW();
			break;
		}

	case 0x23: // Format Capacity
		{
			memcpy(&m_InBuffer,&m_FormatCapReply,SIZE_FORMATR);
			m_nnumber_blocks=0; //nothing more after this send
			m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn,
			                           m_InBuffer,SIZE_FORMATR);
			m_nState=TCDState::DataIn;
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			break;
		}
	case 0x25: // Read Capacity (10))
		{
			memcpy(&m_InBuffer,&m_ReadCapReply,SIZE_READCAPREP);
			m_nnumber_blocks=0; //nothing more after this send
			m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn,
			                           m_InBuffer,SIZE_READCAPREP);
			m_nState=TCDState::DataIn;
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			break;
		}
	case 0x28: // Read (10)
		{
			if(m_CDReady)
			{
	                        //MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Read (10)");
				//will be updated if read fails on any block
				m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;

				m_ReqSenseReply.bSenseKey = 0;
				m_ReqSenseReply.bAddlSenseCode = 0;
				m_nnumber_blocks = (u32)((m_CBW.CBWCB[7] << 8) | m_CBW.CBWCB[8]);
				m_nblock_address =   (u32)(m_CBW.CBWCB[2] << 24)
				                   | (u32)(m_CBW.CBWCB[3] << 16)
				                   | (u32)(m_CBW.CBWCB[4] << 8) | m_CBW.CBWCB[5];
				m_nbyteCount=m_CBW.dCBWDataTransferLength;
				if(m_nnumber_blocks==0)
				{
					m_nnumber_blocks=1+(m_nbyteCount)/BLOCK_SIZE;
				}
				m_nState=TCDState::DataInRead; //see Update() function
			}
			else
			{
				MLOGNOTE("handleSCSI Read(10)","failed, %s", m_CDReady?"ready":"not ready");
				m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
				m_ReqSenseReply.bSenseKey = 2;
				m_ReqSenseReply.bAddlSenseCode = 1;
				SendCSW();
			}
			break;
		}

	case 0x2A: // Write (10)
		{
			if(m_CDReady)
			{
				//->big endian
				m_nnumber_blocks = (u32)(m_CBW.CBWCB[7] << 8) | m_CBW.CBWCB[8];
				m_nblock_address = (u32)(m_CBW.CBWCB[2] << 24) | (u32)(m_CBW.CBWCB[3] << 16)
				                   |(u32)(m_CBW.CBWCB[4] << 8) | m_CBW.CBWCB[5];
				MLOGDEBUG("Write(10)","addr = %u len = %u",m_nblock_address,m_nnumber_blocks);
				m_pEP[EPOut]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataOut,
				                            m_OutBuffer,512);
				m_nState=TCDState::DataOut;
				m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;	   //will be updated if write fails
				m_ReqSenseReply.bSenseKey = 0;
				m_ReqSenseReply.bAddlSenseCode = 0;
			}
			else
			{
				MLOGERR("handleSCSI write(10)","failed, %s",
					m_CDReady?"ready":"not ready");
				m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
				m_ReqSenseReply.bSenseKey = 2;
				m_ReqSenseReply.bAddlSenseCode = 1;
				SendCSW();
			}
			break;
		}

	case 0x2F: // Verify, not implemented but don't tell host
		{
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			SendCSW();
			break;
		}

	case 0x43: // READ TOC/PMA/ATIP
		{
			int msf = m_CBW.CBWCB[1] & 0x02;
			//int format = m_CBW.CBWCB[2] & 0x0f;
			int startingTrack = m_CBW.CBWCB[5];
			int allocationLength = (m_CBW.CBWCB[7] << 8) | m_CBW.CBWCB[8];

	                MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Read TOC with msf = %02x, starting track = %d, allocation length = %d", msf, startingTrack, allocationLength);
			
                        // Calculate Lead-Out LBA
                        int lastDataLBA = m_nDeviceBlocks - 1;
                        u32 leadOutLBA = lastDataLBA + LEADOUT_OFFSET;

			//FIXME this whole section is a hack!
			if (startingTrack == 0 && allocationLength > 12) {
                            // Host wants more than one TOC entry
                            // Simple TOC for now
                            numTracks = 2;

                            int datalen = SIZE_TOC_DATA + (numTracks * SIZE_TOC_ENTRY);

                            TUSBTOCData m_TOCData;
                            m_TOCData.DataLength = htons(datalen - 2);
                            m_TOCData.FirstTrack = 0x01;
                            m_TOCData.LastTrack = numTracks - 1; // no including the leadout
			
                            // Create entry records
                            TUSBTOCEntry entries[2] = {
                                // Track 1
                                {
                                     0x00, // Reserved
                                     0x14, // ADR Control
                                     0x01, // Track Number
                                     0x00, // Reserved
                                     htonl(getAddress(0, msf))
			        },	
                                     // Lead out
                                {
                                     0x00, // Reserved
                                     0x16, // ADR Control
                                     0xAA, // Track Number
                                     0x00, // Reserved
                                     htonl(getAddress(leadOutLBA, msf))
                                }
                            };
			    
                            // Copy the TOC header
                            memcpy(m_InBuffer, &m_TOCData, SIZE_TOC_DATA);

                            // Copy the TOC entries immediately after the header
                            memcpy(m_InBuffer + SIZE_TOC_DATA, entries, numTracks * SIZE_TOC_ENTRY);

                            m_nnumber_blocks=0; //nothing more after this send
                            m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn, m_InBuffer, datalen);
                            m_nState=TCDState::DataIn;
                            m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
                            m_ReqSenseReply.bSenseKey = 0;
                            m_ReqSenseReply.bAddlSenseCode = 0;
			
			} else {
			    // Host wants one track
			    numTracks = 1;
                            int datalen = SIZE_TOC_DATA + (numTracks * SIZE_TOC_ENTRY);
			    
			    // Static for now
                            TUSBTOCData m_TOCData;
                            m_TOCData.DataLength = htons(datalen - 2);
                            m_TOCData.FirstTrack = 0x01;
                            m_TOCData.LastTrack = numTracks;

                            TUSBTOCEntry entry[1];
			    if (startingTrack == 0xAA) {
				//Lead Out Track
				entry[0] = {
                                     0x00, // Reserved
                                     0x16, // ADR Control
                                     0xAA, // Track Number
                                     0x00, // Reserved
                                     htonl(getAddress(leadOutLBA, msf))
                                };
			    } else {
				//FIXME: improve this hack!
				entry[0] = {
                                     0x00, // Reserved
                                     0x14, // ADR Control
                                     0x01, // Track Number
                                     0x00, // Reserved
                                     htonl(getAddress(0, msf))
                                };
			    }
			    
                            // Copy the TOC header
                            memcpy(m_InBuffer, &m_TOCData, SIZE_TOC_DATA);

                            // Copy the TOC entries immediately after the header
                            memcpy(m_InBuffer + SIZE_TOC_DATA, entry, numTracks * SIZE_TOC_ENTRY);

                            m_nnumber_blocks=0; //nothing more after this send
                            m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn, m_InBuffer, datalen);
                            m_nState=TCDState::DataIn;
                            m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
                            m_ReqSenseReply.bSenseKey = 0;
                            m_ReqSenseReply.bAddlSenseCode = 0;
			}
			
			break;
                 }

        case 0x4A: // GET EVENT STATUS NOTIFICATION
		{
                        MLOGNOTE("CUSBCDGadget::HandleSCSICommand", "Get Event Status Notification");

			memcpy(m_InBuffer, &m_EventStatusReply, SIZE_EVENT_STATUS_REPLY);
			m_nnumber_blocks=0; //nothing more after this send
			m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn, m_InBuffer, SIZE_EVENT_STATUS_REPLY);
			m_nState=TCDState::DataIn;
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			break;
		}

        case 0x51: // READ DISC INFORMATION CMD
		{
                        MLOGNOTE("CUSBCDGadget::HandleSCSICommand", "Read Disc Information");

			//TODO: static for now
			memcpy(m_InBuffer, &m_DiscInfoReply, SIZE_DISC_INFO_REPLY);
			m_nnumber_blocks=0; //nothing more after this send
			m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn, m_InBuffer, SIZE_DISC_INFO_REPLY);
			m_nState=TCDState::DataIn;
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			break;
		}

        case 0x46: // Get Configuration
		{

			//int rt = m_CBW.CBWCB[1] & 0x03;
			//int feature = (m_CBW.CBWCB[2] << 8) | m_CBW.CBWCB[3];
                       	//MLOGNOTE("CUSBCDGadget::HandleSCSICommand", "Get Configuration with rt = %d", rt);
                       	MLOGNOTE("CUSBCDGadget::HandleSCSICommand", "Get Configuration");

			/*
			// TODO FIXME
			switch (rt) {
				case 0x00:
					// The Drive shall return the Feature Header and all Feature Descriptors supported by the Drive without regard to currency. 
					memcpy(m_InBuffer, &m_GetConfigurationReply, SIZE_GET_CONFIGURATION_REPLY);
					break;
				case 0x01:
					// The Drive shall return the Feature Header and only those Feature Descriptors in which the Current bit is set to one. 
					memcpy(m_InBuffer, &m_GetConfigurationReply, SIZE_GET_CONFIGURATION_REPLY);
					break;
				case 0x10:
					// The Feature Header and the Feature Descriptor identified by Starting Feature Number shall be returned. 
					// If the Drive does not support the specified feature, only the Feature Header shall be returned. 
					memcpy(m_InBuffer, &m_GetConfigurationReply, SIZE_GET_CONFIGURATION_REPLY);
					break;
				default:
					memcpy(m_InBuffer, &m_GetConfigurationReply, SIZE_GET_CONFIGURATION_REPLY);
					break;

			}
			*/

			memcpy(m_InBuffer, &m_GetConfigurationReply, SIZE_GET_CONFIGURATION_REPLY);
			//m_nnumber_blocks=0; //nothing more after this send
			m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn, m_InBuffer, SIZE_GET_CONFIGURATION_REPLY);
			m_nState=TCDState::DataIn;
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			break;
		}

        case 0x55: // Mode Select (10)
		{
                        MLOGNOTE("CUSBCDGadget::HandleSCSICommand", "Mode Select (10)");

			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			break;
		}
        case 0x5a: // Mode Sense (10)
		{
                        MLOGNOTE("CUSBCDGadget::HandleSCSICommand", "Mode Sense (10)");

			//TODO: static for now
			memcpy(m_InBuffer, &m_ModeSense10Reply, SIZE_MODE_SENSE10_REPLY);
			m_nnumber_blocks=0; //nothing more after this send
			m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn, m_InBuffer, SIZE_MODE_SENSE10_REPLY);
			m_nState=TCDState::DataIn;
			m_CSW.bmCSWStatus=CD_CSW_STATUS_OK;
			m_ReqSenseReply.bSenseKey = 0;
			m_ReqSenseReply.bAddlSenseCode = 0;
			break;
		}

	default:
		{
	                MLOGNOTE ("CUSBCDGadget::HandleSCSICommand", "Unknown SCSI Command is 0x%02x", m_CBW.CBWCB[0]);
			m_ReqSenseReply.bSenseKey = 0x5; // Illegal/not supported
			m_ReqSenseReply.bAddlSenseCode = 0x20;
			m_CSW.bmCSWStatus = CD_CSW_STATUS_FAIL;
			SendCSW();
			break;
		}

	}
}

//this function is called periodically from task level for IO
//(IO must not be attempted in functions called from IRQ)
void CUSBCDGadget::Update()
{
	//MLOGNOTE ("CUSBCDGadget::Update", "entered");
	switch(m_nState)
	{
	case TCDState::DataInRead:
		{
			u64 offset=0;
			int readCount=0;
			if(m_CDReady)
			{
				offset=m_pDevice->Seek(BLOCK_SIZE*m_nblock_address);
				if(offset!=(u64)(-1))
				{
					readCount=m_pDevice->Read(m_InBuffer,BLOCK_SIZE);
					if(readCount < BLOCK_SIZE)
					{
						m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
						m_ReqSenseReply.bSenseKey = 2;
						m_ReqSenseReply.bAddlSenseCode = 1;
						SendCSW();
						break;
					}
					m_nnumber_blocks--;
					m_nblock_address++;
					m_nbyteCount-=readCount;
					m_nState=TCDState::DataIn;
					m_pEP[EPIn]->BeginTransfer(CUSBCDGadgetEndpoint::TransferDataIn,
					                           m_InBuffer,readCount);
				}
			}
			if(!m_CDReady || offset==(u64)(-1))
			{
				MLOGERR("UpdateRead","failed, %s, offset=%i",
				        m_CDReady?"ready":"not ready",offset);
				m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
				m_ReqSenseReply.bSenseKey = 2;
				m_ReqSenseReply.bAddlSenseCode = 1;
				SendCSW();
			}
			break;
		}
	case TCDState::DataOutWrite:
		{
			//process block from host
			assert(m_nnumber_blocks>0);
			u64 offset=0;
			int writeCount=0;
			if(m_CDReady)
			{
				offset=m_pDevice->Seek(BLOCK_SIZE*m_nblock_address);
				if(offset!=(u64)(-1))
				{
					writeCount=m_pDevice->Write(m_OutBuffer,BLOCK_SIZE);
				}
				if(writeCount>0)
				{
					if(writeCount<BLOCK_SIZE)
					{
						MLOGERR("UpdateWrite","writeCount = %u ",writeCount);
						m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
						m_ReqSenseReply.bSenseKey = 0x2;
						m_ReqSenseReply.bAddlSenseCode = 0x1;
						SendCSW();
						break;
					}
					m_nnumber_blocks--;
					m_nblock_address++;
					if(m_nnumber_blocks==0)  //done receiving data from host
					{
						SendCSW();
						break;
					}
				}
			}
			if(!m_CDReady || offset==(u64)(-1) || writeCount<=0)
			{
				MLOGERR("UpdateWrite","failed, %s, offset=%i, writeCount=%i",
				        m_CDReady?"ready":"not ready",offset,writeCount);
				m_CSW.bmCSWStatus=CD_CSW_STATUS_FAIL;
				m_ReqSenseReply.bSenseKey = 2;
				m_ReqSenseReply.bAddlSenseCode = 1;
				SendCSW();
				break;
			}
			else
			{
				if(m_nnumber_blocks>0)  //get next block
				{
					m_pEP[EPOut]->BeginTransfer(
						CUSBCDGadgetEndpoint::TransferDataOut,
					        m_OutBuffer,BLOCK_SIZE);
					m_nState=TCDState::DataOut;
				}
			}
			break;
		}
	default:
		break;
	}
}
