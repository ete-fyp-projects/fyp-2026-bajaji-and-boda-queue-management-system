# Smart Boda Boda and Bajaji Queue Management System

## Student Information
* **Student Name:** MTEPA, SHIRU ISMAIL
* **Registration Number:** 2022-04-08722

## Problem Statement
The absence of a proper queuing system at many bajaji and boda boda stations leads to unfair service order and frequent conflicts among riders competing for customers. This disorganization causes delays in customer service, animosity among riders, potential physical conflicts, and overall negatively impacts service quality and public safety.

## Proposed Solution
This project implements a hardware-based Smart Queue Management System that automatically organizes rider queues using RFID technology, GSM communication, and display units. Registered riders are added to a queue upon scanning their RFID card at the station entrance. The system uses a GSM module to notify riders when it is their turn, displays the currently serving rider on an LED screen, and automatically removes the rider from the queue when they scan out at the exit. 

## Methodology
1. **Requirements Gathering:** Established functional requirements (RFID detection, GSM SMS notifications, CRUD operations via SMS) and non-functional requirements (offline reliability).
2. **System Design & Implementation:** Assembled the hardware components, integrating two RFID readers, an Arduino Mega 2560 microcontroller, a SIM800C GSM module, an LED Display, and an EEPROM module. Programmed the system using the Arduino IDE.
3. **System Testing:** Conducted tests to verify the correct detection of registered riders at entry and exit points, proper queue generation, accurate LCD updates, and timely SMS notifications.
4. **Evaluation:** Confirmed the reliable non-volatile storage of rider information and overall system stability.

## Key Findings
* **RFID Detection:** The RFID readers successfully distinguished between authorized and unauthorized users, accurately adding riders to the queue at the entrance and removing them at the exit.
* **Real-Time Display:** The LCD successfully displayed real-time queue information, instantly updating the currently serving rider and the total number of waiting riders.
* **Data Storage:** The external EEPROM reliably stored rider records (name, phone number, and plate number) permanently, keeping data intact even after power cycles.
* **GSM Communication:** The SIM800C GSM module successfully transmitted SMS notifications to riders when their turn arrived and correctly processed admin SMS commands to add, remove, find, or check the status of riders.

## Conclusion
The hardware implementation demonstrated that this automated system provides a practical, fair, and efficient offline solution for managing boda boda and bajaji queues. By minimizing human intervention, the prototype successfully achieves reliable rider identification, queue organization, and real-time notification, making it highly suitable for deployment in urban transport stations.

## Recommendations
* Develop a centralized database to allow multiple stations to share rider information and synchronize queues.
* Integrate a web-based platform or mobile application for real-time online monitoring alongside SMS.
* Incorporate GPS technology to verify that a rider is physically within the station before adding them to the queue.
* Add a backup power system (such as solar or rechargeable batteries) to ensure continuous operation during power outages.
* Integrate digital payment systems like mobile money to support cashless transactions.
* Improve the user interface by utilizing larger LED information boards for better visibility in crowded stations.

## Repository Contents
* `documents/` — Final report and presentation
* `source-code/` — Software or firmware source code
* `hardware-files/` — Simulation, PCB, schematic, or CAD files
* `images/` — Project diagrams, prototype photographs, and results
