/*
 * Tritium MCP2515 CAN Interface
 * Copyright (c) 2010, Tritium Pty Ltd.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *	- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 *	  in the documentation and/or other materials provided with the distribution.
 *	- Neither the name of Tritium Pty Ltd nor the names of its contributors may be used to endorse or promote products
 *	  derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * - Implements the following CAN interface functions
 *	- can_init
 *	- can_transmit
 *	- can_receive
 *	- can_sleep
 *	- can_wake

 * Extended identidier code by MVE has been updated from an earlier mess, and is COMPLETELY UNTESTED!
 */

// Include files
#include <msp430x24x.h>
#include "tri86.h"
#include "can.h"
#include "usci.h"

// Public variables
can_variables			can;

// Private variables
unsigned char 			buffer[16];

/**************************************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************************************/

/*
 * Initialises MCP2515 CAN controller
 *	- Resets MCP2515 via SPI port (switches to config mode, clears errors)
 *	- Changes CLKOUT to /1 rate (16 MHz)
 *	- Sets up bit timing for 500 kbit operation
 *	- Sets up receive filters and masks
 *		- Rx Filter 0 = Driver controls packets (for remote frame requests)
 *		- Rx Filter 1 = Unused
 *		- Rx Filter 2 = Motor controller packets (for rpm, motor current, motor/control temp, DC voltage)
 *		- Rx Filter 3 = Unused
 *		- Rx Filter 4 = Unused
 *		- Rx Filter 5 = Unused
 *		- Rx Mask 0   = Block identifier must match (upper 6 bits)
 *		- Rx Mask 1   = Block identifier must match (upper 6 bits)
 *	- Enables ERROR and RX interrupts on IRQ pin
 *	- Switches to normal (operating) mode
 */
void can_init( void )
{
	// Set up reset and clocking
	can_reset();
	can_mod( CANCTRL, 0x03, 0x00 );			// CANCTRL register, modify lower 2 bits, CLK = /1 = 16 MHz
	
	// Set up bit timing & interrupts
// Bit 7 = SOF (1 = clock out for clock not SOF)
// Bit 6 = WAKFIL (1 = wake filter enabled)
// Bits 5-3 unused
// Bits 2-0: PHSEG2 (1 less than number of Tqs in phase segment 2)
	buffer[0] = 0x45;						// CNF3 register: PHSEG2 = 6Tq, Wakeup filter, CLKOUT = CLK
// Bit 7 = BLTMODE (1 = use PHSEG2 setting)
// Bit 6 = SAM (1 = sample 3 times)
// Bits 5-3 = PHSEG1 (1 less than Tqs in phase segment 1)
// Bits 2-0 = PRSEG (1 less than Tqs in propagation segment)
	buffer[1] = 0xF8;						// CNF2 register: set PHSEG2 in CNF3, Triple sample, PHSEG1= 8Tq, PROP = 1Tq
// Bits 7-6 are SJW (less 1); sets the Synchronisation Jump Width (can safely be set to 0 for crystal oscillators)
// Bits 5-0 are BRP, which is 1 less than the prescaler from oscillator clock to Tq. Set to 0 for 500k bps, to 1 for 250k pbs
	buffer[2] = 0x00;						// CNF1 register: SJW = 1Tq, BRP = 0 (500k bps)
//	buffer[2] = 0x01;						// CNF1 register: SJW = 1Tq, BRP = 1 (250k bps)

	buffer[3] = 0x63;						// CANINTE register: enable WAKE, ERROR, RX0 & RX1 interrupts on IRQ pin
	buffer[4] = 0x00;						// CANINTF register: clear all IRQ flags
	buffer[5] = 0x00;						// EFLG register: clear all user-changable error flags
	can_write( CNF3, &buffer[0], 6);		// Write to registers
	
	// Set up receive filtering & masks
	// RXF0 - Buffer 0
	buffer[ 0] = (unsigned char)(DC_CAN_BASE >> 3);
	buffer[ 1] = (unsigned char)(DC_CAN_BASE << 5);
	buffer[ 2] = 0x00;
	buffer[ 3] = 0x00;
	// RXF1 - Buffer 0
	buffer[ 4] = (unsigned char)(EL_CAN_ID_H >> (2+3));	// Skip 2 extended bits and SID 0-2
	// Want SID2 SID1 SID0 0 EXIDE=1 0 EID17 EID16:
	buffer[ 5] = (unsigned char)(((EL_CAN_ID_H << 3) & 0xE0) | 8 | (EL_CAN_ID_H  & 3));
	buffer[ 6] = (unsigned char)(EL_CAN_ID_L >>  8);
	buffer[ 7] = (unsigned char)(EL_CAN_ID_L);
	// RXF2 - Buffer 1
	buffer[ 8] = (unsigned char)(MC_CAN_BASE >> 3);
	buffer[ 9] = (unsigned char)(MC_CAN_BASE << 5);
	buffer[10] = 0x00;
	buffer[11] = 0x00;
	can_write( RXF0SIDH, &buffer[0], 12 );
	
	// RXF3 - Buffer 1
	buffer[ 0] = 0x00;
	buffer[ 1] = 0x00;
	buffer[ 2] = 0x00;
	buffer[ 3] = 0x00;
	// RXF4 - Buffer 1
	buffer[ 4] = 0x00;
	buffer[ 5] = 0x00;
	buffer[ 6] = 0x00;
	buffer[ 7] = 0x00;
	// RXF5 - Buffer 1
	buffer[ 8] = 0x00;
	buffer[ 9] = 0x00;
	buffer[10] = 0x00;
	buffer[11] = 0x00;
	can_write( RXF3SIDH, &buffer[0], 12 );

	// RXM0 - Buffer 0
	buffer[ 0] = 0xFC;						// Match upper 6 bits of ID - don't care about lower 5 bits (block identifier)
	buffer[ 1] = 0x00;
	buffer[ 2] = 0x00;
	buffer[ 3] = 0x00;
	// RXM1 - Buffer 1
	buffer[ 4] = 0xFC;						// Match upper 6 bits of ID - don't care about lower 5 bits (block identifier)
	buffer[ 5] = 0x00;
	buffer[ 6] = 0x00;
	buffer[ 7] = 0x00;
	can_write( RXM0SIDH, &buffer[0], 8 );
	
	// Switch out of config mode into normal operating mode
	can_mod( CANCTRL, 0xE0, 0x00 );			// CANCTRL register, modify upper 3 bits, mode = Normal
}

/*
 * Receives a CAN message from the MCP2515
 *	- Run this routine when an IRQ is received
 *	- Query the controller to identify the source of the IRQ
 *		- If it was an ERROR IRQ, read & clear the Error Flag register, and return it
 *		- If it was an RX IRQ, read the message and identifier, and return them
 *		- If both, handle the error preferentially to the message
 *	- Clear the appropriate IRQ flag bits
 */
void can_receive( void )
{
	unsigned char flags;
	
	// Read out the interrupt flags register
	can_read( CANINTF, &flags, 1 );
	// Check for errors
	if(( flags & MCP_IRQ_ERR ) != 0x00 ){
		// Read error flags and counters
		can_read( EFLAG, &buffer[0], 1 );
		can_read( TEC, &buffer[1], 2 );
		// Clear error flags
		can_mod( EFLAG, buffer[0], 0x00 );	// Modify (to '0') all bits that were set
		// Return error code, a blank identifier field, and error registers in data field
		can.status = CAN_ERROR;
		can.identifier = 0x0000;
		can.identifier_ext = 0;				// MVE
		can.data.data_u8[0] = flags;		// CANINTF
		can.data.data_u8[1] = buffer[0];	// EFLG
		can.data.data_u8[2] = buffer[1];	// TEC
		can.data.data_u8[3] = buffer[2];	// REC
		// Clear the IRQ flag
		can_mod( CANINTF, MCP_IRQ_ERR, 0x00 );
	}	
	// No error, check for received messages, buffer 0
	else if(( flags & MCP_IRQ_RXB0 ) != 0x00 ){
		// Read in the info, identifier & message data
		can_read( RXB0CTRL, &buffer[0], 14 );
		// Fill out return structure
		// check for Remote Frame requests and indicate the status correctly
		if(( buffer[0] & MCP_RXB0_RTR ) == 0x00 ){
			// We've received a standard data packet
			can.status = CAN_OK;
			// Fill in the data
			can.data.data_u8[0] = buffer[ 6];
			can.data.data_u8[1] = buffer[ 7];
			can.data.data_u8[2] = buffer[ 8];
			can.data.data_u8[3] = buffer[ 9];
			can.data.data_u8[4] = buffer[10];
			can.data.data_u8[5] = buffer[11];
			can.data.data_u8[6] = buffer[12];
			can.data.data_u8[7] = buffer[13];
		}
		else{
			// We've received a remote frame request
			// Data is irrelevant with an RTR
			can.status = CAN_RTR;
		}
		// Fill in the identifier
		// can.identifier: EID4 EID3 .. EID0 SID10 SID9 .. SID0
		//				    15   14  ..  11   10    9       0
		// can.identifier_ext: EID17 EID16 EID15 EID14 EID13 EID12 EID11 .. EID8 EID7 EID6 EID5
		//					    12    11    10     9    8      7     6   ..   3   2   1    0
		//	overall identifier:	28    27    26    25   24     23    22   ..  19  18  17   16
		can.identifier = buffer[1] <<3		// SID 10-3 (bits 7-0 this register) -> bits 10-3
			| buffer[2] >> 5				// SID 2-1 (bits 7-5 this register) -> bits 2-0
			| buffer[4] << 11;				// Result bits 15-11 (EID 4-0, bits 4-0 this register) -> bits 15-11
		can.identifier_ext = buffer[4] >> 5	// Result bits 18-16 (EID 7-5, bits 7-5 this register) -> bits 2-0
			| buffer[3] << 3				// Result bits 26-19 (EID 15-8, bits 7-0 this register) -> bits 10-3
			| (buffer[2] & 3) << 11;		// Result bits 28-27 (EID 17-16, bits 2-0 this register) -> bits 12-11
		// Clear the IRQ flag
		can_mod( CANINTF, MCP_IRQ_RXB0, 0x00 );
	}
	// No error, check for received messages, buffer 1
	else if(( flags & MCP_IRQ_RXB1 ) != 0x00 ){
		// Read in the info, identifier & message data
		can_read( RXB1CTRL, &buffer[0], 14 );
		// Fill out return structure
		// check for Remote Frame requests and indicate the status correctly
		if(( buffer[0] & MCP_RXB1_RTR ) == 0x00 ){
			// We've received a standard data packet
			can.status = CAN_OK;
			// Fill in the data
			can.data.data_u8[0] = buffer[ 6];
			can.data.data_u8[1] = buffer[ 7];
			can.data.data_u8[2] = buffer[ 8];
			can.data.data_u8[3] = buffer[ 9];
			can.data.data_u8[4] = buffer[10];
			can.data.data_u8[5] = buffer[11];
			can.data.data_u8[6] = buffer[12];
			can.data.data_u8[7] = buffer[13];
		}
		else{
			// We've received a remote frame request
			// Data is irrelevant with an RTR
			can.status = CAN_RTR;
		}
		// Fill in the identifier
		can.identifier = buffer[1];
		can.identifier = buffer[1] <<3		// SID 10-3 (bits 7-0 this register) -> bits 10-3
			| buffer[2] >> 5				// SID 2-1 (bits 7-5 this register) -> bits 2-0
			| buffer[4] << 11;				// Result bits 15-11 (EID 4-0, bits 4-0 this register) -> bits 15-11
		can.identifier_ext = buffer[4] >> 5	// Result bits 18-16 (EID 7-5, bits 7-5 this register) -> bits 2-0
			| buffer[3] << 3				// Result bits 26-19 (EID 15-8, bits 7-0 this register) -> bits 10-3
			| (buffer[2] & 3) << 11;		// Result bits 28-27 (EID 17-16, bits 2-0 this register) -> bits 12-11
		// Clear the IRQ flag
		can_mod( CANINTF, MCP_IRQ_RXB1, 0x00 );
	}
	// Check for wakeup events
	else if(( flags & MCP_IRQ_WAKE ) != 0x00 ){
		// Clear the IRQ flag
		can_mod( CANINTF, MCP_IRQ_WAKE, 0x00 );
		// Signal the event
		can.status = CAN_ERROR;
		can.identifier = 0x0002;
		can.identifier_ext = 0;
	}
	// Else, spurious interrupt, signal an error
	else{
		can.status = CAN_ERROR;
		can.identifier = 0x0001;
		can.identifier_ext = 0;
		can.data.data_u8[0] = flags;		// CANINTF
	}
}

/*
 * Transmits a CAN message to the bus
 *	- Accepts identifier and data payload via can_interface structure
 *	- Checks if any mailboxes are free, if not, returns -1 without transmitting packet
 *	- Busy waits while message is sent to CAN controller
 *	- Uses all available transmit buffers (3 available in CAN controller) to maximise throughput
 *	- Only modifies identifier information if it's different from what is already set up in CAN controller
 *	- Assumes constant 8-byte data length value
 */
char can_transmit( void )
{
	static unsigned int buf_ident[3] = {0xFFFF, 0xFFFF, 0xFFFF};		// 16 LSBs of already-used identifiers
	static unsigned int buf_ident_ext[3] = {0xFFFF, 0xFFFF, 0xFFFF};	// MVE: 13 MSBs of already-used identifiers
	
	// Check for busy mailboxes
//	if(( can_read_status() & 0x54 ) == 0x54){
//		return(-1);
//	}
//	else{
		// Fill data into buffer, it's used by any identifier
		// Allow room at the start of the buffer for the identifier info if needed
		buffer[ 5] = can.data.data_u8[0];
		buffer[ 6] = can.data.data_u8[1];
		buffer[ 7] = can.data.data_u8[2];
		buffer[ 8] = can.data.data_u8[3];
		buffer[ 9] = can.data.data_u8[4];
		buffer[10] = can.data.data_u8[5];
		buffer[11] = can.data.data_u8[6];
		buffer[12] = can.data.data_u8[7];

		// Check if the incoming identifier has already been configured in a mailbox
		if( can.identifier == buf_ident[0] && can.identifier_ext == buf_ident_ext[0] ){
			// Mailbox 0 setup matches our new message
			// Write to TX Buffer 0, start at data registers, and initiate transmission
			can_write_tx( 0x01, &buffer[5] );		
			can_rts( 0 );
		}
		else if( can.identifier == buf_ident[1] && can.identifier_ext == buf_ident_ext[1] ){
			// Mailbox 1 setup matches our new message
			// Write to TX Buffer 1, start at data registers, and initiate transmission
			can_write_tx( 0x03, &buffer[5] );		
			can_rts( 1 );
		}
		else if( can.identifier == buf_ident[2] && can.identifier_ext == buf_ident_ext[2] ){
			// Mailbox 2 setup matches our new message
			// Write to TX Buffer 2, start at data registers, and initiate transmission
			can_write_tx( 0x05, &buffer[5] );		
			can_rts( 2 );
		}
		else{
			// No matches in existing mailboxes
			// No mailboxes already configured, so we'll need to load an identifier - set it up
			// can.identifier: EID4 EID3 .. EID0 SID10 SID9 .. SID0
			//				    15   14  ..  11   10    9       0
			// can.identifier_ext: EID17 EID16 EID15 EID14 EID13 EID12 EID11 .. EID8 EID7 EID6 EID5
			//					    12    11    10     9    8      7     6   ..   3   2   1    0
			//	overall identifier:	28    27    26    25   24     23    22   ..  19  18  17   16
			buffer[0] = (unsigned char)(can.identifier >> 3);	// Identifier bits 10-3 (MSBs of standard identifier)
																// So bits 10-3 -> bits 7-0
			buffer[1] = (unsigned char)(can.identifier << 5)	// Identifier bits 2-0 in MSBs
																// So bits 2-1 -> bits 7-5
				| ( (can.identifier_ext != 0 || can.identifier >= 0x8000) << 3)	// set IDE if extended
				| can.identifier_ext >> 11;						// Identifier bits 28 and 27 (EID 18 and 17) in LSBs
																// So bits 28-27 (12-11 in this word) -> bits 1-0
			buffer[2] = can.identifier_ext >> 3;				// EID 15-8 (overall identifier bits 26-19)
																// So bits EID15-8 (10-3 this word) -> 7-0
			buffer[3] = can.identifier >> 11					// Identifier bits 11-15 (EID 0-4)
																// So bits 15-11 this word -> bits 4-0
				| can.identifier_ext << 5;						// Identifier bits 16-18 (EID 5-7)
																// So bits EID7-5 (in bits 2-0 of this word) -> bits 7-5
			buffer[4] = 0x08;									// DLC (Data Length Code) = 8 bytes
			
			// Check if we've got any un-setup mailboxes free and use them
			// Otherwise, find a non-busy mailbox and set it up with our new identifier
			if( buf_ident_ext[0] == 0xFFFF ){			// Mailbox 0 is free
				// Write to TX Buffer 0, start at identifier registers, and initiate transmission
				can_write_tx( 0x00, &buffer[0] );
				can_rts( 0 );
				buf_ident[0] = can.identifier;
				buf_ident_ext[0] = can.identifier_ext;
			}									
			else if( buf_ident_ext[1] == 0xFFFF ){		// Mailbox 1 is free
				// Write to TX Buffer 1, start at identifier registers, and initiate transmission
				can_write_tx( 0x02, &buffer[0] );
				can_rts( 1 );
				buf_ident[1] = can.identifier;
				buf_ident_ext[1] = can.identifier_ext;
			}
			else if( buf_ident_ext[2] == 0xFFFF ){		// Mailbox 2 is free
				// Write to TX Buffer 2, start at identifier registers, and initiate transmission
				can_write_tx( 0x04, &buffer[0] );
				can_rts( 2 );
				buf_ident[2] = can.identifier;
				buf_ident_ext[2] = can.identifier_ext;
			}
			else {					
		
				// No mailboxes free, wait until at least one is not busy
				while(( can_read_status() & 0x54 ) == 0x54);
				// Is it mailbox 0?
				if(( can_read_status() & 0x04 ) == 0x00) {
					// Setup mailbox 0 and send the message
					can_write_tx( 0x00, &buffer[0] );
					can_rts( 0 );
					buf_ident[0] = can.identifier;
					buf_ident_ext[0] = can.identifier_ext;
				}
				// Is it mailbox 1?
				else if(( can_read_status() & 0x10 ) == 0x00) {
					// Setup mailbox 1 and send the message
					can_write_tx( 0x02, &buffer[0] );
					can_rts( 1 );
					buf_ident[1] = can.identifier;
					buf_ident_ext[1] = can.identifier_ext;
				}
				// Is it mailbox 2?
				else if(( can_read_status() & 0x40 ) == 0x00) {
					// Setup mailbox 2 and send the message
					can_write_tx( 0x04, &buffer[0] );
					can_rts( 2 );
					buf_ident[2] = can.identifier;
					buf_ident_ext[2] = can.identifier_ext;
				}
			}			
		}
		return(1);
//	}
}

/*
 * Abort all pending transmissions
 */
void can_abort_transmit( void )
{
	// Abort transmission of all messages
	can_mod( TXB0CTRL, 0x08, 0x00 );
	can_mod( TXB1CTRL, 0x08, 0x00 );
	can_mod( TXB2CTRL, 0x08, 0x00 );
}

/*
 * Put CAN controller in sleep mode
 * - Busy wait until part is actually asleep
 */
void can_sleep( void )
{
	unsigned char status = 0;
	
	// Switch to sleep mode
	can_mod( CANCTRL, 0xE0, 0x20 );			// CANCTRL register, modify upper 3 bits, mode = Sleep

	// Wait until actually in sleep mode
	while((status & 0xE0) != 0x20){
		// Read out the status register
		can_read( CANSTAT, &status, 1 );
	}
}

/*
 * Wake CAN controller from sleep
 * - Busy wait until in normal operation mode
 */
void can_wake( void )
{
	// Put part in normal mode
	can_mod( CANCTRL, 0xE0, 0x00 );			// CANCTRL register, modify upper 3 bits, mode = Normal
}



/**************************************************************************************************
 * PRIVATE FUNCTIONS
 *************************************************************************************************/

/*
 * Resets MCP2515 CAN controller via SPI port
 *	- SPI port must be already initialised
 */
void can_reset( void )
{
	can_select;
	usci_transmit( MCP_RESET );
	can_deselect;
}

/*
 * Reads data bytes from the MCP2515
 *	- Pass in starting address, pointer to array of bytes for return data, and number of bytes to read
 */
void can_read( unsigned char address, unsigned char *ptr, unsigned char bytes )
{
	unsigned char i;
	
	can_select;
	usci_transmit( MCP_READ );
	usci_transmit( address );
	for( i = 0; i < bytes; i++ ) *ptr++ = usci_exchange( 0x00 );
	can_deselect;
}

/*
 * Reads data bytes from receive buffers
 *	- Pass in buffer number and start position as defined in MCP2515 datasheet
 *		- For starting at data, returns 8 bytes
 *		- For starting at address, returns 13 bytes
 */
void can_read_rx( unsigned char address, unsigned char *ptr )
{
	unsigned char i;
	
	address &= 0x03;						// Force upper bits of address to be zero (they're invalid)
	address <<= 1;							// Shift input bits to correct location in command byte
	address |= MCP_READ_RX;					// Construct command byte for MCP2515
	
	can_select;
	usci_transmit( address );
	
	if(( address & 0x02 ) == 0x00 ){		// Start at address registers
		for( i = 0; i < 13; i++ ){
			*ptr++ = usci_exchange( 0x00 );
		}
	}
	else{									// Start at data registers
		for( i = 0; i < 8; i++ ){
			*ptr++ = usci_exchange( 0x00 );
		}
	}
	can_deselect;
}

/*
 * Writes data bytes to the MCP2515
 *	- Pass in starting address, pointer to array of bytes, and number of bytes to write
 */
void can_write( unsigned char address, unsigned char *ptr, unsigned char bytes )
{
	unsigned char i;
	
	can_select;
	usci_transmit( MCP_WRITE );
	usci_transmit( address );
	for( i = 0; i < (bytes-1); i++ ){
		usci_transmit( *ptr++ );
	}
	usci_transmit( *ptr );
	can_deselect;
}

/*
 * Writes data bytes to transmit buffers
 *	- Pass in buffer number and start position as defined in MCP2515 datasheet
 *		- For starting at data, accepts 8 bytes
 *		- For starting at address, accepts 13 bytes
 */
void can_write_tx( unsigned char address, unsigned char *ptr )
{
	unsigned char i;
	
	address &= 0x07;						// Force upper bits of address to be zero (they're invalid)
	address |= MCP_WRITE_TX;				// Construct command byte for MCP2515
	
	can_select;
	usci_transmit( address );
	
	if(( address & 0x01 ) == 0x00 ){		// Start at address registers
		for( i = 0; i < 13; i++ ){
			usci_transmit( *ptr++ );
		}
	}
	else{									// Start at data registers
		for( i = 0; i < 8; i++ ){
			usci_transmit( *ptr++ );
		}
	}
	can_deselect;
}

/*
 * Request to send selected transmit buffer
 *	- Pass in address of buffer to transmit: 0, 1 or 2
 */
void can_rts( unsigned char address )
{
	unsigned char i;
	
	// Set up address bits in command byte
	i = MCP_RTS;
	if( address == 0 ) i |= 0x01;
	else if( address == 1 ) i |= 0x02;
	else if( address == 2 ) i |= 0x04;
	
	// Write command
	can_select;
	usci_transmit( i );
	can_deselect;
}

/*
 * Reads MCP2515 status register
 */
unsigned char can_read_status( void )
{
	unsigned char status;
	
	can_select;
	usci_transmit( MCP_STATUS );
	status = usci_exchange( 0x00 );
	can_deselect;
	return status;
}

/*
 * Reads MCP2515 RX status (filter match) register
 */
unsigned char can_read_filter( void )
{
	unsigned char status;
	
	can_select;
	usci_transmit( MCP_FILTER );
	status = usci_exchange( 0x00 );
	can_deselect;
	return status;
}

/*
 * Modifies selected register in MCP2515
 *	- Pass in register to be modified, bit mask, and bit data
 */
void can_mod( unsigned char address, unsigned char mask, unsigned char data )
{
	can_select;
	usci_transmit( MCP_MODIFY );
	usci_transmit( address );
	usci_transmit( mask );
	usci_transmit( data );
	can_deselect;
}