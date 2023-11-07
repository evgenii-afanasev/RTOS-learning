/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Auto-generated by STM32CubeIDE
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include <stdint.h>
#include <stdio.h>

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
#warning \
    "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

#include "lib.h"

static const uint32_t amount_of_tasks = 5;
uint8_t current_task = 1;
uint32_t global_tick_count = 0;

void task1_handler(void);
void task2_handler(void);
void task3_handler(void);
void task4_handler(void);

sTCB tasks[5];

__attribute__((naked)) void init_scheduler_stack(
    uint32_t scheduler_stack_start);

void enable_processor_faults(void);
void init_systick_timer(uint32_t ticks);
void switch_to_psp(void);
void save_psp_value(uint32_t addr);
void switch_to_the_next_task(void);
void init_tasks_stack(void);
void update_global_tick_count(void);

void HardFault_Handler(void) {
  printf("hard fault");
  while (1)
    ;
}

void idle_task() {
  while (1)
    ;
}

int main(void) {
  RCC_AHBEN_R |= IOPEEN;

  GPIOE_MODE_R |= (1U << 18);
  GPIOE_MODE_R &= ~(1U << 19);

  GPIOE_MODE_R |= (1U << 22);
  GPIOE_MODE_R &= ~(1U << 23);

  GPIOE_MODE_R |= (1U << 26);
  GPIOE_MODE_R &= ~(1U << 27);

  GPIOE_MODE_R |= (1U << 30);
  GPIOE_MODE_R &= ~(1U << 31);

  enable_processor_faults();

  init_scheduler_stack(SCHEDULER_STACK_START);

  init_tasks_stack();

  init_systick_timer(TICK_PER_SECOND);

  switch_to_psp();

  task1_handler();

  for (;;)
    ;
}

void update_global_tick_count(void) { ++global_tick_count; }

uint32_t get_psp_value(void) { return tasks[current_task].psp; }

void switch_to_the_next_task(void) {
  for (int i = 1; i < amount_of_tasks; ++i) {
    if (tasks[i].state == READY) {
      current_task = i;
      return;
    }
  }

  current_task = 0;  // Switching to the idle task
}

__attribute__((naked)) void switch_to_psp(void) {
  // Initialize the PSP with the Task1 stack stack addr
  __asm volatile("PUSH {LR}");  // preserve the value of the LR, it will be
                                // changed by the next insturction
  __asm volatile("BL get_psp_value");
  __asm volatile("MSR PSP,R0");  // Init PSP
  __asm volatile("POP {LR}");

  // Change SP to PSP using CONTROL register
  __asm volatile("MOV R0,#0x02");
  __asm volatile("MSR CONTROL,R0");
  __asm volatile("BX LR");  // Return to main
}

void schedule(void) {
  uint32_t *pICSR = (uint32_t *)0xE000ED04;
  *pICSR |= (1 << 28);
}

void delay_task(uint32_t tick_count) {
  INTERRUPT_DISABLE();  // Disabling any interrupt to prevent race conditions on writing in or reading from the tasks array

  if (current_task == 0) return;

  tasks[current_task].block_count = global_tick_count + tick_count;
  tasks[current_task].state = BLOCKED;
  schedule();

  INTERRUPT_ENABLE();
}

void task1_handler(void) {
  while (1) {
    GPIOE_OD_R ^= TASK1_LED;
    delay_task(1000);
  }
}

void task2_handler(void) {
  while (1) {
    GPIOE_OD_R ^= TASK2_LED;
    delay_task(500);
  }
}

void task3_handler(void) {
  while (1) {
    GPIOE_OD_R ^= TASK3_LED;
    delay_task(250);
  }
}

void task4_handler(void) {
  while (1) {
    GPIOE_OD_R ^= TASK4_LED;
    delay_task(125);
  }
}

void update_tasks_state(void) {
  for (int i = 1; i < amount_of_tasks; ++i) {
    if (tasks[i].state == BLOCKED) {
      if (tasks[i].block_count == global_tick_count) tasks[i].state = READY;
    }
  }
}

void SysTick_Handler(void) {
  uint32_t *pICSR = (uint32_t *)0xE000ED04;

  update_global_tick_count();
  update_tasks_state();

  // Pend the
  *pICSR |= (1 << 28);
}

__attribute__((naked)) void PendSV_Handler(void) {
  /* Save the context of the currently running task */

  // The processor automatically saved R0-R3, xPSR, SP, LR, and PC registers

  // 1. Get the value of the Task's PSP
  __asm volatile("MRS R0,PSP");

  // 2. We need to save the rest of the registers (R4-R11) using PSP
  // The processor automatically saved R0-R3, xPSR, SP, LR, and PC registers
  __asm volatile("STMDB R0!,{R4-R11}");
  // Prefix '!' singals that the value of the R0 will be saved to the register
  // after the decrement of the address

  __asm volatile("PUSH {LR}");
  // 3. Save the updated value of the PSP
  __asm volatile("BL save_psp_value");

  /* Retrieve the context of the next task */

  // 1. Switch to the next task
  __asm volatile("BL switch_to_the_next_task");

  // 2. Get the task's previous PSP value
  __asm volatile("BL get_psp_value");

  // 3. Restore the R4-R11 registers stare
  __asm volatile("LDMIA R0!,{R4-R11}");

  // 4. Update the PSP and exit
  __asm volatile("MSR PSP,R0");

  __asm volatile("POP {LR}");
  __asm volatile("BX LR");
}
// After the function exit, the processor will automatically retrieve (POP)
// the remaining registers using the PSP. (refer to the "Exception exit sequence
// of the processor")

void save_psp_value(uint32_t addr) { tasks[current_task].psp = addr; }

__attribute__((naked)) void init_scheduler_stack(
    uint32_t scheduler_stack_start) {
  __asm volatile("MSR MSP,R0");
  __asm volatile("BX LR");
}

void init_tasks_stack(void) {
  tasks[0].state = READY;
  tasks[1].state = READY;
  tasks[2].state = READY;
  tasks[3].state = READY;
  tasks[4].state = READY;

  tasks[0].psp = IDLE_STACK_START;
  tasks[1].psp = T1_STACK_START;
  tasks[2].psp = T2_STACK_START;
  tasks[3].psp = T3_STACK_START;
  tasks[4].psp = T4_STACK_START;

  tasks[0].task_handler = idle_task;
  tasks[1].task_handler = task1_handler;
  tasks[2].task_handler = task2_handler;
  tasks[3].task_handler = task3_handler;
  tasks[4].task_handler = task4_handler;

  uint32_t *pPSP;

  for (int i = 0; i < amount_of_tasks; i++) {
    pPSP = (uint32_t *)tasks[i].psp;

    --pPSP;
    *pPSP = DUMMY_XPSR;  // Saving dummy xPSR

    // PC
    --pPSP;
    *pPSP = (uint32_t)tasks[i].task_handler;

    // LR
    --pPSP;
    *pPSP = 0xFFFFFFFD;

    // General purpose registers
    for (int j = 0; j < 13; ++j) {
      --pPSP;
      *pPSP = 0;
    }

    tasks[i].psp = (uint32_t)pPSP;
  }
}

void enable_processor_faults(void) {
  uint32_t *pSHCSR = (uint32_t *)0xE000ED24;

  *pSHCSR |= (1 << 16);  // Enable mem fault
  *pSHCSR |= (1 << 17);  // Enable bus fault
  *pSHCSR |= (1 << 18);  // Enable usage fault
}

void init_systick_timer(uint32_t ticks) {
  uint32_t *pSRVR = (uint32_t *)0xE000E014;
  uint32_t *pSCSR = (uint32_t *)0xE000E010;
  uint32_t count_value = SYSTICK_TIMER_CLK / ticks - 1;

  // Clear the value of the RVR
  *pSRVR &= ~(0x00FFFFFF);

  // Load the required RELOAD value into the RVR
  *pSRVR |= count_value;

  // Set up the SysTick Control and Status register
  *pSCSR |= (1 << 1);  // Enable SysTick exception request
  *pSCSR |= (1 << 2);  // Indicates the clock source (internal one)
  *pSCSR |= (1 << 0);  // Enables the counter
}