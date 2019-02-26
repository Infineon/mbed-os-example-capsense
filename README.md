# Introduction
This example demonstrates implementing CapSense buttons and slider for PSoC 6 MCU with Mbed OS. Additional PSoC 6-related code examples are available in other repos. See all examples at [Code Examples for Modus Toolbox](https://github.com/cypresssemiconductorco/Code-Examples-for-ModusToolbox-Software).

# Instructions to run the CapSense code example

Plug in the kit, and connect a serial terminal with 9600-8N1 setting to view the button status and slider position.

1. Import the code example
 
        mbed import https://github.com/cypresssemiconductorco/mbed-os-example-capsense

2. Change working directory to the example folder
        
        cd mbed-os-example-capsense

3. Compile the example and Program
    
        mbed compile --target CY8CKIT_062_BLE --toolchain GCC_ARM  --flash --sterm

        This command compiles, programs the kit, and opens the Mbed serial port with 9600, 8N1 setting on the command prompt window. 

        For other targets:
        mbed compile -m CY8CKIT_062_WIFI_BT -t GCC_ARM -f --sterm
        mbed compile -m CY8CPROTO_062_4343W -t GCC_ARM -f --sterm

4. Following message is displayed on the serial terminal when the application starts running.

        Application has started. Touch any CapSense button or slider.

5. Touch the buttons or the slider to observe the red LED changing its state. 

6. You can also monitor the CapSense data using the CapSense Tuner application as explained below.

# How to monitor data using CapSense Tuner

1. Open ".../ModusToolbox_1.1/tools/capsense-configurator-1.1/capsense-tuner" to run the CapSense Tuner application. 

2. Click File -> Open and open cycfg_capsense.h file which is available in this example directory. 

3. Switch from DAPLink mode to KitProg mode by pressing the MODE button or CUSTOM APP button depending on the kit. Refer to the kit user guide for more information. 

4. In the Tuner application, click the settings icon or click Tools -> Tuner Communication Setup. In the window that appears, select I2C under KitProg and configure as follows. 

        I2C Address: 8
        Sub-address: 2-Bytes
        Speed (kHz): 100

5. Click the Connect button or Communiction -> Connect.

6. Click the Start button or Communication -> Start.

7. Under Widget View tab on the right side, you can see the corresponding widgets turning blue when you touch the button or slider. You can also view the sensor data in Graph View tab. e.g. To view sensor data for Button 0, you need to select Button0_Rx0 under Button0. 

See the ModusToolbox CapSense Tuner Guide (Help -> View Help) for more information. 

# How to modify the CapSense settings for this project

In this process, you cannot modify the pins or add/delete the CapSense widgets. You can only modify the CapSense settings such as IDAC, clock, threshold etc. 

1. Open ".../ModusToolbox_1.1/tools/capsense-configurator-1.1/capsense-configurator" to run the CapSense Configurator application. 

2. Click File -> Open and open cycfg_capsense.h file which is available in this example directory.

3. When you save the changes, the tool updates cycfg_capsense.h and cfg_capsense.c files in this example project. 
