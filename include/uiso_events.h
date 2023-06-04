/*
 * uiso_events.h
 *
 *  Created on: 4 jun 2023
 *      Author: Francisco
 */

#ifndef UISO_EVENTS_H_
#define UISO_EVENTS_H_

// Events
enum uiso_events_e {
	/* Group 1: Buttons*/
	EVENT_BUTTON1_PRESS,
	EVENT_BUTTON1_RELEASE,

	EVENT_BUTTON2_PRESS,
	EVENT_BUTTON2_RELEASE,

	/* Group 2: SD Card*/
	EVENT_SD_CARD_INSERTED,
	EVENT_SD_CARD_REMOVED,

	/* Group 3: Wifi HW */

	/* Group 4: Sensors */

	/* Group 5: Wifi Connectivity */


	/* Group 6: */
};

void create_event_processor_task(void);


#endif /* UISO_EVENTS_H_ */
