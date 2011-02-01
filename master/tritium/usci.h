/*
 * Tritium MSP430 2xx USCI SPI interface header file
 * Copyright (c) 2009, Tritium Pty Ltd.  All rights reserved.
 *
 * Last Modified: J.Kennedy, Tritium Pty Ltd, 30 September 2009
 * Then by Mike Van Emmerik for dual serial ports
 *
 */
 
// Public Function prototypes
extern 	void 			usci_init( unsigned char clock );
extern 	void			usci_transmit( unsigned char data );
extern 	unsigned char 	usci_exchange( unsigned char data );

extern	void			chgr_transmit(const unsigned char* ptr);
extern	void			chgr_transmit_buf( void );
extern	void			bmu_transmit(const unsigned char* ptr);
extern	void			bmu_transmit_buf( void );

// Private Function prototypes

// Public variables
volatile unsigned int chgr_sent_timeout;

