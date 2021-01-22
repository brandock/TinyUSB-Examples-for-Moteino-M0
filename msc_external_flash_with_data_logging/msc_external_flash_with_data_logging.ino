/* Brandon Baldock: This sketch for Moteino M0 combines the SDFat_datalogging and msc_external_flash
 * examples from the Adafruit TinyUSB Library. 
 * 
 * The sketch defines the Winbond SPIFlash that is standard on
 * the Moteino, defines EXTERNAL_FLASH_USE chip selector for the Moteino, and sets the radio pin
 * HIGH to prevent it from inteferring.
 * 
 * Beyond that the modification is to combine the behavior of the two sketches so that the data 
 * is logged and can be retreived via USB drive when plugged into a PC.
 * 
 * The sketch finds the next unused file in the sequence data1.csv, data2.csv, and logs to this file.
 * 
 * It is designed to be compiled with the USBTiny Stack for the Moteino M0 board. 
 *
 * Before running this sketch use the SdFat_format sketch to format your SPI flash as a FAT USB drive.
 */

/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This example demo how to expose on-board external Flash as USB Mass Storage.
 * Following library is required
 *   - Adafruit_SPIFlash https://github.com/adafruit/Adafruit_SPIFlash
 *   - SdFat https://github.com/adafruit/SdFat
 *
 * Note: Adafruit fork of SdFat enabled ENABLE_EXTENDED_TRANSFER_CLASS and FAT12_SUPPORT
 * in SdFatConfig.h, which is needed to run SdFat on external flash. You can use original
 * SdFat library and manually change those macros
 */

#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"

#define WINBOND4MBIT                                                           \
  {                                                                            \
    .total_size = (1UL << 22), /* 4 MiB */                                     \
    .start_up_time_us = 10000, .manufacturer_id = 0xef,                        \
    .memory_type = 0x30, .capacity = 0x13, .max_clock_speed_mhz = 104,         \
    .quad_enable_bit_mask = 0x02, .has_sector_protection = false,              \
    .supports_fast_read = true, .supports_qspi = false,                        \
    .supports_qspi_writes = false, .write_status_register_split = false,       \
    .single_status_byte = true, .is_fram = false,                              \
  }
static const SPIFlash_Device_t myFlashes[] = { WINBOND4MBIT };

#define EXTERNAL_FLASH_USE_SPI SPI
#define EXTERNAL_FLASH_USE_CS 8

// Configuration for the base datalogging filename. i.e. "data" will result in "data1.csv, data2.csv, ..." incremented with each reset of the MCU 
#define FILE_NAME      "data"

// Uncomment to run example with FRAM
// #define FRAM_CS   A5
// #define FRAM_SPI  SPI

#if defined(FRAM_CS) && defined(FRAM_SPI)
  Adafruit_FlashTransport_SPI flashTransport(FRAM_CS, FRAM_SPI);

#else
  // On-board external flash (QSPI or SPI) macros should already
  // defined in your board variant if supported
  // - EXTERNAL_FLASH_USE_QSPI
  // - EXTERNAL_FLASH_USE_CS/EXTERNAL_FLASH_USE_SPI
  #if defined(EXTERNAL_FLASH_USE_QSPI)
    Adafruit_FlashTransport_QSPI flashTransport;

  #elif defined(EXTERNAL_FLASH_USE_SPI)
    Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);

  #else
    #error No QSPI/SPI flash are defined on your board variant.h !
  #endif
#endif

Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatFileSystem fatfs;

FatFile root;
FatFile file;

// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

String filename;

// the setup function runs once when you press reset or power the board
void setup()
{
  //ensure the radio module CS pin is pulled HIGH or it might interfere!
  pinMode(A2, OUTPUT);
  digitalWrite(A2, HIGH);
  
  pinMode(LED_BUILTIN, OUTPUT);

  flash.begin(myFlashes);

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "External Flash", "1.0");

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Set disk size, block size should be 512 regardless of spi flash page size
  usb_msc.setCapacity(flash.size()/512, 512);

  // MSC is ready for read/write
  usb_msc.setUnitReady(true);
  
  usb_msc.begin();

  // Init file system on the flash
  fatfs.begin(&flash);

  Serial.begin(115200);
  delay(500);   // wait for native usb

  Serial.println("Adafruit TinyUSB Mass Storage and Datalogging to External Flash example");
  Serial.print("JEDEC ID: "); Serial.println(flash.getJEDECID(), HEX);
  Serial.print("Flash size: "); Serial.println(flash.size());

  //Increment to the next filename for logging. i.e. if data4.csv exists, increment to data5.csv
  int i = 0;
  filename = String(FILE_NAME) + String(i) + String(".csv");
  File dataFile = fatfs.open(filename, FILE_READ);
  delay(10);  // give enough time to return true if the file exists
  while (dataFile) {
    dataFile.close();
    i++;
    filename = String(FILE_NAME) + String(i) + String(".csv");
    dataFile = fatfs.open(filename, FILE_READ);
    delay(10);  // give enough time to return true if the file exists
  }
  dataFile.close();
  Serial.print("Next filename is: ");
  Serial.println(filename);
  blink();
  blink();
  blink();
}

void loop()
{
  long timestamp = millis();  
  if (timestamp % 4 == 0)  {
    // Open the datalogging file for writing.  The FILE_WRITE mode will open
    // the file for appending, i.e. it will add new data to the end of the file.
    File dataFile = fatfs.open(filename, FILE_WRITE);
    // Check that the file opened successfully and write a line to it.
    if (dataFile) {
      // Take a new data reading from a sensor, etc.  
      //int reading = random(0,100);  //or use a random number
      int reading = timestamp % 100;
      // Write a line to the file.  You can use all the same print functions
      // as if you're writing to the serial monitor.  For example to write
      // two CSV (commas separated) values:
      dataFile.print(reading);
      dataFile.print(",");
      dataFile.print(timestamp);
      dataFile.println();
      // Finally close the file when done writing.  This is smart to do to make
      // sure all the data is written to the file.
      dataFile.close();
      delay(10);
      Serial.print("Wrote new measurement: ");
      Serial.print(reading);
      Serial.print("   timestamp: ");
      Serial.print(timestamp);
      Serial.print("   to datafile: ");
      Serial.println(filename);
      blink();
    }
    else {
      Serial.println("Failed to open data file for writing!");
    }
  }
  delay(1001); // refresh every 0.5 second
}

void logData (char c) {

}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and 
// return number of copied bytes (must be multiple of block size) 
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  digitalWrite(LED_BUILTIN, HIGH);

  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void)
{
  // sync with flash
  flash.syncBlocks();

  // clear file system's cache to force refresh
  fatfs.cacheClear();

  digitalWrite(LED_BUILTIN, LOW);
}

void blink() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
}
