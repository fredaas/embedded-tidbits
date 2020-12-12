.syntax unified

.include "efm32gg.s"

/////////////////////////////////////////////////////////////////////////////
//
// Exception vector table
//
/////////////////////////////////////////////////////////////////////////////

.section .vectors

  .long   stack_top               /* Top of Stack                 */
  .long   _reset                  /* Reset Handler                */
  .long   dummy_handler           /* NMI Handler                  */
  .long   dummy_handler           /* Hard Fault Handler           */
  .long   dummy_handler           /* MPU Fault Handler            */
  .long   dummy_handler           /* Bus Fault Handler            */
  .long   dummy_handler           /* Usage Fault Handler          */
  .long   dummy_handler           /* Reserved                     */
  .long   dummy_handler           /* Reserved                     */
  .long   dummy_handler           /* Reserved                     */
  .long   dummy_handler           /* Reserved                     */
  .long   dummy_handler           /* SVCall Handler               */
  .long   dummy_handler           /* Debug Monitor Handler        */
  .long   dummy_handler           /* Reserved                     */
  .long   dummy_handler           /* PendSV Handler               */
  .long   dummy_handler           /* SysTick Handler              */

  /* External Interrupts */
  .long   dummy_handler
  .long   gpio_handler            /* GPIO even handler */
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   gpio_handler            /* GPIO odd handler */
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler
  .long   dummy_handler

.section .text

/////////////////////////////////////////////////////////////////////////////
//
// Reset handler
//
/////////////////////////////////////////////////////////////////////////////

  .globl  _reset
  .type   _reset, %function

cmu_base_addr: .long CMU_BASE
gpio_base:     .long GPIO_BASE
gpio_pa_base:  .long GPIO_PA_BASE
gpio_pc_base:  .long GPIO_PC_BASE
iser0_addr:    .long ISER0
scr_addr:      .long SCR

r_leds_base_addr    .req r1
r_gamepad_base_addr .req r2

.thumb_func
_reset:
    b enable_gpio_clock

enable_gpio_clock:
    // Load CMU base address
    ldr r1, cmu_base_addr

    // Load clock base address
    ldr r2, [r1, #CMU_HFPERCLKEN0]

    // Set bit for GPIO clock
    mov r3, #1
    lsl r3, r3, #CMU_HFPERCLKEN0_GPIO
    orr r2, r2, r3

    // Store new value
    str r2, [r1, #CMU_HFPERCLKEN0]

gpio_init_leds:
    // Load base address
    ldr r_leds_base_addr, gpio_pa_base

    // Set drive mode to 20 mA (mode 2, high)
    mov r3, #0x2
    str r3, [r_leds_base_addr, #GPIO_CTRL]

    // Set mode of LEDs to push-pull output with drive-strength set by DRIVEMODE
    mov r3, #0x55555555
    str r3, [r_leds_base_addr, #GPIO_MODEH]

    // Set initial values
    mov r3, #0xFFFFFFFF
    str r3, [r_leds_base_addr, #GPIO_DOUT]

gpio_init_gamepad:
    // Load port C base address
    ldr r_gamepad_base_addr, gpio_pc_base

    // Set mode of gamepad to input enabled with filter. DOUT determines pull
    // direction.
    mov r3, #0x33333333
    str r3, [r_gamepad_base_addr, #GPIO_MODEL]

    // Set inital value of buttons
    mov r3, #0xFF
    str r3, [r_gamepad_base_addr, #GPIO_DOUT]

gpio_enable_interrupt:
    // Enable external interrupt on all pins of port C
    ldr r6, gpio_base
    mov r3, #0x22222222
    str r3, [r6, #GPIO_EXTIPSELL]

    // Enable low-high (EXTIRISE) and high-low (EXTIFALL) interrupts
    mov r3, #0xFF
    str r3, [r6, #GPIO_EXTIRISE]
    str r3, [r6, #GPIO_EXTIFALL]

    // Enable interrupt generation
    mov r3, #0xFF
    str r3, [r6, #GPIO_IEN]

    // Enable interrupt handling
    ldr r6, iser0_addr
    ldr r3, =#0x802
    str r3, [r6]

    // Branch to main where we wait for interrupt
    b main

/////////////////////////////////////////////////////////////////////////////
//
// GPIO interrupt handler
//
/////////////////////////////////////////////////////////////////////////////

.thumb_func
gpio_handler:
    // Clear interrupt flag
    ldr r6, gpio_base
    ldr r3, [r6, #GPIO_IF]
    str r3, [r6, #GPIO_IFC]

    // Read Button state
    ldr r3, [r_gamepad_base_addr, #GPIO_DIN]
    lsl r3, #8

    // Write button state to LEDs
    str r3, [r_leds_base_addr, #GPIO_DOUT]

    bx lr

main:
    // Enter deep sleep on return from ISR
    ldr r6, scr_addr
    mov r3, #6
    str R3, [R6]

    // Wait for interrupt
    wfi

/////////////////////////////////////////////////////////////////////////////

.thumb_func
dummy_handler:
    b . // Do nothing
