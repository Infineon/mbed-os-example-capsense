/*******************************************************************************
* File Name: main.cpp
*
* Version: 1.1
*
* Description:
*   This example demonstrates implementing CapSense buttons and slider with Mbed
*   OS. This example implements two button widgets and a slider widget, turns an
*   LED ON when any of the widgets is touched and OFF when none of them are 
*   touched, and prints the button status and slider position over serial port. 
*   This example enables CapSense Tuner communication over I2C for CapSense data
*   monitoring. This example uses the following RTOS objects and runs a thread 
*   for periodic CapSense scan apart from the main() thread. 
*
*   Semaphore:  The scan loop calls wait() to obtain the Semaphore after 
*               initiating the scan. The Semaphore is released in the end of 
*               scan callback after which the scan loop processes the touch
*               information. 
*   EventQueue: The dispatcher of the EventQueue is run inside a thread to
*               periodically scan the sensors. 
*
* Related Document: README.md
*
* Supported Kits (Target Names): 
*   CY8CKIT-062-BLE PSoC 6 BLE Pioneer Kit (CY8CKIT_062_BLE)
*   CY8CKIT-062-WiFi-BT PSoC 6 WiFi-BT Pioneer Kit (CY8CKIT_062_WIFI_BT)
*   CY8CPROTO-062-4343W PSoC 6 Wi-Fi BT Prototyping Kit (CY8CPROTO_062_4343W)
*
*******************************************************************************
* Copyright (2019), Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* (“Software”), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries (“Cypress”) and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software (“EULA”).
*
* If no EULA applies, Cypress hereby grants you a personal, nonexclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress’s integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death (“High Risk Product”). By
* including Cypress’s product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/


/*******************************************************************************
* Header files including
********************************************************************************/
#include "mbed.h"
#include "cy_pdl.h"
#include "cycfg_capsense.h"
#include "cycfg.h"


/***************************************************************************
* Global constants
***************************************************************************/
#define SLIDER_NUM_TOUCH                        (1u)    /* Number of touches on the slider */
#define LED_OFF                                 (1u) 
#define LED_ON                                  (0u)
#define CAPSENSE_SCAN_PERIOD                    (20u)   /* milliseconds */


/***************************************
* Function Prototypes
**************************************/
void RunCapSenseScan(void);
void InitTunerCommunication(void);
void ProcessTouchStatus(void);
void EZI2C_InterruptHandler(void);
void CapSense_InterruptHandler(void);
void CapSenseEndOfScanCallback(cy_stc_active_scan_sns_t * ptrActiveScan);
void InitCapSenseClock(void);


/*******************************************************************************
* Interrupt configuration
*******************************************************************************/
const cy_stc_sysint_t CapSense_ISR_cfg =
{
    .intrSrc = CYBSP_CSD_IRQ,
    .intrPriority = 4u
};

const cy_stc_sysint_t EZI2C_ISR_cfg = {
    .intrSrc = CYBSP_CSD_COMM_IRQ,
    .intrPriority = 3u
};


/*******************************************************************************
* Global variables
*******************************************************************************/
DigitalOut ledRed(LED_RED);
Semaphore capsense_sem;
EventQueue queue;
cy_stc_scb_ezi2c_context_t EZI2C_context;
uint32_t prevBtn0Status = 0u; 
uint32_t prevBtn1Status = 0u;
uint32_t prevSliderPos = 0u;


/*****************************************************************************
* Function Name: main()
******************************************************************************
* Summary:
*   Main function that starts a thread for CapSense scan and enters a forever
*   wait state. 
*
*****************************************************************************/
int main(void)
{
    /* Configure AMUX bus for CapSense */
    init_cycfg_routing();
    
    /* Configure PERI clocks for CapSense */
    InitCapSenseClock();    
    InitTunerCommunication();
    
    /* Initialize the CSD HW block to the default state. */
    cy_status status = Cy_CapSense_Init(&cy_capsense_context);
    if(CY_RET_SUCCESS != status)
    {
        printf("CapSense initialization failed. Status code: %lu\r\n", status);
        wait(osWaitForever);
    }
    
    /* Initialize CapSense interrupt */
    Cy_SysInt_Init(&CapSense_ISR_cfg, &CapSense_InterruptHandler);
    NVIC_ClearPendingIRQ(CapSense_ISR_cfg.intrSrc);
    NVIC_EnableIRQ(CapSense_ISR_cfg.intrSrc);

    /* Initialize the CapSense firmware modules. */
    Cy_CapSense_Enable(&cy_capsense_context);
    Cy_CapSense_RegisterCallback(CY_CAPSENSE_END_OF_SCAN_E, CapSenseEndOfScanCallback, &cy_capsense_context);
    
    /* Create a thread to run CapSense scan periodically using an event queue
     * dispatcher.
     */
    Thread thread(osPriorityNormal, OS_STACK_SIZE, NULL, "CapSense Scan Thread");
    thread.start(callback(&queue, &EventQueue::dispatch_forever));
    queue.call_every(CAPSENSE_SCAN_PERIOD, RunCapSenseScan);

    /* Initiate scan immediately since the first call of RunCapSenseScan()
     * happens CAPSENSE_SCAN_PERIOD after the event queue dispatcher has
     * started. 
     */
    Cy_CapSense_ScanAllWidgets(&cy_capsense_context); 
    
    printf("Application has started. Touch any CapSense button or slider.\r\n");
    wait(osWaitForever);
    
}


/*****************************************************************************
* Function Name: RunCapSenseScan()
******************************************************************************
* Summary:
*   This function starts the scan, and processes the touch status. It is
* periodically called by an event dispatcher. 
*
*****************************************************************************/
void RunCapSenseScan(void)
{
    Cy_CapSense_ScanAllWidgets(&cy_capsense_context);
    capsense_sem.acquire();          
    Cy_CapSense_ProcessAllWidgets(&cy_capsense_context);
    Cy_CapSense_RunTuner(&cy_capsense_context);
    ProcessTouchStatus();     
}


/*******************************************************************************
* Function Name: InitTunerCommunication
********************************************************************************
*
* Summary:
*   This function performs the following functions:
*       - Initializes SCB block for operation in EZI2C mode.
*       - Connects EZI2C HW to the SDA and SCL pins.
*       - Sets communication data buffer to CapSense data structure.
*
*******************************************************************************/
void InitTunerCommunication(void)
{
    Cy_SCB_EZI2C_Init(CYBSP_CSD_COMM_HW, &CYBSP_CSD_COMM_config, &EZI2C_context);

    /* Initialize and enable EZI2C interrupts */
    Cy_SysInt_Init(&EZI2C_ISR_cfg, &EZI2C_InterruptHandler);
    NVIC_EnableIRQ(EZI2C_ISR_cfg.intrSrc);

    /* Set up communication data buffer to CapSense data structure to be exposed
     * to I2C master at primary slave address request.
     */
    Cy_SCB_EZI2C_SetBuffer1(CYBSP_CSD_COMM_HW, (uint8 *)&cy_capsense_tuner,
        sizeof(cy_capsense_tuner), sizeof(cy_capsense_tuner), &EZI2C_context);

    /* Enable EZI2C block */
    Cy_SCB_EZI2C_Enable(CYBSP_CSD_COMM_HW);
}


/*******************************************************************************
* Function Name: ProcessTouchStatus
********************************************************************************
*
* Summary:
*   Controls the LED status according to the status of CapSense widgets and
*   prints the status to serial terminal.
*
*******************************************************************************/
void ProcessTouchStatus(void)
{
    uint32_t currSliderPos;    
    uint32_t currBtn0Status = Cy_CapSense_IsSensorActive(CY_CAPSENSE_BUTTON0_WDGT_ID, CY_CAPSENSE_BUTTON0_SNS0_ID, &cy_capsense_context);        
    uint32_t currBtn1Status = Cy_CapSense_IsSensorActive(CY_CAPSENSE_BUTTON1_WDGT_ID, CY_CAPSENSE_BUTTON1_SNS0_ID, &cy_capsense_context);       
    cy_stc_capsense_touch_t *sldrTouch = Cy_CapSense_GetTouchInfo(CY_CAPSENSE_LINEARSLIDER0_WDGT_ID, &cy_capsense_context);

    if(currBtn0Status != prevBtn0Status)
    {
        printf("Button_0 status: %lu\r\n", currBtn0Status);
        prevBtn0Status = currBtn0Status;
    }
    
    if(currBtn1Status != prevBtn1Status)
    {
        printf("Button_1 status: %lu\r\n", currBtn1Status);
        prevBtn1Status = currBtn1Status;
    } 

    if (sldrTouch->numPosition == SLIDER_NUM_TOUCH)
    {       
        currSliderPos = sldrTouch->ptrPosition->x;

        if(currSliderPos != prevSliderPos)
        {
            printf("Slider position: %lu\r\n", currSliderPos);
            prevSliderPos = currSliderPos;
        }
    }

    ledRed = (currBtn0Status || currBtn1Status || (sldrTouch->numPosition == SLIDER_NUM_TOUCH)) ? LED_ON : LED_OFF;
}


/*******************************************************************************
* Function Name: EZI2C_InterruptHandler
********************************************************************************
* Summary:
*   Wrapper function for handling interrupts from EZI2C block. 
*
*******************************************************************************/
void EZI2C_InterruptHandler(void)
{
    Cy_SCB_EZI2C_Interrupt(CYBSP_CSD_COMM_HW, &EZI2C_context);
}

/*****************************************************************************
* Function Name: CapSense_InterruptHandler()
******************************************************************************
* Summary:
*  Wrapper function for handling interrupts from CSD block.
*
*****************************************************************************/
void CapSense_InterruptHandler(void)
{
    Cy_CapSense_InterruptHandler(CYBSP_CSD_HW, &cy_capsense_context);
}


/*****************************************************************************
* Function Name: CapSenseEndOfScanCallback()
******************************************************************************
* Summary:
*  This function releases a semaphore to indicate end of a CapSense scan.
*
* Parameters:
*  cy_stc_active_scan_sns_t* : pointer to active sensor details.
*
*****************************************************************************/
void CapSenseEndOfScanCallback(cy_stc_active_scan_sns_t * ptrActiveScan)
{
    capsense_sem.release();
}


/*****************************************************************************
* Function Name: InitCapSenseClock()
******************************************************************************
* Summary:
*  This function configures the peripheral clock for CapSense.  
*
*****************************************************************************/
void InitCapSenseClock(void)
{
    Cy_SysClk_PeriphAssignDivider(PCLK_CSD_CLOCK, CYBSP_CSD_CLK_DIV_HW, CYBSP_CSD_CLK_DIV_NUM);
    Cy_SysClk_PeriphDisableDivider(CYBSP_CSD_CLK_DIV_HW, CYBSP_CSD_CLK_DIV_NUM);
    Cy_SysClk_PeriphSetDivider(CYBSP_CSD_CLK_DIV_HW, CYBSP_CSD_CLK_DIV_NUM, 0u);
    Cy_SysClk_PeriphEnableDivider(CYBSP_CSD_CLK_DIV_HW, CYBSP_CSD_CLK_DIV_NUM);
}


/* [] END OF FILE */
