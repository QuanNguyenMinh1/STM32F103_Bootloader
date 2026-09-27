# STM32F103_Bootloader

A simple STM32 bootloader architecture that demonstrates how to place an application at a custom Flash address and transfer execution from the bootloader to the application.

The project consists of two firmware images:

* **Bootloader** — executes after reset and jumps to the application.
* **Application** — runs from a custom Flash address and relocates the interrupt vector table accordingly.

---

## Project Overview

```text
                 STM32 Reset
                      │
                      ▼
             ┌─────────────────────┐
             │      Bootloader     │
             │                     │
             │     System Init     │
             │     GPIO / UART     │
             │                     │
             │ JumpToApplication() │
             └────────┬────────────┘
                      │
                      │ Jump
                      ▼
             ┌─────────────────┐
             │   Application   │
             │                 │
             │ SCB->VTOR =     │
             │ APP_START_ADDR  │
             │                 │
             │ Main Program    │
             └─────────────────┘
```

The bootloader initializes the MCU and then calls `JumpToApplication()`. The application configures `SCB->VTOR` to point to its own vector table before enabling global interrupts.

---

## Features

* STM32 HAL-based firmware
* Separate Bootloader and Application projects
* Custom application Flash start address
* Application vector table relocation using `SCB->VTOR`
* Bootloader-to-application execution handoff
* USART3 configured at **115200 baud**
* GPIO PC13 used as a simple status/debug indicator
* Generated with STM32CubeMX / STM32 HAL structure

---

## Memory Architecture

The STM32 Flash memory is divided into three logical regions:

* **Bootloader**: `0x08000000 – 0x08003FFF` — 16 KB
* **Application Header**: `0x08004000 – 0x080043FF` — 1 KB
* **Application**: starts at `0x08004400` — 47 KB

The memory layout is defined in `flash_layout.h`:

```c
#define BL_START_ADDR        0x08000000  // 16KB
#define APP_HEADER_ADDR      0x08004000  // 1KB
#define APP_START_ADDR       0x08004400  // 47KB
```

### Flash Memory Map

```text
STM32 Flash
0x08000000
┌──────────────────────────────────────┐
│                                      │
│             BOOTLOADER               │
│                                      │
│             16 KB                    │
│                                      │
│        0x08000000                    │
│            ↓                         │
│        0x08003FFF                    │
├──────────────────────────────────────┤
│                                      │
│          APPLICATION HEADER          │
│                                      │
│             1 KB                     │
│                                      │
│        0x08004000                    │
│            ↓                         │
│        0x080043FF                    │
├──────────────────────────────────────┤
│                                      │
│             APPLICATION              │
│                                      │
│             47 KB                    │
│                                      │
│        0x08004400                    │
│            ↓                         │
│             ...                      │
│                                      │
└──────────────────────────────────────┘
```

The application therefore does **not** start at the default Flash base address (`0x08000000`). Its vector table is located at:

```c
APP_START_ADDR = 0x08004400
```

When the application starts, the vector table is relocated to this address:

```c
SCB->VTOR = APP_START_ADDR;
```

This allows the application to correctly use its own interrupt vector table after the bootloader transfers execution to it.

### Address Summary

| Region             | Start Address |  Size |
| ------------------ | ------------: | ----: |
| Bootloader         |  `0x08000000` | 16 KB |
| Application Header |  `0x08004000` |  1 KB |
| Application        |  `0x08004400` | 47 KB |

The application header is reserved between the bootloader and application regions and can be used to store application metadata such as firmware information, image size, version, CRC, or validity status.

---

# Bootloader

The bootloader initializes the MCU and peripherals before transferring execution to the application.

The main bootloader sequence is:

```c
HAL_Init();

SystemClock_Config();

MX_GPIO_Init();
MX_USART3_UART_Init();

HAL_Delay(3500);
HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
HAL_Delay(3500);

JumpToApplication();
```

The important operation is:

```c
JumpToApplication();
```

This function is implemented in `bl_jump.h` / its corresponding source file.

After the jump, the bootloader is no longer expected to execute its normal main loop.

The bootloader also configures USART3:

```text
Baud rate : 115200
Data      : 8 bits
Parity    : None
Stop bits : 1
Mode      : TX/RX
Flow Ctrl : None
```

---

# Application

The application is linked to and executed from the custom application Flash region.

During initialization, the vector table is relocated:

```c
SCB->VTOR = APP_START_ADDR;
__enable_irq();
```

This is important because the MCU must use the application's interrupt vector table rather than the bootloader's vector table.

The application then initializes its peripherals:

```c
MX_GPIO_Init();
MX_USART3_UART_Init();
```

The current demonstration application toggles PC13 every second:

```c
while (1)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(1000);
}
```

This provides a simple visual indication that the application has successfully started.

---

# Bootloader → Application Flow

The complete execution flow is:

```text
Reset
  │
  ▼
Bootloader
  │
  ├── HAL_Init()
  │
  ├── SystemClock_Config()
  │
  ├── GPIO initialization
  │
  ├── USART3 initialization
  │
  ├── Startup delay
  │
  ▼
JumpToApplication()
  │
  ▼
Application Reset Handler
  │
  ├── HAL_Init()
  │
  ├── SCB->VTOR = APP_START_ADDR
  │
  ├── __enable_irq()
  │
  ├── SystemClock_Config()
  │
  ├── GPIO initialization
  │
  └── USART3 initialization
       │
       ▼
    Application
       │
       └── PC13 toggles every 1 s
```

---

# Vector Table Relocation

Normally, an STM32 application expects its vector table at the beginning of Flash.

When a bootloader occupies this region, the application starts at another address.

Therefore, the application explicitly relocates the vector table:

```c
SCB->VTOR = APP_START_ADDR;
```

This ensures that interrupts generated while the application is running are resolved using the application's interrupt vector table.

The application also explicitly enables interrupts:

```c
__enable_irq();
```

This is performed after entering the application because the bootloader may have modified the global interrupt state before transferring execution.

---

# Clock Configuration

The two firmware images currently use different clock initialization configurations.

### Bootloader

The bootloader uses the internal HSI oscillator:

```c
RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
RCC_OscInitStruct.HSIState = RCC_HSI_ON;
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
```

### Application

The application uses HSE with PLL:

```c
RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
RCC_OscInitStruct.HSEState = RCC_HSE_ON;
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
```

The application therefore runs with a different clock configuration from the bootloader.

---

# GPIO Debug Indicator

PC13 is configured as a push-pull output:

```c
GPIO_InitStruct.Pin = GPIO_PIN_13;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
```

The pin is used as a simple execution indicator.

### Bootloader

PC13 is toggled during the bootloader startup sequence before the application jump.

### Application

PC13 toggles periodically:

```c
HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
HAL_Delay(1000);
```

If PC13 continues toggling at approximately 1-second intervals, the application has successfully taken control of the MCU.

---

# Project Structure

A typical project organization is:

```text
STM32F103_Bootloader/
│
├── F103_Bootloader/
│   ├── Core/
│   │   ├── Inc/
│   │   └── Src/
│   │
│   ├── Drivers/
│   └── ...
│
├── F103_Application/
│   ├── Core/
│   │   ├── Inc/
│   │   └── Src/
│   │
│   ├── Drivers/
│   ├── flash_layout.h
│   └── ...
│
└── README.md
```

The exact directory structure depends on the STM32CubeIDE project configuration.

---

# Important Configuration

The application start address must be consistent across:

1. Bootloader jump code
2. Application linker configuration
3. `flash_layout.h`
4. Vector table relocation

For example:

```c
#define APP_START_ADDR  ...
```

and:

```c
SCB->VTOR = APP_START_ADDR;
```

must correspond to the actual Flash address where the application is linked.

If these addresses do not match, the bootloader may jump to an incorrect location or the application may use an incorrect interrupt vector table.

---

# Build & Flash

Build the projects separately.

### 1. Build Bootloader

Generate the Bootloader `.elf` / `.hex` / `.bin` file.

Flash it at the beginning of the MCU Flash.

```text
Flash base
    │
    ▼
┌───────────────┐
│  Bootloader   │
└───────────────┘
```

### 2. Build Application

Configure the application linker script so that the application is linked to `APP_START_ADDR`.

Generate the application binary.

Flash it at the application start address.

```text
┌───────────────┐
│  Bootloader   │
├───────────────┤
│  Application  │ ← APP_START_ADDR
└───────────────┘
```

### 3. Reset the MCU

After reset:

```text
Bootloader
    ↓
startup delay
    ↓
JumpToApplication()
    ↓
Application
    ↓
PC13 toggles every 1 second
```

---

# Current Limitations

This project currently demonstrates the **bootloader-to-application handoff mechanism** rather than a complete firmware-update system.

The current bootloader does not yet implement features such as:

* Firmware download over UART
* Firmware image validation
* CRC/checksum verification
* Firmware version management
* Application validity checking
* Rollback/recovery
* Dual-bank firmware
* Watchdog-based recovery
* Communication protocol for firmware update

These can be added as subsequent development stages.

---

# Possible Future Development

```text
Current
   │
   ├── Bootloader
   ├── Custom application address
   ├── Vector table relocation
   └── Application jump
          │
          ▼
Next
   │
   ├── UART firmware update
   ├── Frame protocol
   ├── CRC validation
   ├── Application metadata
   ├── Version checking
   └── Watchdog recovery
          │
          ▼
Advanced
   │
   ├── Secure boot
   ├── Firmware authentication
   ├── Anti-rollback
   ├── OTA update
   └── A/B firmware slots
```

---

# Technologies

| Component        | Technology                 |
| ---------------- | -------------------------- |
| MCU Firmware     | C                          |
| Framework        | STM32 HAL                  |
| Configuration    | STM32CubeMX / STM32CubeIDE |
| Communication    | USART3                     |
| UART Baud Rate   | 115200                     |
| Bootloader       | Custom STM32 Bootloader    |
| Application      | Custom Flash Address       |
| Interrupt Vector | `SCB->VTOR`                |
| Debug GPIO       | PC13                       |

---

# Learning Objectives

This project is intended to demonstrate several important embedded-system concepts:

* STM32 Flash memory organization
* Bootloader architecture
* Custom linker/application memory regions
* MCU reset and startup sequence
* Interrupt vector table relocation
* Bootloader-to-application handoff
* STM32 HAL initialization
* UART peripheral configuration
* Firmware partitioning

---

# Status

**Current status:** Functional bootloader-to-application jump demonstration.

The application successfully uses a custom vector-table address and provides a GPIO-based indication of application execution.

---

## License

This project is provided for educational and experimental purposes.

STM32-generated source files may contain copyright and licensing information from STMicroelectronics. Refer to the corresponding source headers and project `LICENSE` file for applicable terms.
