# CS-350-EmergingSystems
Emerging Systems Architecture and Technologies

## Summarize the project and what problem it was solving.
The repository includes two projects. One is the thermostat project (gpiointerrupt_CC3220S_LAUNCHXL_nortos_ccs), which consists of functionality to implement a thermostat. It allows the user to increase or decrease the desired temperature, checks the current temperature periodically and compares it to the set temperature, simulates turning a heater on/off through an LED, and sends out a report on the current status of the system every second.

The second project is the morse code project. It sends out morse code signals via LEDs to spell out "SOS" and "OK". The user can press a button to switch between the two messages.

## What did you do particularly well?
I think that my strong point in both projects was designing the state machines for each process and consistently implementing those state machines through a methodical process. While I encountered minor issues working with the hardware, once those were sorted out the projects ran smoothly with very little debugging of the actual processes themselves.

## Where could you improve?
I think that I need to improve on optimization practices. While the projects each worked successfully I'm sure that I didn't minimize memory usage and processing required as well as I could have done.

## What tools and/or resources are you adding to your support network?
I've had to improve my skills at reading the technical documentation for embedded systems. MCU documentation is often a bit cryptic with a lot of abbreviations and jargon. I'm now able to decipher it much better than I could at the beginning of the course.

## What skills from this project will be particularly transferable to other projects and/or course work?
The use of state machines for asynchronous processing is a tool that I can add to my toolbelt that I can definitely see coming in handy in the future - both the design and specific implementation methods for them.

## How did you make this project maintainable, readable, and adaptable?
There were several methods that I used to try to make these projects maintainable, readable, and adaptable. In addition to following best practices for commenting my code, I have long been in the habit of designing code in as straightforward a manner as possible. I tried to use clear variable and enum names for everything, but especially for the state machine states. The morse code project was also specifically designed to easily allow for additional letters and messages to be implemented with very little work.
